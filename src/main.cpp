#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "Tintin_reporter.hpp"

namespace fs = std::filesystem;

static constexpr int ROOT_UID = 0;
static constexpr uint8_t PORT = (uint8_t)4242;
static constexpr const int MAX_CLIENTS = 3;
static constexpr const char *PIDFILE_PATH = "/var/run/matt_daemon.pid";
static constexpr const char *LOCKFILE_PATH = "/var/lock/matt_daemon.lock";
static constexpr const char *LOGFILE_DIR_PATH = "/var/log/matt_daemon/";
static constexpr const char *LOGFILE_PATH = "/var/log/matt_daemon/matt_daemon.log";

volatile sig_atomic_t g_run = 1; // Global variable to control the main loop

/**
 * Handles SIGINT by setting `g_run` to `0`.
 *
 * @param signum Signal number
 */
static void sigint_handler(int signum) {
    (void)signum;
    g_run = 0;
}

/**
 * Handles SIGTERM by setting `g_run` to `0`.
 *
 * @param signum Signal number
 */
static void sigterm_handler(int signum) {
    (void)signum;
    g_run = 0;
}

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
        signal(i, SIG_DFL);
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
    } catch (std::exception &e) {
        // TODO handle error better
        std::cerr << "matt-daemon: fatal: failed to open pid file: " << e.what();
        return -1;
    }

    return 0;
}

int main(void) {
    if (geteuid() != ROOT_UID) {
        std::cerr << "matt-daemon: fatal: root privileges needed\n";
        return EXIT_FAILURE;
    }

    if (!fs::exists(LOGFILE_DIR_PATH) && !fs::create_directory(LOGFILE_DIR_PATH)) {
        std::cerr << "matt-daemon: fatal: failed to create logfile directory\n";
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

    Tintin_reporter logger(LOGFILE_PATH);
    if (!logger.isValid()) {
        std::cerr << "matt-daemon: fatal: failed to open logfile\n";
        return EXIT_FAILURE;
    }

    /* // Daemonize
    if (ft_daemon(0, 0) == -1) {
        // TODO handle error
        std::cerr << "matt-daemon: fatal: failed to daemonize\n";
        return EXIT_FAILURE;
    } */

    logger.info("started");

    // Register signal handlers
    // Handle SIGINT & SIGTERM, ignore SIGCHLD, SIGPIPE, SIGURG, SIGWINCH, SIGTTOU and SIGTTIN
    // clang-format off
    if (
        signal(SIGINT, sigint_handler) == SIG_ERR
        || signal(SIGTERM, sigterm_handler) == SIG_ERR
        || signal(SIGCHLD, SIG_IGN) == SIG_ERR
        || signal(SIGPIPE, SIG_IGN) == SIG_ERR
        || signal(SIGURG, SIG_IGN) == SIG_ERR
        || signal(SIGWINCH, SIG_IGN) == SIG_ERR
        || signal(SIGTTOU, SIG_IGN) == SIG_ERR
        || signal(SIGTTIN, SIG_IGN) == SIG_ERR
    ) {
        std::cerr << "matt-daemon: fatal: failed to setup signal handlers\n";
        return EXIT_FAILURE;
    }

    int socketfd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    if (bind(socketfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) {
        logger.fatal("failed to bind to port " + std::to_string(PORT) + ": " + strerror(errno));
        return EXIT_FAILURE;
    }

    if (listen(socketfd, MAX_CLIENTS) == -1) {
        logger.fatal("failed to listen on port " + std::to_string(PORT) + ": " + strerror(errno));
        return EXIT_FAILURE;
    }

    // TODO implement client handling - use epoll()?

    while (g_run) {
        int clientSocketFd = accept(socketfd, nullptr, nullptr);
        if (clientSocketFd == -1) {
            // TODO handle error
        }

        // recieving data
        char buffer[1024] = { 0 };
        ssize_t rd = recv(clientSocketFd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (rd == -1) {
            // TODO handle error
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // TODO
            }
            continue;
        } else if (rd == 0) {
            logger.info("peer has shutdown the connection");
            continue;
        }

        if (buffer == "quit") {
            logger.info("received quit requested");
            g_run = 0;
        } else {
            logger.log("received message: " + buffer);
        }
    }

    logger.info("quitting");

    close(socketfd);
    fs::remove(PIDFILE_PATH);
    fs::remove(LOCKFILE_PATH);
    return EXIT_SUCCESS;
}
