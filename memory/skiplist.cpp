#include "skiplist.hpp"

double getRandom() {
    static random_device rd;
    static mt19937 gen(rd());
    static uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gen);
}


int Skiplist::randomLevel(){
    int lvl = 1;
    while(getRandom() < p && lvl < maxlevel){
        lvl += 1;
    }
    return lvl;
}

void Skiplist::insert(string key, vector<uint8_t> val) {
    Element* curr = head;
    vector<Element*> update(maxlevel, nullptr);

    for (int i = level - 1; i >= 0; i--) {
        while (curr->next[i] && curr->next[i]->Key < key) {
            curr = curr->next[i];
        }
        update[i] = curr;
    }

    if(curr->next[0] && curr->next[0]->Key == key){
        curr = curr->next[0];
        if(curr->Tombstone) size += 1;
        curr->Value = val;
        curr->Tombstone = false;
        return;
    }

    int lvl = randomLevel();
    if(lvl > level){
        for(int i = level; i < lvl; i++){
            update[i] = head;
        }
        level = lvl;
    }
    Element* node = new Element();
    node->Key = key;
    node->Value = val;
    node->next.resize(lvl, nullptr);
    size += 1;

    for (int i = 0; i < lvl; i++) {
        node->next[i] = update[i]->next[i];
        update[i]->next[i] = node;
    }
}
void Skiplist::remove(string key){
    Element* curr = head;
    for(int i = level - 1; i >= 0; i--){
        while(curr->next[i] && curr->next[i]->Key < key){
            curr = curr->next[i];
        }
    }
    curr = curr->next[0];
    if(curr && curr->Key == key && !curr->Tombstone){
        curr->Tombstone = true;
        size -= 1;
    }
}
vector<uint8_t> Skiplist::get(string key){
    Element* curr = head;
    for(int i = level - 1; i >= 0; i--){
        while(curr->next[i] && curr->next[i]->Key < key){
            curr = curr->next[i];
        }
    }
    curr = curr->next[0];
    if(curr && curr->Key == key && !curr->Tombstone){
        return curr->Value; 
    }else{
        return {};
    }
}


// int main() {
//     Skiplist* s = new Skiplist();
//     vector<uint8_t> chk;
//     chk.push_back(8);
//     s->insert("first", chk);
//     chk.push_back(4);
//     s->insert("second", chk);
//     chk.push_back(6);
//     s->insert("first", chk);
//     cout << s->size << endl;
//     vector<uint8_t> val = s->get("first");
//     for(auto & it: val){
//         cout << int(it) << " ";
//     }
//     cout << endl;

//     return 0;
// }