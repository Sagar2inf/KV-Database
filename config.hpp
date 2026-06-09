#pragma once
#include <string>
#include <cstddef>

namespace config {
    // Server
    constexpr int PORT = 9000;
    constexpr int BACKLOG = 128;
    constexpr int MAX_CLIENTS = 256;

    // Memtable: flush to SSTable when approximate in-memory bytes exceeds this
    constexpr size_t MEMTABLE_FLUSH_BYTES = 4 * 1024 * 1024; // 4 MB

    // L0 compaction: compact all L0 SSTables into one L1 when L0 count reaches this
    constexpr int L0_COMPACT_TRIGGER = 4;

    // SSTable bloom filter false-positive rate
    constexpr double BLOOM_FP_RATE = 0.01;

    // Sparse index: one index entry per this many data entries
    constexpr int SPARSE_INDEX_INTERVAL = 16;

    // Paths relative to the server's working directory
    inline const std::string DATA_DIR = "data";
    inline const std::string WAL_DIR  = "data/wal";
    inline const std::string WAL_FILE = "walfile";
    inline const std::string SST_DIR  = "data/sstables";
}
