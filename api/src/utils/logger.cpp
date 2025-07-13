#include "logger.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace Logger {

std::shared_ptr<Logger> Logger::m_global_logger = nullptr;

void Logger::initialize(Level level) {
    m_global_logger = std::shared_ptr<Logger>(new Logger(level));
}

std::shared_ptr<Logger> Logger::get_logger() {
    if (!m_global_logger) {
        initialize(Level::INFO);
    }
    return m_global_logger;
}

Logger::Logger(Level level) : m_current_level(level) {}

void Logger::log(Level level, const std::string& message) {
    if (level < m_current_level) {
        return;
    }

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    ss << " [" << level_to_string(level) << "] " << message;

    if (level >= Level::ERROR) {
        std::cerr << ss.str() << std::endl;
    } else {
        std::cout << ss.str() << std::endl;
    }
}

std::string Logger::level_to_string(Level level) {
    switch (level) {
        case Level::DEBUG: return "DEBUG";
        case Level::INFO: return "INFO";
        case Level::WARNING: return "WARN";
        case Level::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

Level parseLogLevel(const std::string& level_str) {
    std::string upper_level_str = level_str;
    for (char &c : upper_level_str) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    if (upper_level_str == "DEBUG") {
        return Level::DEBUG;
    } else if (upper_level_str == "INFO") {
        return Level::INFO;
    } else if (upper_level_str == "WARNING") {
        return Level::WARNING;
    } else if (upper_level_str == "ERROR") {
        return Level::ERROR;
    } else {
        throw std::invalid_argument("Invalid log level: " + level_str);
    }
}

} // namespace Logger
