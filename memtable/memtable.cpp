#include "memtable.hpp"

bool Memtable::set_skiplist(string key, vector<uint8_t>& value){
    try{
        skiplist->insert(key, value);
        return true;
    } catch(...){
        return false;
    }
}
bool Memtable::set_wal(const vector<uint8_t>& data){
    try{
        wal->write_data(data);
        return true;
    } catch(...){
        return false;
    }
}
bool Memtable::remove(string key){
    try{
        skiplist->remove(key);
        return true;
    } catch(...){
        return false;
    }
}
vector<uint8_t> Memtable::get(string key){
    try{
        vector<uint8_t> res = skiplist->get(key);
        return res;
    } catch(...){
        string s = "Failed to get data";
        return vector<uint8_t>(s.begin(), s.end());
    }
}

