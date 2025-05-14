#include <csignal>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <filesystem>
#include <iostream>
#include <string.h>
#include <string>
#include <sys/file.h>
#include <sys/resource.h>
#include <sys/stat.h>

#include "Client.hpp"
#include "Server.hpp"
#include "Tintin_reporter.hpp"
#include "signal.hpp"

namespace fs = std::filesystem;

static constexpr int ROOT_UID = 0;

static constexpr const char *PIDFILE_PATH = "/var/run/matt_daemon.pid";
static constexpr const char *LOCKFILE_PATH = "/var/lock/matt_daemon.lock";
static constexpr const char *LOGFILE_DIR_PATH = "/var/log/matt_daemon/";
static constexpr const char *LOGFILE_PATH = "/var/log/matt_daemon/matt_daemon.log";

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
    for (int i = 0; i < NSIG; i++) {
        std::signal(i, SIG_DFL);
        sigaddset(&sigset, i);
    }

    // Unblock all signals to ensure they're all unblocked on the daemon process
    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1) {
        // TODO handle error better
        std::cerr << "matt-daemon: sigprocmask() failed" << strerror(errno);
        return -1;
    }

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

    // Create a session to detach from any terminal and
    // go to an independent session.
    if (setsid() == -1) {
        // Failed to create new session
        std::cerr << "matt-daemon: setsid() failed" << strerror(errno);
        exit(EXIT_FAILURE);
    }

    // Fork again to ensure that the daemon can never re-acquire a terminal again
    pid = fork();
    if (pid == -1) {
        // TODO handle error
        // Failed to create grand-child process
        std::cerr << "matt-daemon: fork() failed" << strerror(errno);
        exit(EXIT_FAILURE);
    }

    if (pid != 0) {
        // Exit child process
        exit(EXIT_SUCCESS);
    }

    // Daemon process (grand-child)...

    if (!noclose) {
        int fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            // TODO handle error
            return -1;
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

    return 0;
}

// TODO
// - (!) Refactor main;
// - Garantir que não há leaks de fds e mem;

// - Review the repetitive close of socket fd and removal of pid + lock files;
// - Review all TODOs

int main(void) {
    if (geteuid() != ROOT_UID) {
        std::cerr << "matt-daemon: fatal: root privileges needed\n";
        return EXIT_FAILURE;
    }

    /* // Daemonize
    if (ft_daemon(0, 0) == -1) {
        g_logger->fatal("failed to daemonize");
        return EXIT_FAILURE;
    } */

    int lockfileFd = open(LOCKFILE_PATH, O_CREAT);
    if (lockfileFd == -1) {
        std::cerr << "matt-daemon: fatal: failed to open lock file: " << strerror(errno) << "\n";
        return EXIT_FAILURE;
    }
    if (flock(lockfileFd, LOCK_EX | LOCK_NB) == -1) {
        close(lockfileFd);
        if (errno == EWOULDBLOCK) {
            std::cout << "matt-daemon: notice: there is an instance running already, exiting..." << std::endl;
            return EXIT_SUCCESS;
        } else {
            std::cerr << "matt-daemon: fatal: failed to lock pid file\n";
            return EXIT_FAILURE;
        }
    }

    int pidFileFd = open(PIDFILE_PATH, O_CREAT | O_WRONLY);
    if (pidFileFd == -1) {
        close(lockfileFd);
        std::cerr << "matt-daemon: fatal: failed to open pid file: " << strerror(errno) << "\n";
        return EXIT_FAILURE;
    }
    if (flock(pidFileFd, LOCK_EX | LOCK_NB) == -1) {
        close(lockfileFd);
        close(pidFileFd);
        std::cerr << "matt-daemon: fatal: failed to lock pid file\n";
        return EXIT_FAILURE;
    }
    if (dprintf(pidFileFd, "%d", getpid()) < 0) {
        close(lockfileFd);
        close(pidFileFd);
        std::cerr << "matt-daemon: fatal: failed to write to pid file\n";
        return EXIT_FAILURE;
    }

    if (!fs::exists(LOGFILE_DIR_PATH) && !fs::create_directory(LOGFILE_DIR_PATH)) {
        std::cerr << "matt-daemon: fatal: failed to create logfile directory\n";
        return EXIT_FAILURE;
    }

    Tintin_reporter logger(LOGFILE_PATH);
    if (!logger.isValid()) {
        std::cerr << "matt-daemon: fatal: failed to open logfile\n";
        close(pidFileFd);
        close(lockfileFd);
        fs::remove(PIDFILE_PATH);
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
    } catch (const std::runtime_error &e) {
        g_logger->fatal(std::string("failed to setup signal handlers: ") + e.what());
        close(pidFileFd);
        close(lockfileFd);
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

#ifdef _DEBUG
    std::cout << "Starting server..." << std::endl;
#endif

    try {
        Server server = Server();
        server.start();
    } catch (const std::exception &e) {
        g_logger->fatal(e.what());
        // TODO exit with EXIT_FAILURE
    }

    g_logger->notice("quitting...");

    close(pidFileFd);
    close(lockfileFd);
    fs::remove(PIDFILE_PATH);
    fs::remove(LOCKFILE_PATH);
    return EXIT_SUCCESS;
}
