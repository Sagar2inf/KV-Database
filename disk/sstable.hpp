#pragma once
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <unistd.h>
#include <fcntl.h>

using namespace std;


class SSTable{
private:
    string name;
    string dir;
    int fr, fw; 
    int size_per_page;
    int num_pages;
public:
    SSTable(string dir){
        
    }

}