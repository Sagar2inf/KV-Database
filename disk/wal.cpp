#include "wal.hpp"
#include <filesystem>
#include <fcntl.h>

namespace fs = filesystem;

WAL::WAL(const string& name, const string& dir) {
    fs::create_directories(dir);
    path_ = dir + "/" + name;

    fr_ = open(path_.c_str(), O_RDONLY | O_CREAT, 0664);
    if (fr_ < 0) cerr << "WAL: failed to open for reading: " << path_ << "\n";

    fw_ = open(path_.c_str(), O_WRONLY | O_APPEND, 0664);
    if (fw_ < 0) cerr << "WAL: failed to open for writing: " << path_ << "\n";
}

WAL::~WAL() {
    if (fr_ >= 0) { close(fr_); fr_ = -1; }
    if (fw_ >= 0) { close(fw_); fw_ = -1; }
}

void WAL::write_data(const vector<uint8_t>& data) {
    size_t done = 0;
    while (done < data.size()) {
        ssize_t n = write(fw_, data.data() + done, data.size() - done);
        if (n < 0) { cerr << "WAL: write failed\n"; return; }
        done += (size_t)n;
    }
    // Flush to storage so a crash doesn't lose this entry.
    if (fdatasync(fw_) < 0)
        cerr << "WAL: fdatasync failed\n";
}

vector<uint8_t> WAL::read_data(bool is_start, int data_size) {
    if (is_start) lseek(fr_, 0, SEEK_SET);
    vector<uint8_t> buffer(data_size);
    size_t done = 0;
    while ((int)done < data_size) {
        ssize_t n = read(fr_, buffer.data() + done, data_size - done);
        if (n < 0) { cerr << "WAL: read failed\n"; return {}; }
        if (n == 0) { buffer.resize(done); return buffer; } // EOF
        done += (size_t)n;
    }
    return buffer;
}

void WAL::truncate() {
    if (fr_ >= 0) { close(fr_); fr_ = -1; }
    if (fw_ >= 0) { close(fw_); fw_ = -1; }

    // Open with O_TRUNC to zero the file, then close immediately.
    int f = open(path_.c_str(), O_WRONLY | O_TRUNC, 0664);
    if (f >= 0) close(f);

    fr_ = open(path_.c_str(), O_RDONLY, 0664);
    if (fr_ < 0) cerr << "WAL: failed to reopen for reading after truncate\n";

    fw_ = open(path_.c_str(), O_WRONLY | O_APPEND, 0664);
    if (fw_ < 0) cerr << "WAL: failed to reopen for writing after truncate\n";
}
