#include <iostream>
#include <vector>
#include <string>
#include <cstdint>
#include <random>

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

class Skiplist{
public:
    int maxlevel;
    long double p;
    int level;
    int size;
    Element* head;
    Skiplist(){
        maxlevel = 16;
        p = 0.5;
        level = 1;
        size = 0;
        head = new Element();
        head->Key = "";
        head->next.resize(maxlevel, nullptr);
    }
    int randomLevel();
    void insert(string key, vector<uint8_t> val);
    void remove(string key);
    vector<uint8_t> get(string key);    

};
