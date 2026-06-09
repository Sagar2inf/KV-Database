#include "sstable.hpp"
#include "../config.hpp"
#include "../platform.hpp"
#include <fcntl.h>
#include <sys/stat.h>
#include <chrono>
#include <algorithm>
#include <iostream>

static constexpr uint8_t MAGIC[8] = {'S','S','T','R','1','.','0','\0'};
static constexpr int FOOTER_SIZE  = 40;
static constexpr int64_t DATA_START = 12; // magic(8) + entry_count(4)

// ---- byte helpers ----

void SSTable::push_u32be(vector<uint8_t>& buf, uint32_t v) {
    v = htonl(v);
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
    buf.insert(buf.end(), p, p + 4);
}

void SSTable::push_u64be(vector<uint8_t>& buf, uint64_t v) {
    for (int i = 7; i >= 0; --i)
        buf.push_back(static_cast<uint8_t>(v >> (i * 8)));
}

uint32_t SSTable::read_u32be(const uint8_t* p) {
    uint32_t v; memcpy(&v, p, 4); return ntohl(v);
}

uint64_t SSTable::read_u64be(const uint8_t* p) {
    uint64_t v = 0;
    for (int i = 0; i < 8; ++i) v = (v << 8) | p[i];
    return v;
}

// ---- pread helper ----

vector<uint8_t> SSTable::read_at(int fd, int64_t offset, size_t n) {
    vector<uint8_t> buf(n);
    size_t done = 0;
    while (done < n) {
        ssize_t r = pread(fd, buf.data() + done, n - done, offset + (int64_t)done);
        if (r <= 0) { buf.resize(done); return buf; }
        done += (size_t)r;
    }
    return buf;
}

// ---- Destructor ----

SSTable::~SSTable() {
    if (fd_ >= 0) { close(fd_); fd_ = -1; }
}

// ---- Write ----

bool SSTable::write(const string& path, int level, const vector<Entry>& entries) {
    int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { cerr << "SSTable::write: cannot open " << path << "\n"; return false; }

    auto write_buf = [&](const vector<uint8_t>& b) -> bool {
        size_t done = 0;
        while (done < b.size()) {
            ssize_t n = ::write(fd, b.data() + done, b.size() - done);
            if (n <= 0) return false;
            done += (size_t)n;
        }
        return true;
    };
    auto write_raw = [&](const void* data, size_t len) -> bool {
        size_t done = 0;
        const uint8_t* p = static_cast<const uint8_t*>(data);
        while (done < len) {
            ssize_t n = ::write(fd, p + done, len - done);
            if (n <= 0) return false;
            done += (size_t)n;
        }
        return true;
    };

    if (!write_raw(MAGIC, 8)) { close(fd); return false; }
    {
        vector<uint8_t> hdr;
        push_u32be(hdr, (uint32_t)entries.size());
        if (!write_buf(hdr)) { close(fd); return false; }
    }

    int64_t cur = DATA_START;
    BloomFilter bloom((int)entries.size(), config::BLOOM_FP_RATE);
    vector<pair<string, int64_t>> sparse_idx;

    for (int i = 0; i < (int)entries.size(); i++) {
        const auto& e = entries[i];
        bloom.add(e.key);
        if (i % config::SPARSE_INDEX_INTERVAL == 0)
            sparse_idx.push_back({e.key, cur});

        vector<uint8_t> eb;
        push_u32be(eb, (uint32_t)e.key.size());
        eb.insert(eb.end(), e.key.begin(), e.key.end());
        eb.push_back(e.tombstone ? 1 : 0);
        if (!e.tombstone) {
            push_u32be(eb, (uint32_t)e.value.size());
            eb.insert(eb.end(), e.value.begin(), e.value.end());
        }
        cur += (int64_t)eb.size();
        if (!write_buf(eb)) { close(fd); return false; }
    }
    int64_t data_end = cur;

    int64_t bloom_off = cur;
    vector<uint8_t> bloom_bytes = bloom.serialize();
    if (!write_buf(bloom_bytes)) { close(fd); return false; }
    cur += (int64_t)bloom_bytes.size();

    int64_t idx_off = cur;
    {
        vector<uint8_t> ib;
        push_u32be(ib, (uint32_t)sparse_idx.size());
        for (auto& [k, off] : sparse_idx) {
            push_u32be(ib, (uint32_t)k.size());
            ib.insert(ib.end(), k.begin(), k.end());
            push_u64be(ib, (uint64_t)off);
        }
        if (!write_buf(ib)) { close(fd); return false; }
    }

    // Footer: 8+8+8+4+4+8 = 40 bytes
    {
        int64_t ts = chrono::duration_cast<chrono::milliseconds>(
            chrono::system_clock::now().time_since_epoch()).count();
        vector<uint8_t> ft;
        push_u64be(ft, (uint64_t)data_end);
        push_u64be(ft, (uint64_t)bloom_off);
        push_u64be(ft, (uint64_t)idx_off);
        push_u32be(ft, (uint32_t)entries.size());
        push_u32be(ft, (uint32_t)level);
        push_u64be(ft, (uint64_t)ts);
        if (!write_buf(ft)) { close(fd); return false; }
    }

    if (fdatasync(fd) < 0)
        cerr << "SSTable::write: fdatasync failed for " << path << "\n";
    close(fd);
    return true;
}

// ---- Open / load metadata ----

bool SSTable::open(const string& path) {
    path_ = path;
    fd_   = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) { cerr << "SSTable::open: cannot open " << path << "\n"; return false; }
    return load_metadata();
}

bool SSTable::load_metadata() {
    struct stat st;
    if (fstat(fd_, &st) < 0) return false;
    int64_t fsz = st.st_size;
    if (fsz < DATA_START + FOOTER_SIZE) return false;

    auto magic = read_at(fd_, 0, 8);
    if (magic.size() < 8 || memcmp(magic.data(), MAGIC, 8) != 0) {
        cerr << "SSTable::open: bad magic in " << path_ << "\n";
        return false;
    }

    auto ft = read_at(fd_, fsz - FOOTER_SIZE, FOOTER_SIZE);
    if ((int)ft.size() < FOOTER_SIZE) return false;

    data_end_          = (int64_t)read_u64be(ft.data());
    int64_t bloom_off  = (int64_t)read_u64be(ft.data() + 8);
    int64_t idx_off    = (int64_t)read_u64be(ft.data() + 16);
    entry_count_       = read_u32be(ft.data() + 24);
    level_             = (int)read_u32be(ft.data() + 28);
    creation_ts_       = (int64_t)read_u64be(ft.data() + 32);

    if (bloom_off < DATA_START || idx_off <= bloom_off) return false;
    auto bd = read_at(fd_, bloom_off, (size_t)(idx_off - bloom_off));
    bloom_ = BloomFilter::deserialize(bd.data(), bd.size());

    int64_t idx_end = fsz - FOOTER_SIZE;
    if (idx_off >= idx_end) return false;
    auto id = read_at(fd_, idx_off, (size_t)(idx_end - idx_off));
    if (id.size() < 4) return false;

    uint32_t cnt = read_u32be(id.data());
    size_t pos = 4;
    for (uint32_t i = 0; i < cnt; i++) {
        if (pos + 4 > id.size()) break;
        uint32_t klen = read_u32be(id.data() + pos); pos += 4;
        if (pos + klen + 8 > id.size()) break;
        string k(id.begin() + pos, id.begin() + pos + klen); pos += klen;
        int64_t off = (int64_t)read_u64be(id.data() + pos); pos += 8;
        index_.push_back({move(k), off});
    }

    if (!index_.empty()) min_key_ = index_.front().key;

    // Walk from last index entry forward to find the actual max key
    if (entry_count_ > 0) {
        int64_t sp = index_.empty() ? DATA_START : index_.back().offset;
        string last_key;
        while (sp < data_end_) {
            auto kl = read_at(fd_, sp, 4); if (kl.size() < 4) break;
            uint32_t klen = read_u32be(kl.data()); sp += 4;
            auto kb = read_at(fd_, sp, klen); if (kb.size() < klen) break;
            last_key = string(kb.begin(), kb.end()); sp += klen;
            auto tb = read_at(fd_, sp, 1); if (tb.empty()) break;
            bool tomb = tb[0]; sp += 1;
            if (!tomb) {
                auto vl = read_at(fd_, sp, 4); if (vl.size() < 4) break;
                uint32_t vlen = read_u32be(vl.data()); sp += 4 + vlen;
            }
        }
        if (!last_key.empty()) max_key_ = last_key;
    }
    return true;
}

// ---- Get ----

SSTable::LookupResult SSTable::get(const string& key) const {
    LookupResult r;
    if (!bloom_.might_contain(key)) return r;

    int64_t scan_from = DATA_START;
    if (!index_.empty()) {
        int lo = 0, hi = (int)index_.size() - 1, best = -1;
        while (lo <= hi) {
            int mid = (lo + hi) / 2;
            if (index_[mid].key <= key) { best = mid; lo = mid + 1; }
            else                         hi = mid - 1;
        }
        if (best >= 0) scan_from = index_[best].offset;
    }

    int64_t pos = scan_from;
    while (pos < data_end_) {
        auto kl = read_at(fd_, pos, 4); if (kl.size() < 4) break;
        uint32_t klen = read_u32be(kl.data()); pos += 4;
        auto kb = read_at(fd_, pos, klen); if (kb.size() < klen) break;
        string k(kb.begin(), kb.end()); pos += klen;

        if (k > key) break;

        auto tb = read_at(fd_, pos, 1); if (tb.empty()) break;
        bool tomb = tb[0]; pos += 1;

        if (!tomb) {
            auto vl = read_at(fd_, pos, 4); if (vl.size() < 4) break;
            uint32_t vlen = read_u32be(vl.data()); pos += 4;
            if (k == key) { r.found = true; r.value = read_at(fd_, pos, vlen); return r; }
            pos += vlen;
        } else {
            if (k == key) { r.found = true; r.is_tombstone = true; return r; }
        }
    }
    return r;
}

// ---- Scan all ----

vector<SSTable::Entry> SSTable::scan_all() const {
    vector<Entry> entries;
    int64_t pos = DATA_START;
    while (pos < data_end_) {
        auto kl = read_at(fd_, pos, 4); if (kl.size() < 4) break;
        uint32_t klen = read_u32be(kl.data()); pos += 4;
        auto kb = read_at(fd_, pos, klen); if (kb.size() < klen) break;
        string k(kb.begin(), kb.end()); pos += klen;

        auto tb = read_at(fd_, pos, 1); if (tb.empty()) break;
        bool tomb = tb[0]; pos += 1;

        Entry e; e.key = move(k); e.tombstone = tomb;
        if (!tomb) {
            auto vl = read_at(fd_, pos, 4); if (vl.size() < 4) break;
            uint32_t vlen = read_u32be(vl.data()); pos += 4;
            e.value = read_at(fd_, pos, vlen);
            if (e.value.size() < vlen) break;
            pos += vlen;
        }
        entries.push_back(move(e));
    }
    return entries;
}
