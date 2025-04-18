#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

#include "Tintin_reporter.hpp"

Tintin_reporter::Tintin_reporter(const std::string &logfilePath) {
    this->_isValid = true;
    this->logfile.open(logfilePath, std::ios::out | std::ios::app);
    if (!this->logfile.is_open()) {
        this->_isValid = false;
    }
};

Tintin_reporter::Tintin_reporter(const Tintin_reporter &to_copy) {
    // TODO
    (void)to_copy;
};

Tintin_reporter &Tintin_reporter::operator=(const Tintin_reporter &to_copy) {
    // TODO
    (void)to_copy;
    return *this;
};

Tintin_reporter::~Tintin_reporter(void){};

bool Tintin_reporter::isValid(void) {
    return this->_isValid;
}

void Tintin_reporter::debug(const std::string &msg) {
    this->_log(LogLevel::DEBUG, msg);
};

void Tintin_reporter::log(const std::string &msg) {
    this->_log(LogLevel::LOG, msg);
};

void Tintin_reporter::notice(const std::string &msg) {
    this->_log(LogLevel::NOTICE, msg);
};

void Tintin_reporter::info(const std::string &msg) {
    this->_log(LogLevel::INFO, msg);
};

void Tintin_reporter::error(const std::string &msg) {
    this->_log(LogLevel::ERROR, msg);
};

void Tintin_reporter::fatal(const std::string &msg) {
    this->_log(LogLevel::FATAL, msg);
};

const std::string Tintin_reporter::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    struct tm time_info;
    localtime_r(&time_t_now, &time_info);

    std::stringstream ss;
    ss << std::put_time(&time_info, "%d/%m/%Y %H:%M:%S");
    return ss.str();
}

void Tintin_reporter::_log(LogLevel level, const std::string &msg) {
    const char *levelStr;

    switch (level) {
    case LogLevel::DEBUG:
        levelStr = "DEBUG";
        break;
    case LogLevel::LOG:
        levelStr = "LOG";
        break;
    case LogLevel::NOTICE:
        levelStr = "NOTICE";
        break;
    case LogLevel::INFO:
        levelStr = "INFO";
        break;
    case LogLevel::ERROR:
        levelStr = "ERROR";
        break;
    case LogLevel::FATAL:
        levelStr = "FATAL";
        break;
    }

    this->logfile << "[" << this->getTimestamp() << "] "
                  << "[" << levelStr << "] " << this->LOG_PREFIX << " " << msg << std::endl;
}
