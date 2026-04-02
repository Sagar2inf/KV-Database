#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>

using namespace std;

class Service{
public:
    int validate_request(const vector<uint8_t> &request);
    pair<string, vector<uint8_t>> parse_request(const vector<uint8_t> &request);
};