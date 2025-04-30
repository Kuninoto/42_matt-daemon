#pragma once

#include <string>

class Client {
public:
    Client(int socketfd) noexcept;
    Client(const Client &toCopy) noexcept;
    Client &operator=(const Client &toCopy) noexcept;
    ~Client(void) noexcept;

    int socketfd;
    std::string msg;
};

std::ostream &operator<<(std::ostream &stream, const Client &client) noexcept;
