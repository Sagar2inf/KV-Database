#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>

using namespace std;

class WAL{
    int fr, fw;
    int size;
    string walname;
    string dir;
    string path;
public:
    WAL(string name, string dir){
        this->walname = name;
        this->dir = dir;
        this->path = dir + "/" + name;
        this->size = 0;
        fr = open(path.c_str(), O_RDONLY | O_CREAT, 0664);
        if(fr < 0){
            cerr << "Failed to open WAL file for reading" << endl;
             return;
        }
        fw = open(path.c_str(), O_WRONLY | O_APPEND, 0664);
        if(fw < 0){
            cerr << "Failed to open WAL file for writing" << endl;
            return;
        }
    }
    void write_data(const vector<uint8_t> data);
    void read_data();
};