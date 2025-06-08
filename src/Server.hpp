#pragma once

#include <sys/epoll.h>

#include <array>
#include <memory>
#include <ostream>
#include <vector>

#include "Client.hpp"

class Server {
    static constexpr const char ACK_MSG[] = "ACK\n";
    static constexpr const char CLIENT_REJECTED_MSG[] = "Rejected due to client limit\n";

    static constexpr const int MAX_CLIENTS = 3;
    static constexpr const int MAX_EVENTS = 10;
    static constexpr uint16_t PORT = (uint16_t)4242;
    static constexpr int RECV_BUFFER_SIZE = 1024;

    int epollfd;
    int socketfd;
    std::array<struct epoll_event, Server::MAX_EVENTS> events;
    std::vector<std::unique_ptr<Client>> clients;

    void handleNewConnection(void) noexcept;
    void handleClientMsg(int clientFd) noexcept;

public:
    Server(void);
    Server(Server &rhs) noexcept;
    Server &operator=(Server &rhs) noexcept;
    ~Server(void) noexcept;

    void start(void) noexcept;
};
