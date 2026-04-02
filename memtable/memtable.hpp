#include "../disk/wal.hpp"
#include "../memory/skiplist.hpp"

class Memtable{
private:
    Skiplist* skiplist;
    WAL* wal;
public:
    Memtable(string walname, string waldir){
        this->skiplist = new Skiplist();
        this->wal = new WAL(walname, waldir);
    }
    bool set_skiplist(string key, vector<uint8_t>& value);    
    bool set_wal(const vector<uint8_t>& data);
    bool remove(string key);
    vector<uint8_t> get(string key);
};