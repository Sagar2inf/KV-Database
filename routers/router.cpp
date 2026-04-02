#include "router.hpp"

vector<uint8_t> Router::pass_data(const vector<uint8_t>& request){
    int validation = service->validate_request(request);


    if(validation < 0){
        string s = "bad request";
        return vector<uint8_t>(s.begin(), s.end());
    }
    pair<string, vector<uint8_t>> parsed = service->parse_request(request);
    command = int(parsed.first[0] - '0');
    key = parsed.first.substr(1);
    value = parsed.second;
    // debug
    cout << command << " " << key << endl;
    for(auto & it: value){
        cout << it;
    }
    cout << endl;
    bool res = memtable->set_wal(request);
    if(!res){
        string s = "failed to write data to wal";
        return vector<uint8_t>(s.begin(), s.end());
    }
    if(command == 1){
        cout << "set command" << endl;
        // set command
        bool res1 = memtable->set_skiplist(key, value);
        // call mmtable for insert data
        if(!res1){
            string s = "failed to set data";
            return vector<uint8_t>(s.begin(), s.end());
        }else{
            string s = "data set successfully";
            return vector<uint8_t>(s.begin(), s.end());
        }
    }else if(command == 2){
        cout << "get command" << endl;
        // get command
        vector<uint8_t> res1 = memtable->get(key);
        // call mmtable for get data
        return res1;
    }else if(command == 3){
        cout << "delete command" << endl;
        // delete command
        bool res1 = memtable->remove(key);
        // call mmtable for delete data
        return {};
    }else{
        string s = "no query possible";
        return vector<uint8_t>(s.begin(), s.end());
    }
}