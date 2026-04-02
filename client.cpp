#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cstring>
#include<netdb.h>
#include<arpa/inet.h>
#include<string>
#include <vector>
#include "./transport/transport.hpp"

Transport transport;
int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9000);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));

    std::string message = "set x = 12";
    std::vector<uint8_t> buffer(message.begin(), message.end());
    transport.write_data(sockfd, buffer);
    std::cout<<"data sent" << std::endl;
    buffer.clear();
    buffer = transport.read_data(sockfd);
    for(auto & it: buffer){
        std::cout << it;
    }
    std::cout << std::endl;   
}