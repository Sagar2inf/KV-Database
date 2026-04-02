#include "../memtable/memtable.hpp"
#include "../services/service.hpp"

class Router{
private:
    int command;
    string key;
    vector<uint8_t> value;
    Service* service;
    Memtable * memtable;
public:
    Router(){
        this->service = new Service();
        this->memtable = new Memtable("walfile", "../wal");
    }
    vector<uint8_t> pass_data(const vector<uint8_t>& request);
};