#include "skiplist.hpp"
#include <mutex>

static double getRandom() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gen);
}

int Skiplist::randomLevel() {
    int lvl = 1;
    while (getRandom() < p && lvl < maxlevel) lvl++;
    return lvl;
}

void Skiplist::insert(string key, vector<uint8_t> val) {
    unique_lock<shared_mutex> lock(mtx_);
    Element* curr = head;
    vector<Element*> update(maxlevel, nullptr);

    for (int i = level - 1; i >= 0; i--) {
        while (curr->next[i] && curr->next[i]->Key < key)
            curr = curr->next[i];
        update[i] = curr;
    }

    if (curr->next[0] && curr->next[0]->Key == key) {
        curr = curr->next[0];
        if (curr->Tombstone) size++;
        curr->Value = val;
        curr->Tombstone = false;
        return;
    }

    int lvl = randomLevel();
    if (lvl > level) {
        for (int i = level; i < lvl; i++) update[i] = head;
        level = lvl;
    }
    Element* node = new Element();
    node->Key = key;
    node->Value = val;
    node->next.resize(lvl, nullptr);
    size++;

    for (int i = 0; i < lvl; i++) {
        node->next[i] = update[i]->next[i];
        update[i]->next[i] = node;
    }
}

void Skiplist::remove(string key) {
    unique_lock<shared_mutex> lock(mtx_);
    Element* curr = head;
    for (int i = level - 1; i >= 0; i--)
        while (curr->next[i] && curr->next[i]->Key < key)
            curr = curr->next[i];

    curr = curr->next[0];
    if (curr && curr->Key == key && !curr->Tombstone) {
        curr->Tombstone = true;
        size--;
    }
}

vector<uint8_t> Skiplist::get(string key) const {
    shared_lock<shared_mutex> lock(mtx_);
    const Element* curr = head;
    for (int i = level - 1; i >= 0; i--)
        while (curr->next[i] && curr->next[i]->Key < key)
            curr = curr->next[i];

    curr = curr->next[0];
    if (curr && curr->Key == key && !curr->Tombstone)
        return curr->Value;
    return {};
}

Skiplist::LookupResult Skiplist::lookup(const string& key) const {
    shared_lock<shared_mutex> lock(mtx_);
    const Element* curr = head;
    for (int i = level - 1; i >= 0; i--)
        while (curr->next[i] && curr->next[i]->Key < key)
            curr = curr->next[i];
    curr = curr->next[0];
    if (!curr || curr->Key != key) return {};
    if (curr->Tombstone) return {true, true, {}};
    return {true, false, curr->Value};
}

vector<SkiplistSnapshot> Skiplist::snapshot() const {
    shared_lock<shared_mutex> lock(mtx_);
    vector<SkiplistSnapshot> out;
    const Element* curr = head->next[0];
    while (curr) {
        out.push_back({curr->Key, curr->Value, curr->Tombstone});
        curr = curr->next[0];
    }
    return out;
}

size_t Skiplist::approx_bytes() const {
    shared_lock<shared_mutex> lock(mtx_);
    size_t total = 0;
    const Element* curr = head->next[0];
    while (curr) {
        total += curr->Key.size() + curr->Value.size() + 64;
        curr = curr->next[0];
    }
    return total;
}

void Skiplist::clear() {
    unique_lock<shared_mutex> lock(mtx_);
    Element* curr = head->next[0];
    while (curr) {
        Element* next = curr->next[0];
        delete curr;
        curr = next;
    }
    // Reset head's forward pointers and counters
    fill(head->next.begin(), head->next.end(), nullptr);
    level = 1;
    size  = 0;
}

Skiplist::~Skiplist() {
    // Do NOT lock here — destructor is called when no other threads use this.
    Element* curr = head;
    while (curr) {
        Element* next = curr->next[0];
        delete curr;
        curr = next;
    }
}
