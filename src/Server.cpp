#include <algorithm>
#include <csignal>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <netinet/in.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

#include "Server.hpp"
#include "Tintin_reporter.hpp"
#include "signal.hpp"

extern std::unique_ptr<Tintin_reporter> g_logger;

volatile sig_atomic_t g_run = 0; // Global variable to control the server loop

/**
 * @throws `std::runtime_error`
 */
Server::Server(void) {
    if (std::signal(SIGINT, sigintHandler) == SIG_ERR) {
        throw std::runtime_error("failed to setup signal handler for SIGINT");
    }

#ifdef _DEBUG
    std::cout << "Creating server's socket..." << std::endl;
#endif

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        throw std::runtime_error(std::string("failed to create server's socket: socket() failed: ") + strerror(errno));
    }
    this->socketfd = socketfd;

#ifdef _DEBUG
    std::cout << "Binding socket to port " << std::to_string(Server::PORT) << "..." << std::endl;
#endif

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(Server::PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) {
        throw std::runtime_error(std::string("failed to bind to port ") + std::to_string(Server::PORT) + ": " + strerror(errno));
    }

#ifdef _DEBUG
    std::cout << "Setting server's socket to listen..." << std::endl;
#endif

    if (listen(socketfd, MAX_CLIENTS) == -1) {
        throw std::runtime_error(std::string("failed to listen on port ") + std::to_string(Server::PORT) + ": " + strerror(errno));
    }

#ifdef _DEBUG
    std::cout << "Creating epollfd..." << std::endl;
#endif

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        throw std::runtime_error(std::string("failed to create epoll: epoll_create1() failed: ") + strerror(errno));
    }
    this->epollfd = epollfd;

#ifdef _DEBUG
    std::cout << "Adding server's socket to polled fds..." << std::endl;
#endif

    // Add server's socket to the polled fds
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = socketfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
        throw std::runtime_error(std::string("failed to add server's socket fd to polled fds: epoll_ctl() failed: ") + strerror(errno));
    }
}

Server::Server(Server &rhs) noexcept {
    if (this != &rhs) {
        *this = rhs;
    }
}

Server &Server::operator=(Server &rhs) noexcept {
    if (this != &rhs) {
        this->socketfd = rhs.socketfd;
        this->epollfd = rhs.epollfd;
        std::memcpy(this->events.data(), rhs.events.data(), sizeof(rhs.events));
        this->clients = std::move(rhs.clients);
    }
    return *this;
}

Server::~Server(void) noexcept {
    if (this->epollfd != -1) {
        close(this->epollfd);
    }
    if (this->socketfd != -1) {
        close(this->socketfd);
    }

    for (const auto& it : clients) {
        close(it.get()->socketfd);
    }
}

void Server::handleNewConnection(void) noexcept {
#ifdef _DEBUG
    std::cout << "Received event on server's socket, trying to accept client..." << std::endl;
#endif

    // If event is on server's socket fd, accept new client
    int clientSocketFd = accept(this->socketfd, nullptr, nullptr);
    if (clientSocketFd == -1) {
        g_logger->error(std::string("failed to accept client: accept() failed: ") + strerror(errno));
        return;
    }

#ifdef _DEBUG
    std::cout << "Client accepted" << std::endl;
#endif

    if (clients.size() == MAX_CLIENTS) {
        if (send(clientSocketFd, CLIENT_REJECTED_MSG, sizeof(CLIENT_REJECTED_MSG), MSG_DONTWAIT) == -1) {
            g_logger->warn(std::string("failed to send client rejected message: send() failed: ") + strerror(errno));
        }

        close(clientSocketFd);
        g_logger->notice("rejected client due to connections limit");
        return;
    }

    // Set client socket as non-blocking
    int flags = fcntl(clientSocketFd, F_GETFL, 0);
    if (flags == -1) {
        close(clientSocketFd);
        g_logger->error(std::string("failed to get client's socket options: fcntl() failed: ") + strerror(errno));
        return;
    }
    if (fcntl(clientSocketFd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(clientSocketFd);
        g_logger->error(std::string("failed to set client's socket as non-blocking: fcntl() failed: ") + strerror(errno));
        return;
    }

    // Add new client's socket to the polled fds
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = clientSocketFd;
    if (epoll_ctl(this->epollfd, EPOLL_CTL_ADD, clientSocketFd, &ev) == -1) {
        close(clientSocketFd);
        g_logger->error(std::string("failed to add client's socket to epoll() interest list: epoll_ctl() failed: ") + strerror(errno));
        return;
    }

    this->clients.push_back(std::make_unique<Client>(clientSocketFd));

#ifdef _DEBUG
    std::cout << "New client registered, socketfd=" << this->clients.back()->socketfd << std::endl;
#endif
}

void Server::handleClientMsg(int clientFd) noexcept {
    // Find the client associated with this fd
    auto clientIt = std::find_if(
        this->clients.begin(),
        this->clients.end(),
        [clientFd](const std::unique_ptr<Client> &client) { return client->socketfd == clientFd; }
    );

    char buf[RECV_BUFFER_SIZE] = {0};
    ssize_t rd = recv(clientFd, buf, sizeof(buf), MSG_DONTWAIT);
    if (rd == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            g_logger->error(std::string("recv() failed: ") + strerror(errno));
        }
        return;
    } else if (rd == 0) {
#ifdef _DEBUG
    std::cout << "Client socketfd=" << clientFd << " closed the connection" << std::endl;
#endif
        g_logger->info("peer has shutdown the connection");
        if (epoll_ctl(this->epollfd, EPOLL_CTL_DEL, clientFd, nullptr) == -1) {
            g_logger->error(std::string("failed to remove client socket from epoll() interest list: epoll_ctl() failed: ") + strerror(errno));
        }

        close(clientIt->get()->socketfd);
        this->clients.erase(clientIt);
        return;
    }

    std::string msg(buf);
    if (msg == "quit\n") {
        g_logger->info("received quit request");
        g_run = 0;
        return;
    }

    (*clientIt)->msg.append(msg);

    if ((*clientIt)->msg.back() == '\n') {
        // Delete newline
        (*clientIt)->msg.pop_back();

        if (!(*clientIt)->msg.empty()) {
            // If message has text, log it
            g_logger->log(std::string("received message: ") + (*clientIt)->msg);
            (*clientIt)->msg.clear();
        }

        // Send ACK
        if (send(clientFd, ACK_MSG, sizeof(ACK_MSG), MSG_DONTWAIT) == -1) {
            g_logger->warn(std::string("send() failed: ") + strerror(errno));
        }
    }
}

void Server::start(void) noexcept {
    g_run = 1;

    while (g_run) {
        int nfds = epoll_wait(this->epollfd, this->events.data(), Server::MAX_EVENTS, -1);
        if (nfds == -1) {
            if (errno != EINTR) {
                g_logger->error(std::string("failed to wait for events on polled fds: epoll_wait() failed: ") + strerror(errno));
            }
            continue;
        }

        for (int n = 0; n < nfds; n++) {
            if (this->events[n].data.fd == this->socketfd) {
                // Server's socket fd has events: new connections coming in
                handleNewConnection();
            } else {
                // One of the clients fds has events: message coming in
                handleClientMsg(this->events[n].data.fd);
            }
        }
    }
}
