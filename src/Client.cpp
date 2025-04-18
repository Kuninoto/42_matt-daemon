#include <string>

#include "Client.hpp"

Client::Client(int socketfd) {
    this->socketfd = socketfd;
}

Client::Client(const Client &to_copy) {
    // TODO
    (void)to_copy;
};

Client &Client::operator=(const Client &to_copy) {
    // TODO
    (void)to_copy;
    return *this;
};

Client::~Client(void){};
