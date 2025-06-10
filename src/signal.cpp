#include "signal.hpp"

#include <csignal>
#include <memory>
#include <stdexcept>

#include "Tintin_reporter.hpp"

extern volatile sig_atomic_t g_run;
extern std::unique_ptr<Tintin_reporter> g_logger;

static constexpr int SIGNALS_TO_HANDLE[]{
    // User defined signals
    SIGUSR1,
    SIGUSR2,
    // Interrupt a process
    SIGINT,
    // Illegal CPU instruction due to corruption or something else
    SIGILL,
    // Indicates that the program is behaving abnormally, usually raised by the
    // program itself
    SIGABRT,
    // Bus error due to illegal memory access related issues
    SIGBUS,
    // When the program tries to access restricted memory areas
    SIGSEGV,
    // Arithmetic exception due to various arithmetic issues such as division by
    // zero, etc...
    SIGFPE,
    // When the process tries to write to a pipe or a socket which doesn't have
    // a reader
    SIGPIPE,
    // Termination request
    SIGTERM,
    // When child process terminates
    SIGCHLD,
    // Used for timers
    SIGALRM,
    // Continue the execution
    SIGCONT,
    // Background process tries to read from console STDIN the OS sends this
    // signal
    SIGTTIN,
    // Background process tries to read from console STDOUT the OS sends this
    // signal
    SIGTTOU,
    // When urgent, out of band data arrives to a socket
    SIGURG,
    // Process has exceeded it's CPU budget
    SIGXCPU,
    // Process has exceeded max file size limits
    SIGXFSZ,
    // Same as SIGALRM but with CPU time, not real time
    SIGVTALRM,
    // Used for profilers, useful to generate a log file when the profiler asks
    SIGPROF,
    // Terminal size has changed
    SIGWINCH,
    // Async I/O notification
    SIGIO,
    // Bad system call
    SIGSYS,
};

/**
 * Gets the name for the specified signal number, e.g. 11 -> "SIGSEV".
 *
 * @param signum Signal number
 */
static const char *getSignalName(int signum) noexcept {
    switch (signum) {
        case SIGUSR1:
            return "SIGUSR1";
        case SIGUSR2:
            return "SIGUSR2";
        case SIGINT:
            return "SIGINT";
        case SIGILL:
            return "SIGILL";
        case SIGABRT:
            return "SIGABRT";
        case SIGBUS:
            return "SIGBUS";
        case SIGSEGV:
            return "SIGSEGV";
        case SIGFPE:
            return "SIGFPE";
        case SIGPIPE:
            return "SIGPIPE";
        case SIGTERM:
            return "SIGTERM";
        case SIGCHLD:
            return "SIGCHLD";
        case SIGALRM:
            return "SIGALRM";
        case SIGCONT:
            return "SIGCONT";
        case SIGTTIN:
            return "SIGTTIN";
        case SIGTTOU:
            return "SIGTTOU";
        case SIGURG:
            return "SIGURG";
        case SIGXCPU:
            return "SIGXCPU";
        case SIGXFSZ:
            return "SIGXFSZ";
        case SIGVTALRM:
            return "SIGVTALRM";
        case SIGPROF:
            return "SIGPROF";
        case SIGWINCH:
            return "SIGWINCH";
        case SIGIO:
            return "SIGIO";
        case SIGSYS:
            return "SIGSYS";
        default:
            return "UNKNOWN";
    }
}

/**
 * Handles various signals by logging a message to the logfile and,
 * in case of `SIGINT` or `SIGTERM`, setting `g_run` to `0`. Otherwise
 * the signal is ignored.
 *
 * @param signum Signal number
 */
void sigHandler(int signum) noexcept {
    std::string msg = "Received ";
    msg += getSignalName(signum);

    if (signum == SIGINT || signum == SIGTERM) {
        g_run = 0;
    } else {
        msg += ", ignoring...";
    }

    g_logger->notice(msg);
}

/**
 * Sets various signal handlers.
 *
 * @throws `std::runtime_error` if setting the handler for any signal failed
 */
void setupSignalHandlers(void) {
    for (size_t i = 0; i < sizeof(SIGNALS_TO_HANDLE) / sizeof(int); i += 1) {
        if (std::signal(SIGNALS_TO_HANDLE[i], sigHandler) == SIG_ERR) {
            throw std::runtime_error(std::string("failed to setup signal handler for ") + getSignalName(SIGNALS_TO_HANDLE[i]));
        }
    }
}
