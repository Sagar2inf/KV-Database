#include "router.hpp"

static vector<uint8_t> str_resp(const string& s) {
    return vector<uint8_t>(s.begin(), s.end());
}

vector<uint8_t> Router::pass_data(const vector<uint8_t>& request) {
    if (service_->validate_request(request) < 0)
        return str_resp("ERROR: bad request");

    auto [key_with_cmd, value] = service_->parse_request(request);
    int    command = int(key_with_cmd[0] - '0');
    string key     = key_with_cmd.substr(1);

    if (command == 1) { // SET
        vector<uint8_t> wal_data = service_->transform_to_wal(command, key, value);
        if (!memtable_->set_wal(wal_data))
            return str_resp("ERROR: failed to write WAL");
        if (!memtable_->set(key, value))
            return str_resp("ERROR: failed to set key");
        memtable_->maybe_flush();
        return str_resp("OK");
    }

    if (command == 2) { // GET — no WAL write
        vector<uint8_t> val = memtable_->get(key);
        if (val.empty()) return str_resp("NOT_FOUND");
        return val;
    }

    if (command == 3) { // DELETE
        vector<uint8_t> empty;
        vector<uint8_t> wal_data = service_->transform_to_wal(command, key, empty);
        if (!memtable_->set_wal(wal_data))
            return str_resp("ERROR: failed to write WAL");
        memtable_->remove(key);
        memtable_->maybe_flush();
        return str_resp("OK");
    }

    return str_resp("ERROR: unknown command");
}
