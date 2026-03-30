#include<iostream>
#include<sys/socket.h>
#include<netinet/in.h>
#include<unistd.h>
#include<cstring>
#include<netdb.h>
#include<arpa/inet.h>
#include<string>

int main(){
    int sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    char buffer[256];
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(9000);
    serv_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    connect(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr));

    std::string message = "set x = 12\n";
    uint32_t len = htons(message.size());
    write(sockfd, &len, sizeof(len));
    write(sockfd, message.c_str(), message.size());

}