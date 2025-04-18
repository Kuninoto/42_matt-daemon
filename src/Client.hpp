#pragma once

#include <string>

class Client {
public:
    Client(int socketfd);
    Client(const Client &to_copy);
    Client &operator=(const Client &to_copy);
    ~Client(void);

    int socketfd;
    std::string msg;
};
