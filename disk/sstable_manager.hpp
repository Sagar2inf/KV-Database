#pragma once
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include "sstable.hpp"

using namespace std;

/*
Manages a collection of SSTable files across two levels:
  L0 – direct memtable flushes (key ranges may overlap between files, newest first)
  L1 – result of L0 compaction (merged, sorted, single file typical)

Read order: L0 (newest→oldest) then L1 (newest→oldest).
The first found (non-empty) result wins; tombstones stop the search.

Compaction: when L0 reaches L0_COMPACT_TRIGGER files, all L0 files are merged
with any existing L1 files into a single new L1 file, then old files are removed.
*/
class SSTableManager {
public:
    explicit SSTableManager(const string& dir);

    // Flush a sorted list of entries to a new L0 SSTable.
    bool flush(const vector<SSTable::Entry>& entries);

    // Look up a key across all SSTables, newest first.
    // Returns empty vector if key not found or is a tombstone.
    vector<uint8_t> get(const string& key);

    // Compact L0 into L1 if L0 count >= config::L0_COMPACT_TRIGGER.
    void maybe_compact();

    int l0_count() const;
    int total_count() const;

private:
    string dir_;
    // tables_ is ordered: L0 newest-first, then L1 newest-first
    vector<unique_ptr<SSTable>> tables_;
    mutable mutex mtx_;

    void load_existing();
    string make_sst_path(int level) const;
    void sort_tables();
    bool compact();
};
