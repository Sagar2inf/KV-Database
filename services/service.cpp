#include "service.hpp"

// query is valid only if i don't contain any control character and ':' and it should start with set/get/delete
int Service::validate_request(const vector<uint8_t> &request){
    string s = "";
    for(int i = 0; i < request.size(); i++){
        if(int(request[i]) < 32 || int(request[i]) > 126 || int(request[i]) == int(':')){
            return -1;
        }
        if(request[i] != ' ') s += char(request[i]);
    }
    if(s[0] == 's' || s[0] == 'S' || s[0] == 'g' || s[0] == 'G'){
        if(s.size() < 4) return -2;
        if(s[1] != 'e' && s[1] != 'E') return -1;
        if(s[2] != 't' && s[2] != 'T') return -1;
        if((s[0] == 's' || s[0] == 'S') && s.size() < 6) return -2;
        return 0;
    }else if(s[0] == 'd' || s[0] == 'D'){
        if(s.size() < 6) return -1;
        for(int i = 1; i < 6; i++){
            if(s[i] != "delete"[i] && s[i] != "DELETE"[i]) return -1;
        }
        if(s.size() < 7) return -2;
        return 0;
    }else {
        return -1;
    }
}
// commands 1 for set, 2 for get, 3 for delete
// it returns pair of command + key and value for get and delete are empty
pair<string, vector<uint8_t>> Service::parse_request(const vector<uint8_t>& request){
    string s = "";
    for(int i = 0; i < request.size(); i++){
        if(request[i] == ' ') continue;
        s += char(request[i]);
    }
    int command = 0;
    string key = "";
    vector<uint8_t> value;
    if(s[0] == 's' || s[0] == 'S') command = 1;
    else if(s[0] == 'g' || s[0] == 'G') command = 2;
    else if(s[0] == 'd' || s[0] == 'D') command = 3;
    if(command == 1 || command == 2){
        bool f = 0;
        for(int i = 3; i < s.size(); i++){
            if(s[i] == '='){
                f = 1; continue;
            }
            if(f) value.push_back(uint8_t(s[i]));
            else key += s[i];
        }
    }else{
        for(int i = 6; i < s.size(); i++){
            key += s[i];
        }
    }
    key = char('0' + command) + key;
    return {key, value};
}
