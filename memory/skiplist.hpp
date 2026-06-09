#pragma once
#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <random>
#include <mutex>
#include <shared_mutex>

using namespace std;

namespace types {
    struct Entry {
        string Key;
        vector<uint8_t> Value;
        bool Tombstone = false;
    };
}

struct Element : public types::Entry {
    vector<Element*> next;
};

struct SkiplistSnapshot {
    string key;
    vector<uint8_t> value;
    bool tombstone;
};

class Skiplist {
public:
    int maxlevel;
    long double p;
    int level;
    int size;
    Element* head;

    mutable shared_mutex mtx_;

    Skiplist() {
        maxlevel = 16;
        p = 0.5;
        level = 1;
        size = 0;
        head = new Element();
        head->Key = "";
        head->next.resize(maxlevel, nullptr);
    }
    ~Skiplist();
    Skiplist(const Skiplist&) = delete;
    Skiplist& operator=(const Skiplist&) = delete;

    int randomLevel();
    void insert(string key, vector<uint8_t> val);
    void remove(string key);
    vector<uint8_t> get(string key) const;

    // O(log n) lookup that distinguishes not-found / live / tombstone.
    struct LookupResult { bool found = false; bool tombstone = false; vector<uint8_t> value; };
    LookupResult lookup(const string& key) const;

    // Return all entries (including tombstones) in sorted key order.
    vector<SkiplistSnapshot> snapshot() const;

    // Approximate memory usage in bytes.
    size_t approx_bytes() const;

    // Remove all nodes; resets to empty state.
    void clear();
};
