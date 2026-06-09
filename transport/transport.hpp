#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>
#include <cstring>
#include "../platform.hpp"

using namespace std;

class Transport {
public:
    vector<uint8_t> read_data(int client);
    bool write_data(int client, const vector<uint8_t>& data);
};
