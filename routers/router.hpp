#pragma once
#include "../memtable/memtable.hpp"
#include "../services/service.hpp"
#include "../config.hpp"

class Router {
    Service*   service_;
    Memtable*  memtable_;

public:
    Router()
        : service_(new Service()),
          memtable_(new Memtable(config::WAL_DIR, config::WAL_FILE, config::SST_DIR))
    {}
    ~Router() { delete service_; delete memtable_; }

    Router(const Router&) = delete;
    Router& operator=(const Router&) = delete;

    vector<uint8_t> pass_data(const vector<uint8_t>& request);

    void recover() { memtable_->recover(); }

    // Force-flush the memtable to an SSTable (e.g. on graceful shutdown).
    void flush_memtable() { memtable_->flush(); }
};
