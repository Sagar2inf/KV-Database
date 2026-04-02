#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cstring>
#include<netdb.h>
#include<arpa/inet.h>
#include<string>
#include <vector>

using namespace std;

class Transport{
public:
    std::vector<uint8_t> read_data(int client);
    bool write_data(int client, const std::vector<uint8_t>& data);
};