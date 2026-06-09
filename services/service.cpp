#include "service.hpp"

// Returns 0 if valid, -1 if invalid syntax, -2 if missing argument.
int Service::validate_request(const vector<uint8_t>& request) {
    if (request.empty()) return -1;

    // Strip spaces and validate printable ASCII (excluding ':')
    string s;
    for (uint8_t b : request) {
        if (b < 32 || b > 126 || b == ':') return -1;
        if (b != ' ') s += (char)b;
    }
    if (s.empty()) return -1;

    char c0 = (char)tolower(s[0]);
    if (c0 == 's' || c0 == 'g') {
        // Expect "set" or "get"
        if (s.size() < 3) return -2;
        if (tolower(s[1]) != 'e' || tolower(s[2]) != 't') return -1;
        // GET needs a key; SET needs key=value
        if (c0 == 's') {
            // SET key=value  (at least "set" + one char key + "=" + one char value)
            if (s.size() < 6) return -2;
        } else {
            // GET key (at least "get" + one char key)
            if (s.size() < 4) return -2;
        }
        return 0;
    }
    if (c0 == 'd') {
        // Expect "delete"
        if (s.size() < 6) return -1;
        const char* del = "delete";
        for (int i = 0; i < 6; i++)
            if (tolower(s[i]) != del[i]) return -1;
        if (s.size() < 7) return -2; // DELETE needs a key
        return 0;
    }
    return -1;
}

// Returns {command_digit + key, value}.
// Commands: 1=SET, 2=GET, 3=DELETE.
pair<string, vector<uint8_t>> Service::parse_request(const vector<uint8_t>& request) {
    string s;
    for (uint8_t b : request)
        if (b != ' ') s += (char)b;

    int command = 0;
    if (tolower(s[0]) == 's') command = 1;
    else if (tolower(s[0]) == 'g') command = 2;
    else if (tolower(s[0]) == 'd') command = 3;

    string key;
    vector<uint8_t> value;

    if (command == 1 || command == 2) {
        // Skip "set" or "get" (3 chars), then key=value
        bool past_eq = false;
        for (int i = 3; i < (int)s.size(); i++) {
            if (s[i] == '=') { past_eq = true; continue; }
            if (past_eq) value.push_back((uint8_t)s[i]);
            else          key += s[i];
        }
    } else {
        // Skip "delete" (6 chars)
        for (int i = 6; i < (int)s.size(); i++)
            key += s[i];
    }

    // Prefix the command digit onto the key so the router can unpack it.
    key = char('0' + command) + key;
    return {key, value};
}

/*
WAL entry format (all uint32 are big-endian):
  [total_len : 4]  bytes after this field
  [command   : 1]
  [key_len   : 4]
  [key       : key_len]
  [value_len : 4]   only if command == 1
  [value     : value_len]  only if command == 1
*/
vector<uint8_t> Service::transform_to_wal(int command, string key,
                                            vector<uint8_t>& value) {
    vector<uint8_t> res;

    auto push_u32be = [&](uint32_t v) {
        v = htonl(v);
        uint8_t buf[4]; memcpy(buf, &v, 4);
        res.insert(res.end(), buf, buf + 4);
    };

    uint32_t k_size = (uint32_t)key.size();
    uint32_t v_size = (command == 1) ? (uint32_t)value.size() : 0;
    uint32_t total  = 1 + 4 + k_size + (command == 1 ? 4 + v_size : 0);

    push_u32be(total);
    res.push_back((uint8_t)command);
    push_u32be(k_size);
    res.insert(res.end(), key.begin(), key.end());
    if (command == 1) {
        push_u32be(v_size);
        res.insert(res.end(), value.begin(), value.end());
    }
    return res;
}
