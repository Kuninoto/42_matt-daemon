#include <unistd.h>
#include <string>
#include <ostream>

#include <iostream>

#include "Client.hpp"

Client::Client(int socketfd) {
    this->socketfd = socketfd;
}

Client::Client(const Client &to_copy) {
    if (this != &to_copy) {
        *this = to_copy;
    }
};

Client &Client::operator=(const Client &to_copy) {
    if (this != &to_copy) {
        this->socketfd = to_copy.socketfd;
        this->msg = to_copy.msg;
    }
    return *this;
};

Client::~Client(void) {
    close(this->socketfd);
};

std::ostream &operator<<(std::ostream &stream, const Client &client) {
	stream << "socketfd=" << client.socketfd << "\n"
           << "msg=" << client.msg;
	return stream;
}
