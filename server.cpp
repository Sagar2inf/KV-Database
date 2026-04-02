#include<iostream>
#include<cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <vector>
#include <unistd.h>
#include <string>
#include <netdb.h>
#include "./routers/router.hpp"
#include "./transport/transport.hpp"

Router router;
Transport transport;
int main(){
    int sockfd, portno = 9000, newsockfd;
    sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0){
        std::cout<<"failed to start socket" << std::endl;
    }else{
        std::cout<<"Socket is started" << std::endl;
    }
    struct sockaddr_in serv_addr, cli_addr;
    memset((char*)&serv_addr, 0, sizeof(serv_addr));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(portno);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    
    if(bind(sockfd, (sockaddr*)&serv_addr, sizeof(serv_addr))){
        std::cout<<"Failed to bind socket with address" << std::endl;
    }else{
        std::cout<<"Socket bind with address" << std::endl;
    }
    if(listen(sockfd, 5)){
        std::cout<<"Failed to listening" << std::endl;
    }else{
        std::cout<<"Started to listening on local port 9000" << std::endl;
    }
    
    std::vector<int> clients;
    while(true){
        fd_set fr, fw, fe;
        FD_ZERO(&fr);
        FD_ZERO(&fw);
        FD_SET(sockfd, &fr);
        FD_SET(sockfd, &fw);
        int max_fd = sockfd;
        for(int client: clients){
            FD_SET(client, &fr);
            FD_SET(client, &fw);
            max_fd = std::max(max_fd, client);
        }
        select(max_fd + 1, &fr, &fw, NULL, NULL);
        if(FD_ISSET(sockfd, &fr)){
            socklen_t clilen = sizeof(cli_addr);
            newsockfd = accept(sockfd, (sockaddr*)&cli_addr, &clilen);
            clients.push_back(newsockfd);
            std::cout << "new client connected " << newsockfd << std::endl;
        }
        for(auto it = clients.begin(); it != clients.end();){
            int client = *it;
            if(FD_ISSET(client, &fr)){
                std::vector<uint8_t> data = transport.read_data(client);
                if(data.empty()){
                    cout << "client disconnected" << endl;
                    close(client);
                    it = clients.erase(it);
                    continue;
                }    
                
                std::vector<uint8_t> res = router.pass_data(data);
                cout << "writing data" << endl;
                bool chk = transport.write_data(client, res);
                if(!chk){
                    cout << "Couldn't sent data" << endl;
                }
                cout << "data written" << endl;
            }
            it++;
        }
    }
}
// https://www.linkedin.com/posts/quantitative-finance-cohort-25_roadmap-for-learning-low-latency-c-activity-7301555373499367424-dIg8/

/*
structure of sockaddr_in: 
struct sockaddr_in{
    sin_family = AF_INET; -> more common
    sin_port = htons(port); -> port on which server is running 
    struct sin_addr s_addr = INADDR_ANY or 27.0.0.01; -> host name
    and an array which is generally stay empty
}

htons: -> learn about endian concept where (htons and ntohs) are used.
 

fd_set -> if there are multiple client then store their fd in binary so that no client have to wait for their turn manually

everysocket is default blocking socket, learn about non-blocking sockets also
*/



