#include "signal.hpp"

#include <csignal>
#include <memory>
#include <stdexcept>

#include "Tintin_reporter.hpp"

extern volatile sig_atomic_t g_run;
extern std::unique_ptr<Tintin_reporter> g_logger;

static constexpr int SIGNALS_TO_HANDLE[]{
    // User’s terminal is disconnected (daemons repurpose this to reload configurations)
    SIGHUP,
    // External interrupt
    SIGINT,
    // Used by debuggers
    SIGTRAP,
    // User defined signals
    SIGUSR1,
    SIGUSR2,
    // Termination request
    SIGTERM,
    // Signal sent to parent process when child process is stopped
    SIGCHLD,
    // Expiration of timer that measures real or clock time, used by function like alarm()
    SIGALRM,
    // Continue the execution
    SIGCONT,
    // Interactive stop request
    SIGTSTP,
    // Reading from terminal is not possible
    SIGTTIN,
    // Writing to terminal is not possible
    SIGTTOU,
    // When urgent, out of band data arrives to a socket
    SIGURG,
    // CPU time limit exceeded
    SIGXCPU,
    // File size limit exceeded
    SIGXFSZ,
    // Same as SIGALRM but with CPU time, not real time
    SIGVTALRM,
    // Used for profilers, useful to generate a log file when the profiler asks
    SIGPROF,
    // Window has been resized
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
        case SIGHUP:
            return "SIGHUP";
        case SIGINT:
            return "SIGINT";
        case SIGTRAP:
            return "SIGTRAP";
        case SIGUSR1:
            return "SIGUSR1";
        case SIGUSR2:
            return "SIGUSR2";
        case SIGTERM:
            return "SIGTERM";
        case SIGCHLD:
            return "SIGCHLD";
        case SIGALRM:
            return "SIGALRM";
        case SIGCONT:
            return "SIGCONT";
        case SIGTSTP:
            return "SIGTSTP";
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
