#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include "./platform.hpp"
#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  static inline void close_sock(int fd) { closesocket((SOCKET)fd); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <arpa/inet.h>
  static inline void close_sock(int fd) { close(fd); }
#endif
#include "./transport/transport.hpp"
#include "./config.hpp"

int main(int argc, char* argv[]) {
#ifdef _WIN32
    WSADATA wsa; WSAStartup(MAKEWORD(2, 2), &wsa);
#endif
    const char* host = (argc > 1) ? argv[1] : "127.0.0.1";
    int port         = (argc > 2) ? atoi(argv[2]) : config::PORT;

    int sockfd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sockfd < 0) { perror("socket"); return 1; }

    sockaddr_in serv_addr{};
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_port        = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(host);

    if (connect(sockfd, reinterpret_cast<sockaddr*>(&serv_addr),
                sizeof(serv_addr)) < 0) {
        perror("connect"); return 1;
    }
    cout << "Connected to " << host << ":" << port
         << " — commands: SET key=value | GET key | DELETE key | exit\n";

    Transport transport;
    string message;
    while (getline(cin, message)) {
        if (message == "exit") break;
        if (message.empty()) continue;

        // Send message as-is (no trailing newline — service rejects control chars)
        vector<uint8_t> buf(message.begin(), message.end());
        if (!transport.write_data(sockfd, buf)) {
            cerr << "Failed to send\n"; break;
        }

        vector<uint8_t> resp = transport.read_data(sockfd);
        if (resp.empty()) { cerr << "Server disconnected\n"; break; }

        cout << string(resp.begin(), resp.end()) << "\n";
    }

#ifdef _WIN32
    WSACleanup();
#endif
    close_sock(sockfd);
    return 0;
}
