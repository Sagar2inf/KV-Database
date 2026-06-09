#pragma once
#include "../disk/wal.hpp"
#include "../disk/sstable_manager.hpp"
#include "../memory/skiplist.hpp"
#include "../config.hpp"
#include "../platform.hpp"
#include <atomic>

class Memtable {
    Skiplist*        skiplist_;
    WAL*             wal_;
    SSTableManager*  sst_mgr_;
    atomic<size_t>   mem_bytes_{0};

public:
    Memtable(const string& wal_dir, const string& wal_file, const string& sst_dir);
    ~Memtable();
    Memtable(const Memtable&) = delete;
    Memtable& operator=(const Memtable&) = delete;

    // Write raw WAL bytes (pre-serialized by Service::transform_to_wal).
    bool            set_wal(const vector<uint8_t>& data);
    bool            set(string key, vector<uint8_t>& value);
    bool            remove(string key);
    vector<uint8_t> get(string key);

    // Replay WAL to reconstruct the in-memory skiplist.
    void recover();

    // Flush skiplist to a new L0 SSTable, then truncate the WAL and clear
    // the skiplist. Triggers L0 compaction if needed.
    void flush();

    // Flush if in-memory size exceeds the configured threshold.
    void maybe_flush();
};
