#include "transport.hpp"

vector<uint8_t> Transport::read_data(int client) {
    uint32_t len = 0;
    size_t received = 0;
    while (received < sizeof(uint32_t)) {
        int x = recv(client, reinterpret_cast<char*>(&len) + received,
                     (int)(sizeof(uint32_t) - received), 0);
        if (x <= 0) return {};
        received += (size_t)x;
    }
    len = ntohl(len);
    vector<uint8_t> buffer(len);
    received = 0;
    while (received < len) {
        int n = recv(client, reinterpret_cast<char*>(buffer.data() + received),
                     (int)(len - received), 0);
        if (n <= 0) return {};
        received += (size_t)n;
    }
    return buffer;
}

bool Transport::write_data(int client, const vector<uint8_t>& data) {
    uint32_t len = htonl((uint32_t)data.size());
    size_t sent = 0;
    while (sent < sizeof(uint32_t)) {
        int x = send(client, reinterpret_cast<const char*>(&len) + sent,
                     (int)(sizeof(uint32_t) - sent), 0);
        if (x <= 0) return false;
        sent += (size_t)x;
    }
    size_t sz = 0;
    while (sz < data.size()) {
        int x = send(client, reinterpret_cast<const char*>(data.data() + sz),
                     (int)(data.size() - sz), 0);
        if (x <= 0) return false;
        sz += (size_t)x;
    }
    return true;
}
