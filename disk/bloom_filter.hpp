#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <algorithm>
#include "../platform.hpp"

using namespace std;

// Bloom filter using double hashing over a MurmurHash64 base.
class BloomFilter {
    vector<uint8_t> bits_;
    int m_ = 0; // total bits
    int k_ = 0; // number of hash functions

    void set_bit(int pos) { bits_[pos >> 3] |= (1u << (pos & 7)); }
    bool get_bit(int pos) const { return (bits_[pos >> 3] >> (pos & 7)) & 1; }

    static uint64_t murmur64(const void* key, int len, uint64_t seed) {
        const uint64_t m = 0xc6a4a7935bd1e995ULL;
        uint64_t h = seed ^ ((uint64_t)len * m);
        const uint8_t* data = static_cast<const uint8_t*>(key);
        while (len >= 8) {
            uint64_t k;
            memcpy(&k, data, 8);
            k *= m; k ^= k >> 47; k *= m;
            h ^= k; h *= m;
            data += 8; len -= 8;
        }
        switch (len) {
            case 7: h ^= (uint64_t)data[6] << 48; [[fallthrough]];
            case 6: h ^= (uint64_t)data[5] << 40; [[fallthrough]];
            case 5: h ^= (uint64_t)data[4] << 32; [[fallthrough]];
            case 4: h ^= (uint64_t)data[3] << 24; [[fallthrough]];
            case 3: h ^= (uint64_t)data[2] << 16; [[fallthrough]];
            case 2: h ^= (uint64_t)data[1] << 8;  [[fallthrough]];
            case 1: h ^= (uint64_t)data[0];        h *= m;
        }
        h ^= h >> 47; h *= m; h ^= h >> 47;
        return h;
    }

public:
    BloomFilter() = default;

    // n = expected number of elements; fp_rate = target false positive rate
    BloomFilter(int n, double fp_rate) {
        n = max(n, 1);
        m_ = max(64, (int)ceil(-n * log(fp_rate) / (log(2.0) * log(2.0))));
        k_ = max(1, min(20, (int)round((double)m_ * log(2.0) / n)));
        bits_.assign((m_ + 7) / 8, 0);
    }

    void add(const string& key) {
        uint64_t h1 = murmur64(key.data(), (int)key.size(), 0xdeadbeef12345678ULL);
        uint64_t h2 = murmur64(key.data(), (int)key.size(), 0xcafebabe87654321ULL);
        for (int i = 0; i < k_; i++)
            set_bit((int)((h1 + (uint64_t)i * h2) % (uint64_t)m_));
    }

    bool might_contain(const string& key) const {
        if (m_ == 0) return true;
        uint64_t h1 = murmur64(key.data(), (int)key.size(), 0xdeadbeef12345678ULL);
        uint64_t h2 = murmur64(key.data(), (int)key.size(), 0xcafebabe87654321ULL);
        for (int i = 0; i < k_; i++)
            if (!get_bit((int)((h1 + (uint64_t)i * h2) % (uint64_t)m_)))
                return false;
        return true;
    }

    // Wire format: [m:4 big-endian][k:1][bits: ceil(m/8) bytes]
    vector<uint8_t> serialize() const {
        vector<uint8_t> res;
        uint32_t m_be = htonl((uint32_t)m_);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&m_be);
        res.insert(res.end(), p, p + 4);
        res.push_back((uint8_t)k_);
        res.insert(res.end(), bits_.begin(), bits_.end());
        return res;
    }

    static BloomFilter deserialize(const uint8_t* data, size_t size) {
        BloomFilter bf;
        if (size < 5) return bf;
        uint32_t m_be;
        memcpy(&m_be, data, 4);
        bf.m_ = (int)ntohl(m_be);
        bf.k_ = data[4];
        int byte_count = (bf.m_ + 7) / 8;
        if ((int)size < 5 + byte_count) return bf;
        bf.bits_.assign(data + 5, data + 5 + byte_count);
        return bf;
    }

    int serialized_size() const { return 4 + 1 + (int)bits_.size(); }
};
