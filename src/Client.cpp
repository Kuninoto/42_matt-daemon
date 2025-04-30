#include <unistd.h>
#include <string>
#include <ostream>

#include <iostream>

#include "Client.hpp"

Client::Client(int socketfd) noexcept {
    this->socketfd = socketfd;
}

Client::Client(const Client &toCopy) noexcept {
    if (this != &toCopy) {
        *this = toCopy;
    }
};

Client &Client::operator=(const Client &toCopy) noexcept {
    if (this != &toCopy) {
        this->socketfd = toCopy.socketfd;
        this->msg = toCopy.msg;
    }
    return *this;
};

Client::~Client(void) noexcept {
    close(this->socketfd);
};

std::ostream &operator<<(std::ostream &stream, const Client &client) noexcept {
	stream << "socketfd=" << client.socketfd << "\n"
           << "msg=" << client.msg;
	return stream;
}
