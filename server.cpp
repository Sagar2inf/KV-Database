#include <iostream>
#include <cstring>
#include <csignal>
#include <vector>
#include <filesystem>
#include "./platform.hpp"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using sock_t   = SOCKET;
  using nfds_t   = ULONG;
  #define poll(fds, nfds, to) WSAPoll((fds), (nfds), (to))
  #ifndef POLLRDHUP
    #define POLLRDHUP 0
  #endif
  static inline void close_sock(sock_t s) { closesocket(s); }
  static inline sock_t invalid_sock() { return INVALID_SOCKET; }
  static inline bool   sock_ok(sock_t s) { return s != INVALID_SOCKET; }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <poll.h>
  #include <unistd.h>
  using sock_t = int;
  static inline void   close_sock(sock_t s) { close(s); }
  static inline sock_t invalid_sock() { return -1; }
  static inline bool   sock_ok(sock_t s) { return s >= 0; }
#endif

#include "./routers/router.hpp"
#include "./transport/transport.hpp"
#include "./config.hpp"

namespace fs = filesystem;

static volatile sig_atomic_t g_stop = 0;
static void on_signal(int) { g_stop = 1; }

int main() {
#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    fs::create_directories(config::WAL_DIR);
    fs::create_directories(config::SST_DIR);

#ifndef _WIN32
    struct sigaction sa{};
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGINT,  &sa, nullptr);
    signal(SIGPIPE, SIG_IGN);
#else
    signal(SIGTERM, on_signal);
    signal(SIGINT,  on_signal);
#endif

    Router    router;
    Transport transport;

    sock_t server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (!sock_ok(server_fd)) { cerr << "socket() failed\n"; return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(config::PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        cerr << "bind() failed\n"; return 1;
    }
    if (listen(server_fd, config::BACKLOG) < 0) {
        cerr << "listen() failed\n"; return 1;
    }

    router.recover();
    cout << "Server listening on port " << config::PORT << "\n";

    // pollfd.fd is SOCKET on Windows, int on Linux — use sock_t throughout.
    struct PollEntry { sock_t fd; short events; short revents; };
    auto to_pollfd = [](const PollEntry& e) -> pollfd {
        pollfd p{};
        p.fd      = e.fd;
        p.events  = e.events;
        p.revents = e.revents;
        return p;
    };

    vector<PollEntry> clients;
    clients.push_back({server_fd, POLLIN, 0});

    while (!g_stop) {
        // Build pollfd array for this iteration.
        vector<pollfd> fds;
        fds.reserve(clients.size());
        for (auto& c : clients) fds.push_back(to_pollfd(c));

        int ready = poll(fds.data(), (nfds_t)fds.size(), 1000 /*ms*/);
        if (ready < 0) {
#ifndef _WIN32
            if (errno == EINTR) continue;
#endif
            cerr << "poll() error\n"; break;
        }

        // Copy revents back.
        for (size_t i = 0; i < clients.size(); i++)
            clients[i].revents = fds[i].revents;

        // New connection on listening socket.
        if (clients[0].revents & POLLIN) {
            sockaddr_in cli_addr{};
            socklen_t   cli_len = sizeof(cli_addr);
            sock_t cli_fd = accept(server_fd,
                                   reinterpret_cast<sockaddr*>(&cli_addr), &cli_len);
            if (sock_ok(cli_fd)) {
                if ((int)clients.size() - 1 >= config::MAX_CLIENTS) {
                    cerr << "Max clients reached; rejecting\n";
                    close_sock(cli_fd);
                } else {
                    cout << "Client connected fd=" << cli_fd << "\n";
                    clients.push_back({cli_fd, POLLIN, 0});
                }
            }
        }

        // Handle existing clients (backwards so erase is safe).
        for (int i = (int)clients.size() - 1; i >= 1; i--) {
            short rev = clients[i].revents;
            if (!(rev & (POLLIN | POLLERR | POLLHUP))) continue;

            if (rev & (POLLERR | POLLHUP)) {
                close_sock(clients[i].fd);
                clients.erase(clients.begin() + i);
                continue;
            }

            vector<uint8_t> data = transport.read_data((int)clients[i].fd);
            if (data.empty()) {
                cout << "Client disconnected fd=" << clients[i].fd << "\n";
                close_sock(clients[i].fd);
                clients.erase(clients.begin() + i);
                continue;
            }

            vector<uint8_t> resp = router.pass_data(data);
            if (!transport.write_data((int)clients[i].fd, resp)) {
                cerr << "Write failed; closing fd=" << clients[i].fd << "\n";
                close_sock(clients[i].fd);
                clients.erase(clients.begin() + i);
            }
        }
    }

    cout << "\nShutting down...\n";
    router.flush_memtable();
    for (int i = 1; i < (int)clients.size(); i++) close_sock(clients[i].fd);
    close_sock(server_fd);
#ifdef _WIN32
    WSACleanup();
#endif
    cout << "Goodbye.\n";
    return 0;
}
