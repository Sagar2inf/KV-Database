#include "sstable_manager.hpp"
#include "../config.hpp"
#include <filesystem>
#include <chrono>
#include <map>
#include <algorithm>
#include <iostream>

namespace fs = filesystem;
using namespace chrono;

SSTableManager::SSTableManager(const string& dir) : dir_(dir) {
    fs::create_directories(dir_);
    load_existing();
}

// Load and open all .sst files already on disk.
void SSTableManager::load_existing() {
    if (!fs::exists(dir_)) return;
    for (auto& de : fs::directory_iterator(dir_)) {
        if (de.path().extension() != ".sst") continue;
        auto t = make_unique<SSTable>();
        if (t->open(de.path().string()))
            tables_.push_back(move(t));
        else
            cerr << "SSTableManager: failed to open " << de.path() << "\n";
    }
    sort_tables();
}

// Sort: L0 newest-first, then L1 newest-first.
void SSTableManager::sort_tables() {
    sort(tables_.begin(), tables_.end(), [](const unique_ptr<SSTable>& a,
                                            const unique_ptr<SSTable>& b) {
        if (a->level() != b->level()) return a->level() < b->level();
        return a->creation_ts() > b->creation_ts(); // newer first within a level
    });
}

// Build a unique filename: L{level}_{timestamp_ms}.sst
string SSTableManager::make_sst_path(int level) const {
    int64_t ts = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
    return dir_ + "/L" + to_string(level) + "_" + to_string(ts) + ".sst";
}

bool SSTableManager::flush(const vector<SSTable::Entry>& entries) {
    if (entries.empty()) return true;
    lock_guard<mutex> lock(mtx_);

    string path = make_sst_path(0);
    if (!SSTable::write(path, 0, entries)) {
        cerr << "SSTableManager::flush: write failed\n";
        return false;
    }
    auto t = make_unique<SSTable>();
    if (!t->open(path)) {
        cerr << "SSTableManager::flush: cannot re-open " << path << "\n";
        return false;
    }
    // Insert at front of L0 (newest first)
    tables_.insert(tables_.begin(), move(t));
    return true;
}

vector<uint8_t> SSTableManager::get(const string& key) {
    lock_guard<mutex> lock(mtx_);
    for (auto& t : tables_) {
        auto res = t->get(key);
        if (res.found) {
            if (res.is_tombstone) return {}; // key was deleted
            return res.value;
        }
    }
    return {};
}

void SSTableManager::maybe_compact() {
    {
        lock_guard<mutex> lock(mtx_);
        if (l0_count() < config::L0_COMPACT_TRIGGER) return;
    }
    compact();
}

bool SSTableManager::compact() {
    lock_guard<mutex> lock(mtx_);

    // Gather all tables (both L0 and L1) for a full merge.
    // Use a map keyed by (key) where the value is the entry from the newest
    // table that contains that key (tables_ is already newest-first).
    map<string, SSTable::Entry> merged;
    for (auto& t : tables_) {
        auto entries = t->scan_all();
        for (auto& e : entries) {
            if (merged.find(e.key) == merged.end())
                merged[e.key] = e; // first seen = newest version (tables_ sorted newest-first)
        }
    }

    // Collect in sorted key order (map iterates in key order).
    vector<SSTable::Entry> compacted;
    compacted.reserve(merged.size());
    for (auto& [k, e] : merged)
        compacted.push_back(e);

    string new_path = make_sst_path(1);
    if (!SSTable::write(new_path, 1, compacted)) {
        cerr << "SSTableManager::compact: write failed\n";
        return false;
    }

    // Delete all old SSTable files.
    for (auto& t : tables_) {
        if (::remove(t->path().c_str()) != 0)
            cerr << "SSTableManager::compact: cannot delete " << t->path() << "\n";
    }
    tables_.clear();

    // Open the new compacted L1 file.
    auto nt = make_unique<SSTable>();
    if (!nt->open(new_path)) {
        cerr << "SSTableManager::compact: cannot open " << new_path << "\n";
        return false;
    }
    tables_.push_back(move(nt));
    cerr << "Compacted " << merged.size() << " entries into " << new_path << "\n";
    return true;
}

int SSTableManager::l0_count() const {
    int n = 0;
    for (auto& t : tables_) if (t->level() == 0) n++;
    return n;
}

int SSTableManager::total_count() const { return (int)tables_.size(); }
