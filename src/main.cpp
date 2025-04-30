#include <csignal>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/epoll.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include <vector>

#include "Client.hpp"
#include "Tintin_reporter.hpp"
#include "signal.hpp"

namespace fs = std::filesystem;

static constexpr int ROOT_UID = 0;
static constexpr uint16_t PORT = (uint16_t)4248;
static constexpr const int MAX_CLIENTS = 3;
static constexpr const int MAX_EVENTS = 10;
static constexpr const char *PIDFILE_PATH = "/var/run/matt_daemon.pid";
static constexpr const char *LOCKFILE_PATH = "/var/lock/matt_daemon.lock";
static constexpr const char *LOGFILE_DIR_PATH = "/var/log/matt_daemon/";
static constexpr const char *LOGFILE_PATH = "/var/log/matt_daemon/matt_daemon.log";

static constexpr int RECV_BUFFER_SIZE = 1024;
static constexpr const char *ACK_MSG = "ACK\n";

volatile sig_atomic_t g_run = 1; // Global variable to control the main loop
Tintin_reporter *g_logger = nullptr; // Global pointer to the logger, we need it as global to be usable on signal handlers

/**
 * Run the calling process as a system daemon. `daemon()` replica.
 *
 * @param nochdir Whether to not change daemon's process working directory to `/`
 * @param noclose Whether to not close daemon's process inherited file descriptors
 *
 * @return `0` on success, `-1` on failure
 */
int ft_daemon(int nochdir, int noclose) {
    // Clean all open file descriptors except standard input, output and error
    // see https://man7.org/linux/man-pages/man7/daemon.7.html
    int maxFds = 0;

    DIR *dir = opendir("/proc/self/fd");
    if (dir == NULL) {
        // If opening /proc/self/fd failed, fallback to getrlimit()
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
            // If getrlimit() failed, fallback to FOPEN_MAX
            maxFds = FOPEN_MAX;
        } else {
            maxFds = (int)rlim.rlim_cur;
        }

        for (int i = STDERR_FILENO + 1; i < maxFds; i++) {
            close(i);
        }
    } else {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            int fd = atoi(entry->d_name);
            if (fd > STDERR_FILENO) {
                close(fd);
            }
        }
    }

    sigset_t sigset;
    sigemptyset(&sigset);

    // Reset all signal handlers to their default and add all signals to a set
    for (int i = 0; i < SIGRTMAX; i++) {
        std::signal(i, SIG_DFL);
        sigaddset(&sigset, i);
    }

    // Reset all signals masks
    sigprocmask(SIG_SETMASK, &sigset, NULL);

    // TODO Sanitize the environment block, removing or resetting
    // environment variables that might negatively impact daemon
    // runtime.

    // Create a background process
    pid_t pid = fork();
    if (pid == -1) {
        // TODO handle error
        // Failed to create child process
        exit(EXIT_FAILURE);
    }

    if (pid != 0) {
        // Exit parent process
        exit(EXIT_SUCCESS);
    }

    // Create a SID to detach from any terminal and
    // create an independent session.
    pid_t sid = setsid();
    if (sid == -1) {
        std::cerr << "matt-daemon: setsid() failed";
        // Failed to create new session
        exit(EXIT_FAILURE);
    }

    // Fork again to ensure that the daemon can never re-acquire a terminal again
    pid = fork();
    if (pid == -1) {
        // TODO handle error
        // Failed to create grand-child process
        std::cerr << "matt-daemon: fork() failed";
        exit(EXIT_FAILURE);
    }

    if (pid != 0) {
        // Exit child process
        exit(EXIT_SUCCESS);
    }

    // Daemon process (grand-child)...

    if (!noclose) {
        int fd = open("/dev/null", O_WRONLY);
        if (fd == -1) {
            // TODO handle error
        }

        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);
    }

    // Reset the umask to 0, so that the file modes passed
    // to open(), mkdir() and suchlike directly control the
    // access mode of the created files and directories.
    umask(0);

    // If nochdir == 0, change the current directory to the
    // root directory (/), in order to avoid that the daemon
    // involuntarily blocks mount points from being unmounted
    if (!nochdir && chdir("/") == -1) {
        // TODO handle error
        // Failed to change directory to root ("/")
        return -1;
    }

    // Write the daemon PID to a PID file
    // to ensure that the daemon cannot
    // be started more than once
    try {
        std::ofstream pidfile(PIDFILE_PATH);
        pidfile << getpid();
        pidfile.close();
    } catch (const std::exception &e) {
        // TODO handle error better
        std::cerr << "matt-daemon: fatal: failed to open pid file: " << e.what();
        return -1;
    }

    return 0;
}

// TODO
// - Review the repetitive close of socket fd and removal of pid + lock files;
// - Review how the error messages are being created i.e. the need to create
// a std::string to concatenate with strerror().

int main(void) {
    if (geteuid() != ROOT_UID) {
        std::cerr << "matt-daemon: fatal: root privileges needed\n";
        return EXIT_FAILURE;
    }

    if (fs::exists(LOCKFILE_PATH)) {
        std::cerr << "matt-daemon: notice: there is one instance running already, exiting...\n";
        return EXIT_SUCCESS;
    }

    std::ofstream lockfile(LOCKFILE_PATH);
    if (!lockfile.is_open()) {
        std::cerr << "matt-daemon: fatal: failed to open lockfile\n";
        return EXIT_FAILURE;
    }

    if (!fs::exists(LOGFILE_DIR_PATH) && !fs::create_directory(LOGFILE_DIR_PATH)) {
        std::cerr << "matt-daemon: fatal: failed to create logfile directory\n";
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }
    
    // Daemonize
    if (ft_daemon(0, 0) == -1) {
        g_logger->fatal("failed to daemonize");
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    Tintin_reporter logger(LOGFILE_PATH);
    if (!logger.isValid()) {
        std::cerr << "matt-daemon: fatal: failed to open logfile\n";
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    g_logger = &logger;
    g_logger->info("started");

    #ifdef _DEBUG
        std::cout << "Setting up signal handlers..." << std::endl;
    #endif

    try {
        setupSignalHandlers();
    } catch (const std::runtime_error& e) {
        g_logger->fatal(std::string("failed to setup signal handlers: ") + e.what());
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    #ifdef _DEBUG
        std::cout << "Creating server's socket..." << std::endl;
    #endif

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketfd == -1) {
        g_logger->fatal(std::string("failed to create server's socket: socket() failed: ") + strerror(errno));
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    #ifdef _DEBUG
        std::cout << "Binding socket to port " << std::to_string(PORT) << "..." << std::endl;
    #endif

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) {
        g_logger->fatal(std::string("failed to bind to port ") + std::to_string(PORT) + ": " + strerror(errno));
        close(socketfd);
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    #ifdef _DEBUG
        std::cout << "Setting server's socket to listen..." << std::endl;
    #endif

    if (listen(socketfd, MAX_CLIENTS) == -1) {
        g_logger->fatal(std::string("failed to listen on port ") + std::to_string(PORT) + ": " + strerror(errno));
        close(socketfd);
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    #ifdef _DEBUG
        std::cout << "Creating epollfd..." << std::endl;
    #endif

    int epollfd = epoll_create1(0);
    if (epollfd == -1) {
        g_logger->fatal(std::string("failed to create epoll: epoll_create1() failed: ") + strerror(errno));
        close(socketfd);
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    #ifdef _DEBUG
        std::cout << "Adding server's socket to polled fds..." << std::endl;
    #endif

    struct epoll_event ev;
    struct epoll_event events[MAX_EVENTS];
    
    ev.events = EPOLLIN;
    ev.data.fd = socketfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
        g_logger->fatal(std::string("failed to add server's socket fd to polled fds: epoll_ctl() failed: ") + strerror(errno));
        close(socketfd);
        close(epollfd);
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    std::vector<Client *> clients;

    #ifdef _DEBUG
        std::cout << "Starting main loop..." << std::endl;
    #endif

    while (g_run) {
        int nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1 && errno != EINTR) {
            // TODO review this
            g_logger->error(std::string("failed to wait for events on polled fds: epoll_wait() failed: ") + strerror(errno));
            close(socketfd);
            close(epollfd);
            fs::remove(PIDFILE_PATH);
            fs::remove(LOCKFILE_PATH);
            return EXIT_FAILURE;
        }

        for (int n = 0; n < nfds; n++) {
            if (events[n].data.fd == socketfd) {
                #ifdef _DEBUG
                    std::cout << "Received event on server's socket..." << std::endl;
                #endif

                // If event is on server's socket fd, accept new client
                int clientSocketFd = accept(socketfd, nullptr, nullptr);
                if (clientSocketFd == -1) {
                    // TODO review this
                    g_logger->error(std::string("failed to accept client: accept() failed: ") + strerror(errno));
                    continue;
                }

                if (clients.size() == 3) {
                    close(clientSocketFd);
                    g_logger->notice("rejected client due to connections limit");
                    continue;
                }

                // Set client socket as non-blocking
                int flags = fcntl(clientSocketFd, F_GETFL, 0);
                if (flags == -1) {
                    // TODO review this
                    logger.error(std::string("failed to get client's socket options: fcntl() failed: ") + strerror(errno));
                    close(clientSocketFd);
                    continue;
                }
                if (fcntl(clientSocketFd, F_SETFL, flags | O_NONBLOCK) == -1) {
                    // TODO review this
                    g_logger->error(std::string("failed to set client's socket as non-blocking: fcntl() failed: ") + strerror(errno));
                    close(clientSocketFd);
                    continue;
                }

                ev.events = EPOLLIN;
                ev.data.fd = clientSocketFd;
                // Add new client's socket to the polled fds
                if (epoll_ctl(epollfd, EPOLL_CTL_ADD, clientSocketFd, &ev) == -1) {
                    // TODO review this
                    g_logger->error(std::string("failed to add client's socket to epoll() interest list: epoll_ctl() failed: ") + strerror(errno));
                    close(clientSocketFd);
                    continue;
                }

                Client *client = new Client(clientSocketFd);
                clients.push_back(client);

                #ifdef _DEBUG
                    std::cout << "New client registered, socketfd=" << clients.back()->socketfd << std::endl;
                #endif
            } else {
                // One of the polled fds has data to be read
                #ifdef _DEBUG
                    std::cout << "One of the polled fds has data to be read..." << std::endl;
                #endif

                std::vector<Client*>::iterator clientIt = clients.end();
                for (auto it = clients.begin(); it != clients.end(); ++it) {
                    if ((*it)->socketfd == events[n].data.fd) {
                        clientIt = it;
                        break;
                    }
                }

                // If client not found, skip this event
                if (clientIt == clients.end()) {
                    std::cout << "Warning: Event for unknown client, fd=" << events[n].data.fd << std::endl;
                    /* // Remove from epoll and close it
                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, clientFd, &ev) == -1) {
                        std::cout << "Failed to remove unknown fd from epoll" << std::endl;
                    }
                    close(clientFd); */
                    continue;
                }

                char buf[RECV_BUFFER_SIZE] = { 0 };
                ssize_t rd = recv(events[n].data.fd, buf, sizeof(buf), MSG_DONTWAIT);
                if (rd == -1) {
                    std::cout << "recv == -1" << std::endl;
                    // TODO handle error
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // TODO
                    }
                    continue;
                } else if (rd == 0) {
                    g_logger->info("peer has shutdown the connection");
                    if (epoll_ctl(epollfd, EPOLL_CTL_DEL, events[n].data.fd, &ev) == -1) {
                        g_logger->error(std::string("failed to remove client socket from epoll() interest list: epoll_ctl() failed: ") + strerror(errno));
                        // TODO
                    }

                    delete *clientIt;
                    clients.erase(clientIt);
                    continue;
                }

                std::string msg(buf);
                if (msg == "quit\n") {
                    g_logger->info("received quit request");
                    g_run = 0;
                } else {
                    (*clientIt)->msg.append(msg);

                    if ((*clientIt)->msg.back() == '\n') {
                        // Delete newline and log msg
                        (*clientIt)->msg.pop_back();

                        if (!(*clientIt)->msg.empty()) {
                            // If message was more than just a newline, log it
                            g_logger->log(std::string("received message: ") + (*clientIt)->msg);
                            (*clientIt)->msg.clear();
                        }

                        if (send(events[n].data.fd, ACK_MSG, sizeof(ACK_MSG), MSG_DONTWAIT) == -1) {
                            g_logger->warn(std::string("send() failed: ") + strerror(errno));
                        }
                    }
                }
            }
        }
    }

    g_logger->info("quitting");
   
    // Send FIN to clients and delete them from the vector
    for (auto it = clients.begin(); it != clients.end(); ++it) {
        close((*it)->socketfd);
        delete *it;
    }

    close(socketfd);
    close(epollfd);

    fs::remove(PIDFILE_PATH);
    fs::remove(LOCKFILE_PATH);
    return EXIT_SUCCESS;
}
