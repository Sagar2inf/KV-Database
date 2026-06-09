#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include "../platform.hpp"

using namespace std;

class Service{
public:
    int validate_request(const vector<uint8_t> &request);
    pair<string, vector<uint8_t>> parse_request(const vector<uint8_t> &request);
    vector<uint8_t> transform_to_wal(int command, string key, vector<uint8_t>& value);
};