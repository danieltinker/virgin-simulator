// ===================== ErrorLogger.cpp =====================
#include "ErrorLogger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace UserCommon_315634022;
ErrorLogger& ErrorLogger::instance() {
    static ErrorLogger logger;
    return logger;
}

ErrorLogger::ErrorLogger() {}
ErrorLogger::~ErrorLogger() {
    if (out_.is_open()) out_.close();
}

void ErrorLogger::init() {
    timestamp_ = currentTimestamp();
    std::string fileName = "input_errors_" + timestamp_ + ".txt";
    out_.open(fileName);
    if (!out_.is_open()) {
        std::cerr << "Failed to open error log: " << fileName << std::endl;
    } else {
        out_ << "=== MAP LOADING ERRORS ===\n";
        out_ << "Generated: " << timestamp_ << "\n\n";
    }
}

void ErrorLogger::log(const std::string& msg) {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (out_.is_open()) out_ << msg << std::endl;
}

void ErrorLogger::logFormatted(const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (!out_.is_open()) return;

    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    out_ << buf << std::endl;
}

void ErrorLogger::logSection(const std::string& title) {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (out_.is_open()) out_ << "\n=== " << title << " ===\n";
}

void ErrorLogger::logDivider() {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (out_.is_open()) out_ << "----------------------------------------\n\n";
}

void ErrorLogger::logGameManagerError(const std::string& mapName,
                                      const std::string& algo1,
                                      const std::string& algo2,
                                      const std::string& errorMsg) {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (!out_.is_open()) return;
    out_ << "\n=== GAMEMANAGER ERRORS ===\n";
    out_ << "Map: " << mapName << "\n";
    out_ << "Algorithms: " << algo1 << " vs " << algo2 << "\n";
    out_ << "Error: " << errorMsg << "\n";
    out_ << "----------------------------------------\n\n";
}

std::string ErrorLogger::currentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&t, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y%m%d_%H%M%S");
    return oss.str();
}
