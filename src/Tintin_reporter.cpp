#include "Tintin_reporter.hpp"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

Tintin_reporter::Tintin_reporter(const std::string &logfilePath) noexcept {
    this->logfilePath = logfilePath;
    this->logfile.open(logfilePath, std::ios::out | std::ios::app);
};

Tintin_reporter::Tintin_reporter(const Tintin_reporter &rhs) noexcept {
    if (this != &rhs) {
        *this = rhs;
    }
};

Tintin_reporter &Tintin_reporter::operator=(const Tintin_reporter &rhs) noexcept {
    if (this != &rhs) {
        this->logfilePath = rhs.logfilePath;
        this->logfile.open(rhs.logfilePath, std::ios::out);
    }
    return *this;
};

Tintin_reporter::~Tintin_reporter(void) noexcept {};

/**
 * @return Whether `Tintin_reporter` was successfully constructed (if it was able to open the logfile)
 */
bool Tintin_reporter::isValid(void) const noexcept {
    return this->logfile.is_open();
}

/**
 * Log level logs.
 *
 * @param msg The message to log
 */
void Tintin_reporter::log(const std::string &msg) noexcept {
    this->_log(LogLevel::LOG, msg);
};

/**
 * Notice level logs.
 *
 * @param msg The message to log
 */
void Tintin_reporter::notice(const std::string &msg) noexcept {
    this->_log(LogLevel::NOTICE, msg);
};

/**
 * Info level logs.
 *
 * @param msg The message to log
 */
void Tintin_reporter::info(const std::string &msg) noexcept {
    this->_log(LogLevel::INFO, msg);
};

/**
 * Warn level logs.
 *
 * @param msg The message to log
 */
void Tintin_reporter::warn(const std::string &msg) noexcept {
    this->_log(LogLevel::WARN, msg);
};

/**
 * Error level logs.
 *
 * @param msg The message to log
 */
void Tintin_reporter::error(const std::string &msg) noexcept {
    this->_log(LogLevel::ERROR, msg);
};

/**
 * Fatal level logs.
 *
 * @param msg The message to log
 */
void Tintin_reporter::fatal(const std::string &msg) noexcept {
    this->_log(LogLevel::FATAL, msg);
};

/**
 * Gets the current timestamp and formats it as day/month/year
 * hour:minute:second.
 *
 * @return A string containing the timestamp
 */
const std::string Tintin_reporter::getTimestamp(void) const noexcept {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    struct tm time_info;
    localtime_r(&time_t_now, &time_info);

    std::stringstream ss;
    ss << std::put_time(&time_info, "%d/%m/%Y %H:%M:%S");
    return ss.str();
}

/**
 * Internal log function. Chooses the corresponding string
 * for the log level `level` and logs `msg` to the logfile
 * alongside a timestamp in a predefined format.
 * Example: [25/04/2025 03:05:54] [INFO] matt-daemon: started
 *
 * @param level Log level
 * @param msg The message to log
 */
void Tintin_reporter::_log(LogLevel level, const std::string &msg) noexcept {
    if (!logfile.is_open()) {
        return;
    }

    const char *levelStr;

    switch (level) {
        case LogLevel::LOG:
            levelStr = "LOG";
            break;
        case LogLevel::NOTICE:
            levelStr = "NOTICE";
            break;
        case LogLevel::INFO:
            levelStr = "INFO";
            break;
        case LogLevel::WARN:
            levelStr = "WARN";
            break;
        case LogLevel::ERROR:
            levelStr = "ERROR";
            break;
        case LogLevel::FATAL:
            levelStr = "FATAL";
            break;
        default:
            levelStr = "UNKNOWN LEVEL";
            break;
    }

    this->logfile << "[" << this->getTimestamp() << "] "
                  << "[" << levelStr << "] " << this->LOG_PREFIX << " " << msg << std::endl;
}
