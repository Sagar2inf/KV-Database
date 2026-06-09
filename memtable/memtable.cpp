#include "memtable.hpp"
#include <iostream>

Memtable::Memtable(const string& wal_dir, const string& wal_file,
                   const string& sst_dir)
    : skiplist_(new Skiplist()),
      wal_(new WAL(wal_file, wal_dir)),
      sst_mgr_(new SSTableManager(sst_dir))
{}

Memtable::~Memtable() {
    delete skiplist_;
    delete wal_;
    delete sst_mgr_;
}

bool Memtable::set_wal(const vector<uint8_t>& data) {
    try { wal_->write_data(data); return true; }
    catch (...) { return false; }
}

bool Memtable::set(string key, vector<uint8_t>& value) {
    try {
        skiplist_->insert(key, value);
        mem_bytes_ += key.size() + value.size() + 64;
        return true;
    } catch (...) { return false; }
}

bool Memtable::remove(string key) {
    try {
        skiplist_->remove(key);
        mem_bytes_ += key.size() + 64;
        return true;
    } catch (...) { return false; }
}

vector<uint8_t> Memtable::get(string key) {
    // O(log n) lookup: distinguishes not-found vs tombstone vs live value.
    auto res = skiplist_->lookup(key);
    if (res.found) {
        if (res.tombstone) return {}; // deleted in memtable, don't check SSTables
        return res.value;
    }
    // Not in memtable — check SSTables (bloom filter + sparse index inside).
    return sst_mgr_->get(key);
}

// ---- WAL recovery ----

void Memtable::recover() {
    cout << "Recovering WAL...\n";
    auto read_u32 = [&](const vector<uint8_t>& buf) -> uint32_t {
        if (buf.size() < 4) return 0;
        uint32_t v; memcpy(&v, buf.data(), 4); return ntohl(v);
    };

    bool first = true;
    while (true) {
        auto total_len_buf = wal_->read_data(first, sizeof(uint32_t));
        if (total_len_buf.size() < sizeof(uint32_t)) break;
        first = false;

        uint32_t total_len = read_u32(total_len_buf);

        auto cmd_buf = wal_->read_data(false, 1);
        if (cmd_buf.empty()) break;
        uint8_t command = cmd_buf[0];

        auto key_len_buf = wal_->read_data(false, sizeof(uint32_t));
        uint32_t key_len = read_u32(key_len_buf);

        auto key_buf = wal_->read_data(false, key_len);
        string key(key_buf.begin(), key_buf.end());

        if (command == 1) { // SET
            auto val_len_buf = wal_->read_data(false, sizeof(uint32_t));
            uint32_t val_len = read_u32(val_len_buf);
            auto val_buf = wal_->read_data(false, val_len);

            if (total_len == 1 + 4 + key_len + 4 + val_len) {
                skiplist_->insert(key, val_buf);
                mem_bytes_ += key.size() + val_buf.size() + 64;
                cout << "  recovered SET " << key << "\n";
            } else {
                cerr << "WAL corrupted at key=" << key << " — stopping recovery\n";
                break;
            }
        } else if (command == 3) { // DELETE
            if (total_len == 1 + 4 + key_len) {
                skiplist_->remove(key);
                cout << "  recovered DELETE " << key << "\n";
            } else {
                cerr << "WAL corrupted at key=" << key << " — stopping recovery\n";
                break;
            }
        } else {
            cerr << "WAL: unknown command " << (int)command << " — stopping\n";
            break;
        }
    }
    cout << "Recovery done. " << skiplist_->size << " live keys in memtable.\n";
}

// ---- Flush ----

void Memtable::flush() {
    auto entries_raw = skiplist_->snapshot();
    if (entries_raw.empty()) return;

    // Convert to SSTable::Entry format (include tombstones so they shadow
    // older SSTable entries during compaction).
    vector<SSTable::Entry> entries;
    entries.reserve(entries_raw.size());
    for (auto& e : entries_raw)
        entries.push_back({e.key, e.value, e.tombstone});

    if (!sst_mgr_->flush(entries)) {
        cerr << "Memtable::flush: SSTable write failed — keeping WAL\n";
        return;
    }

    // WAL is now redundant; truncate it and reset in-memory state.
    wal_->truncate();
    skiplist_->clear();
    mem_bytes_ = 0;

    sst_mgr_->maybe_compact();
}

void Memtable::maybe_flush() {
    if (mem_bytes_ >= config::MEMTABLE_FLUSH_BYTES)
        flush();
}
