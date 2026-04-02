#include "transport.hpp"


std::vector<uint8_t> Transport::read_data(int client){
    uint32_t len;
    int received = 0;
    while(received < sizeof(uint32_t)){
        int x = read(client, reinterpret_cast<char*>(&len) + received, sizeof(uint32_t) - received);
        if(x <= 0){
            return {};
        }
        received += x;
    }
    len = ntohl(len);
    std::vector<uint8_t>buffer(len);
    received = 0;
    while(received < len){
        int n = read(client, buffer.data() + received, len - received);
        if(n <= 0) return {};
        received += n;
    }
    return buffer;
}

bool Transport::write_data(int client, const vector<uint8_t>& data){

    cout << client << endl;
    for(auto & it: data){
        cout << it << " ";
    }
    cout << endl;

    uint32_t len = htonl(data.size());
    int sent = 0;
    while(sent < sizeof(uint32_t)){
        int x = write(client, reinterpret_cast<char*>(&len) + sent, sizeof(uint32_t) - sent);
        if(x <= 0){
            return false;
        }
        sent += x;
    }
    cout << "debug" << endl;
    size_t sz = 0;
    while(sz < data.size()){
        cout << "debug0.4" << endl;
        ssize_t x = write(client, data.data() + sz, data.size() - sz);
        cout << "Debug0.5" << endl;
        cout << x << endl;
        if(x <= 0){
            return false;
        }
        sz += x;
    }
    cout << "debug2" << endl;
    return true;
}

