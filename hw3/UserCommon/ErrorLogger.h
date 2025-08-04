#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <cstdio> // for std::snprintf

class ErrorLogger {
public:
    static ErrorLogger& instance();

    void init(); // idempotent
    void log(const std::string& msg);
    void logContext(const std::string& msg, const char* file, int line);
    void flush();

private:
    ErrorLogger();
    ~ErrorLogger();
    std::mutex mu_;
    std::ofstream ofs_;
    bool initialized_;
    std::string makeTimestamp();
};

#define LOG_ERROR(msg) ErrorLogger::instance().logContext(msg, __FILE__, __LINE__)
#define LOG_ERROR_FMT(fmt, ...) do { \
    char buf[1024]; \
    std::snprintf(buf, sizeof(buf), fmt, __VA_ARGS__); \
    ErrorLogger::instance().logContext(buf, __FILE__, __LINE__); \
} while(0)
