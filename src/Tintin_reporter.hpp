#pragma once

#include <fstream>
#include <string>

enum class LogLevel { LOG, NOTICE, INFO, WARN, ERROR, FATAL };

class Tintin_reporter {
    static constexpr const char *LOG_PREFIX = "matt-daemon:";

    std::ofstream logfile;
    std::string logfilePath;

    void _log(LogLevel level, const std::string &msg) noexcept;
    const std::string getTimestamp(void) const noexcept;

public:
    Tintin_reporter(const std::string &logfilePath) noexcept;
    Tintin_reporter(const Tintin_reporter &rhs) noexcept;
    Tintin_reporter &operator=(const Tintin_reporter &rhs) noexcept;
    ~Tintin_reporter(void) noexcept;

    bool isValid(void) const noexcept;

    void log(const std::string &msg) noexcept;
    void notice(const std::string &msg) noexcept;
    void info(const std::string &msg) noexcept;
    void warn(const std::string &msg) noexcept;
    void error(const std::string &msg) noexcept;
    void fatal(const std::string &msg) noexcept;
};
