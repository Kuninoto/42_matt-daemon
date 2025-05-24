#include <csignal>
#include <cstdlib>
#include <dirent.h>
#include <errno.h>
#include <filesystem>
#include <iostream>
#include <memory>
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

std::unique_ptr<Tintin_reporter> g_logger = nullptr; // Global pointer to the logger, we need it as global to be usable on signal handlers

/**
 * Run the calling process as a system daemon. `daemon()` replica.
 *
 * @param nochdir Whether to not change daemon's process working directory to `/`
 * @param noclose Whether to not close daemon's process inherited file descriptors
 *
 * @throws `std::runtime_exception`
 *
 * @see https://man7.org/linux/man-pages/man7/daemon.7.html, SysV Daemons
 * @see https://sandervanderburg.blogspot.com/2020/01/writing-well-behaving-daemon-in-c.html
 */
void ft_daemon(int nochdir, int noclose) {
    // Clean all open file descriptors except standard input, output and error
    int maxFds = 0;

    DIR *dir = opendir("/proc/self/fd");

    if (dir == NULL) {
        // If opening /proc/self/fd failed, fallback to getrlimit()
        struct rlimit rlim;
        if (getrlimit(RLIMIT_NOFILE, &rlim) == -1) {
            // If getrlimit() failed, fallback to FOPEN_MAX
            maxFds = FOPEN_MAX;
        } else {
            maxFds = static_cast<int>(rlim.rlim_cur);
        }

        for (int i = STDERR_FILENO + 1; i < maxFds; i++) {
            close(i);
        }
    } else {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.') {
                continue;
            }

            int fd = atoi(entry->d_name);
            if (fd > STDERR_FILENO) {
                close(fd);
            }
        }

        closedir(dir);
    }

    // Reset all signal handlers to their default
#ifdef NSIG // Not every libc implementation defines NSIG
    for (int i = 1; i < NSIG; i++) {
        if (i != SIGKILL && i != SIGSTOP) {
            std::signal(i, SIG_DFL);
        }
    }
#endif

    sigset_t sigset;
    if (sigemptyset(&sigset) == -1) {
        throw new std::runtime_error(std::string("failed empty signal set: sigemptyset() failed: ") + strerror(errno));
    }

    // Reset all signal masks (unblock) by passing an empty set
    if (sigprocmask(SIG_UNBLOCK, &sigset, NULL) == -1) {
        throw new std::runtime_error(std::string("failed to reset signal masks: sigprocmask() failed: ") + strerror(errno));
    }

    // Create a background process
    pid_t pid = fork();
    if (pid == -1) {
        // Failed to create child process
        throw new std::runtime_error(std::string("failed to create child process: fork() failed: ") + strerror(errno));
    }

    if (pid != 0) {
        // Exit parent process
        exit(EXIT_SUCCESS);
    }

    // Child process...

    // Create a session to detach from any terminal and go to an independent session
    if (setsid() == -1) {
        // Failed to create new session
        throw new std::runtime_error(std::string("failed to create session: setsid() failed: ") + strerror(errno));
    }

    // Fork again to ensure that the daemon can never re-acquire a terminal again
    pid = fork();
    if (pid == -1) {
        // Failed to create grand-child process
        throw new std::runtime_error(std::string("failed to gramd-child process: fork() failed: ") + strerror(errno));
    }

    if (pid != 0) {
        // Exit child process
        exit(EXIT_SUCCESS);
    }

    // Daemon process (grand-child)...

    if (!noclose) {
        // Redirect standard input, output and error to /dev/null
        int fd = open("/dev/null", O_RDWR);
        if (fd == -1) {
            throw new std::runtime_error(std::string("failed to open /dev/null: open() failed: ") + strerror(errno));
        }

        dup2(fd, STDIN_FILENO);
        dup2(fd, STDOUT_FILENO);
        dup2(fd, STDERR_FILENO);

        if (fd > STDERR_FILENO) {
            close(fd);
        }
    }

    // Reset the umask to 0
    umask(0);

    // If nochdir == 0, change the current directory to the root directory
    // to prevent the daemon from involuntarily blocks mount points from being
    // unmounted
    if (!nochdir && chdir("/") == -1) {
        throw new std::runtime_error(std::string("failed to change directory to root: chdir(): ") + strerror(errno));
    }

    // Write the daemon PID to pidfile
    int pidFileFd = open(PIDFILE_PATH, O_CREAT | O_EXCL | O_WRONLY, S_IRUSR | S_IWUSR);
    if (pidFileFd == -1) {
        throw std::runtime_error(std::string("failed to open pid file: open() failed: ") + strerror(errno));
    }
    if (dprintf(pidFileFd, "%d", getpid()) < 0) {
        close(pidFileFd);
        throw std::runtime_error("failed to write to pid file: dprintf() failed");
    }
    close(pidFileFd);
}

int main(void) {
    if (geteuid() != ROOT_UID) {
        std::cerr << "matt-daemon: fatal: root privileges needed\n";
        return EXIT_FAILURE;
    }

    // Daemonize
    try {
        ft_daemon(0, 0);
    } catch (const std::runtime_error &e) {
        std::cerr << "matt-daemon: fatal: failed to daemonize, ft_daemon() failed: " << e.what() << "\n";
        return EXIT_FAILURE;
    }

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

    if (!fs::exists(LOGFILE_DIR_PATH) && !fs::create_directory(LOGFILE_DIR_PATH)) {
        std::cerr << "matt-daemon: fatal: failed to create logfile directory\n";
        return EXIT_FAILURE;
    }

    g_logger = std::make_unique<Tintin_reporter>(LOGFILE_PATH);
    if (!g_logger->isValid()) {
        std::cerr << "matt-daemon: fatal: failed to open logfile\n";
        close(lockfileFd);
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

    g_logger->info("started");

#ifdef _DEBUG
    std::cout << "Setting up signal handlers..." << std::endl;
#endif

    try {
        setupSignalHandlers();
    } catch (const std::runtime_error &e) {
        g_logger->fatal(std::string("failed to setup signal handlers: ") + e.what());
        close(lockfileFd);
        fs::remove(PIDFILE_PATH);
        fs::remove(LOCKFILE_PATH);
        return EXIT_FAILURE;
    }

#ifdef _DEBUG
    std::cout << "Starting server..." << std::endl;
#endif

    int exitStatus = EXIT_SUCCESS;
    try {
        Server server = Server();
        server.start();
    } catch (const std::exception &e) {
        g_logger->fatal(std::string("failed to start server: ") + e.what());
        exitStatus = EXIT_FAILURE;
    }

    g_logger->notice("quitting...");

    close(lockfileFd);
    fs::remove(PIDFILE_PATH);
    fs::remove(LOCKFILE_PATH);
    return exitStatus;
}
