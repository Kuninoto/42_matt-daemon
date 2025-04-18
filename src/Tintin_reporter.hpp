#pragma once

#include <fstream>
#include <string>

enum class LogLevel { DEBUG, LOG, NOTICE, INFO, ERROR, FATAL };

class Tintin_reporter {
    static constexpr const char *LOG_PREFIX = "matt-daemon:";
    std::ofstream logfile;
    bool _isValid;

    void _log(LogLevel level, const std::string &msg);
    const std::string getTimestamp();

public:
    Tintin_reporter(const std::string &logfilePath);
    Tintin_reporter(const Tintin_reporter &to_copy);
    Tintin_reporter &operator=(const Tintin_reporter &to_copy);
    ~Tintin_reporter(void);

    bool isValid(void);

    void debug(const std::string &msg);
    void log(const std::string &msg);
    void notice(const std::string &msg);
    void info(const std::string &msg);
    void error(const std::string &msg);
    void fatal(const std::string &msg);
};
