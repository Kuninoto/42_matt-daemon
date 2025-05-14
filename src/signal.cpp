#include <csignal>
#include <stdexcept>

#include "Tintin_reporter.hpp"
#include "signal.hpp"

extern volatile sig_atomic_t g_run;
extern Tintin_reporter *g_logger;

static constexpr int SIGNALS_TO_HANDLE[]{
    // User defined signals
    SIGUSR1,
    SIGUSR2,
    // Illegal CPU instruction due to corruption or something else
    SIGILL,
    // Indicates that the program is behaving abnormally, usually raised by the program itself
    SIGABRT,
    // Bus error due to illegal memory access related issues
    SIGBUS,
    // When the program tries to access restricted memory areas
    SIGSEGV,
    // Arithmetic exception due to various arithmetic issues such as division by zero, etc...
    SIGFPE,
    // When the process tries to write to a pipe or a socket which doesn't have a reader
    SIGPIPE,
    // When child process terminates
    SIGCHLD,
    // Used for timers
    SIGALRM,
    // Continue the execution
    SIGCONT,
    // Background process tries to read from console STDIN the OS sends this signal
    SIGTTIN,
    // Background process tries to read from console STDOUT the OS sends this signal
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
 * Gets a signal name from a signal number.
 * E.g. 11 -> "SIGSEV"
 *
 * @param signum Signal number
 */
static const char *getSignalName(int signum) noexcept {
    switch (signum) {
    case SIGUSR1:
        // User defined signals
        return "SIGUSR1";
    case SIGUSR2:
        // User defined signals
        return "SIGUSR2";
    case SIGILL:
        // Illegal CPU instruction due to corruption or something else
        return "SIGILL";
    case SIGABRT:
        // Indicates that the program is behaving abnormally, usually raised by the program itself
        return "SIGABRT";
    case SIGBUS:
        // Bus error due to illegal memory access related issues
        return "SIGBUS";
    case SIGSEGV:
        // When the program tries to access restricted memory areas
        return "SIGSEGV";
    case SIGFPE:
        // Arithmetic exception due to various arithmetic issues such as division by zero, etc...
        return "SIGFPE";
    case SIGPIPE:
        // When the process tries to write to a pipe or a socket which doesn't have a reader
        return "SIGPIPE";
    case SIGCHLD:
        // When child process terminates
        return "SIGCHLD";
    case SIGALRM:
        // Used for timers
        return "SIGALRM";
    case SIGCONT:
        // Continue the execution
        return "SIGCONT";
    case SIGTTIN:
        // Background process tries to read from console STDIN the OS sends this signal
        return "SIGTTIN";
    case SIGTTOU:
        // Background process tries to read from console STDOUT the OS sends this signal
        return "SIGTTOU";
    case SIGURG:
        // When urgent, out of band data arrives to a socket
        return "SIGURG";
    case SIGXCPU:
        // Process has exceeded it's CPU budget
        return "SIGXCPU";
    case SIGXFSZ:
        // Process has exceeded max file size limits
        return "SIGXFSZ";
    case SIGVTALRM:
        // Same as SIGALRM but with CPU time, not real time
        return "SIGVTALRM";
    case SIGPROF:
        // Used for profilers, useful to generate a log file when the profiler asks
        return "SIGPROF";
    case SIGWINCH:
        // Terminal size has changed
        return "SIGWINCH";
    case SIGIO:
        // Async I/O notification
        return "SIGIO";
    case SIGSYS:
        // Bad system call
        return "SIGSYS";
    default:
        return "UNKNOWN";
    }
}

/**
 * Handles various signals by logging a message to the logfile and ignoring them.
 *
 * @param signum Signal number
 */
void badsigHandler(int signum) noexcept {
    (void)signum;
    g_logger->notice(std::string("Received ") + getSignalName(signum) + " ignoring...");
}

/**
 * Handles `SIGINT` by setting `g_run` to `0`.
 *
 * @param signum Signal number
 */
void sigintHandler(int signum) noexcept {
    (void)signum;
    g_run = 0;
    g_logger->notice("Received SIGINT");
}

/**
 * Handles `SIGTERM` by setting `g_run` to `0`.
 *
 * @param signum Signal number
 */
void sigtermHandler(int signum) noexcept {
    (void)signum;
    g_run = 0;
    g_logger->notice("Received SIGTERM");
}

/**
 * Sets various signal handlers.
 *
 * @throws `std::runtime_error` if setting the handler for any signal failed
 */
void setupSignalHandlers(void) {
    if (std::signal(SIGTERM, sigtermHandler) == SIG_ERR) {
        throw std::runtime_error("failed to setup signal handler for SIGTERM");
    }

    for (size_t i = 0; i < sizeof(SIGNALS_TO_HANDLE) / sizeof(int); i += 1) {
        if (std::signal(SIGNALS_TO_HANDLE[i], badsigHandler) == SIG_ERR) {
            throw std::runtime_error(std::string("failed to setup signal handler for ") + getSignalName(SIGNALS_TO_HANDLE[i]));
        }
    }
}
