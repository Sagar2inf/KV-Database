#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include "bloom_filter.hpp"

using namespace std;

/*
SSTable file layout (all multi-byte integers big-endian):

  HEADER  (12 bytes):
    magic[8]        "SSTR1.0\0"
    entry_count[4]

  DATA SECTION (variable):
    For each entry, sorted by key:
      key_len[4]
      key[key_len]
      tombstone[1]   1 = deleted, 0 = live value
      value_len[4]   only if tombstone == 0
      value[value_len]  only if tombstone == 0

  BLOOM SECTION:
    m[4]  k[1]  bits[ceil(m/8)]

  INDEX SECTION:
    count[4]
    For each sparse index entry (every SPARSE_INDEX_INTERVAL-th data entry):
      key_len[4]  key[key_len]  offset[8]   (offset = start of that entry in DATA)

  FOOTER (40 bytes):
    data_end[8]       byte offset where DATA SECTION ends
    bloom_offset[8]
    index_offset[8]
    entry_count[4]
    level[4]
    creation_ts_ms[8]
*/

class SSTable {
public:
    struct Entry {
        string key;
        vector<uint8_t> value;
        bool tombstone = false;
    };

    struct LookupResult {
        bool found       = false;
        bool is_tombstone = false;
        vector<uint8_t> value;
    };

    SSTable() = default;
    ~SSTable();
    SSTable(const SSTable&) = delete;
    SSTable& operator=(const SSTable&) = delete;

    // Write a new SSTable file from a sorted list of entries.
    static bool write(const string& path, int level, const vector<Entry>& entries);

    // Open an existing SSTable for reading. Returns false on failure.
    bool open(const string& path);

    // Point lookup using bloom filter + sparse index + sequential scan.
    LookupResult get(const string& key) const;

    // Return all entries in key order (used for compaction).
    vector<Entry> scan_all() const;

    const string&  path()        const { return path_; }
    int            level()       const { return level_; }
    uint32_t       entry_count() const { return entry_count_; }
    int64_t        creation_ts() const { return creation_ts_; }
    const string&  min_key()     const { return min_key_; }
    const string&  max_key()     const { return max_key_; }

private:
    string   path_;
    int      level_       = 0;
    uint32_t entry_count_ = 0;
    int64_t  creation_ts_ = 0;
    string   min_key_, max_key_;

    BloomFilter bloom_;

    struct IndexEntry { string key; int64_t offset; };
    vector<IndexEntry> index_;

    int64_t data_end_    = 0;
    int     fd_          = -1;

    bool load_metadata();

    // Read exactly n bytes from fd at file offset. Returns fewer bytes on EOF/error.
    static vector<uint8_t> read_at(int fd, int64_t offset, size_t n);

    static void     push_u32be(vector<uint8_t>& buf, uint32_t v);
    static void     push_u64be(vector<uint8_t>& buf, uint64_t v);
    static uint32_t read_u32be(const uint8_t* p);
    static uint64_t read_u64be(const uint8_t* p);
};
