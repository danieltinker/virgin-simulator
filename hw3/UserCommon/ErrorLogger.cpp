#include "ErrorLogger.h"
#include <chrono>
#include <iomanip>
#include <ctime>
#include <sstream>

ErrorLogger& ErrorLogger::instance() {
    static ErrorLogger inst;
    return inst;
}

ErrorLogger::ErrorLogger() : initialized_(false) {}

std::string ErrorLogger::makeTimestamp() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm tm;
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H-%M-%S");
    return oss.str();
}

void ErrorLogger::init() {
    std::lock_guard<std::mutex> lg(mu_);
    if (initialized_) return;
    std::string ts = makeTimestamp();
    std::string name = "errors_" + ts + ".txt";
    ofs_.open(name, std::ios::app);
    if (!ofs_) {
        return;
    }
    initialized_ = true;
    ofs_ << "=== Error log started at " << ts << " ===\n";
    ofs_.flush();
}

void ErrorLogger::log(const std::string& msg) {
    std::lock_guard<std::mutex> lg(mu_);
    if (!initialized_) init();
    if (ofs_) {
        ofs_ << msg << "\n";
        ofs_.flush();
    } else {
        fprintf(stderr, "ErrorLogger fallback: %s\n", msg.c_str());
    }
}

void ErrorLogger::logContext(const std::string& msg, const char* file, int line) {
    std::ostringstream oss;
    oss << "[" << file << ":" << line << "] " << msg;
    log(oss.str());
}

void ErrorLogger::flush() {
    std::lock_guard<std::mutex> lg(mu_);
    if (ofs_) ofs_.flush();
}

ErrorLogger::~ErrorLogger() {
    if (ofs_) {
        ofs_ << "=== Error log closed ===\n";
        ofs_.flush();
    }
}
