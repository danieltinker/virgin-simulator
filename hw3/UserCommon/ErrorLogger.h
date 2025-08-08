// ===================== ErrorLogger.h =====================
#pragma once
#include <fstream>
#include <mutex>
#include <string>
#include <cstdarg>
namespace UserCommon_315634022 {
class ErrorLogger {
public:
    static ErrorLogger& instance();

    void init();
    void log(const std::string& msg);
    void logFormatted(const char* fmt, ...);
    void logSection(const std::string& title);
    void logDivider();
    void logGameManagerError(const std::string& mapName,
                             const std::string& algo1,
                             const std::string& algo2,
                             const std::string& errorMsg);

private:
    ErrorLogger();
    ~ErrorLogger();
    ErrorLogger(const ErrorLogger&) = delete;
    ErrorLogger& operator=(const ErrorLogger&) = delete;

    std::ofstream out_;
    std::mutex logMutex_;
    std::string timestamp_;

    std::string currentTimestamp() const;
};
}