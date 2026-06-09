#pragma once
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include "../platform.hpp"
#include <fcntl.h>

using namespace std;

/*
WAL entry format (all 4-byte lengths are big-endian / network byte order):
  [total_len : 4]  bytes that follow (excludes this field itself)
  [command   : 1]  1=SET, 3=DELETE
  [key_len   : 4]
  [key       : key_len]
  [value_len : 4]  only present when command == 1
  [value     : value_len]  only present when command == 1
*/
class WAL {
    int    fr_, fw_;
    string path_;

public:
    WAL(const string& name, const string& dir);
    ~WAL();
    WAL(const WAL&) = delete;
    WAL& operator=(const WAL&) = delete;

    // Append data to the WAL and fdatasync.
    void write_data(const vector<uint8_t>& data);

    // Read exactly data_size bytes. If is_start, seek to the beginning first.
    vector<uint8_t> read_data(bool is_start, int data_size);

    // Truncate the WAL to zero bytes and reopen for append.
    void truncate();
};
