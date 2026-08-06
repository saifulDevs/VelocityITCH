#pragma once

#include <iostream>
#include <string_view>
#include <chrono>
#include <iomanip>

namespace itch {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
private:
    inline static LogLevel min_level_{LogLevel::INFO};

public:
    static void set_level(LogLevel level) noexcept {
        min_level_ = level;
    }

    static void debug(std::string_view msg) {
        if (min_level_ <= LogLevel::DEBUG) log("DEBUG", msg);
    }

    static void info(std::string_view msg) {
        if (min_level_ <= LogLevel::INFO) log("INFO", msg);
    }

    static void warn(std::string_view msg) {
        if (min_level_ <= LogLevel::WARN) log("WARN", msg);
    }

    static void error(std::string_view msg) {
        if (min_level_ <= LogLevel::ERROR) log("ERROR", msg);
    }

private:
    static void log(std::string_view prefix, std::string_view msg) {
        std::cout << "[" << prefix << "] " << msg << "\n";
    }
};

} // namespace itch
