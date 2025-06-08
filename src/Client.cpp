#include "Client.hpp"

#include <unistd.h>

#include <ostream>
#include <string>

Client::Client(int socketfd) noexcept {
    this->socketfd = socketfd;
}

Client::Client(const Client &rhs) noexcept {
    if (this != &rhs) {
        *this = rhs;
    }
};

Client &Client::operator=(const Client &rhs) noexcept {
    if (this != &rhs) {
        this->socketfd = rhs.socketfd;
        this->msg = rhs.msg;
    }
    return *this;
};

Client::~Client(void) noexcept {
    close(this->socketfd);
};

std::ostream &operator<<(std::ostream &stream, const Client &client) noexcept {
    stream << "socketfd=" << client.socketfd << ", "
           << "msg=" << client.msg;
    return stream;
}
