#pragma once
#include <string>
#include <memory>
#include <format>

namespace Logger {

enum class Level { DEBUG, INFO, WARNING, ERROR };

class Logger {
public:
    static void initialize(Level level = Level::INFO);
    static std::shared_ptr<Logger> get_logger();

    template<typename... Args>
    void debug(const std::string& fmt_str, Args&&... args) {
        if (m_current_level <= Level::DEBUG) {
            log(Level::DEBUG, std::vformat(fmt_str, std::make_format_args(args...)));
        }
    }

    template<typename... Args>
    void info(const std::string& fmt_str, Args&&... args) {
        if (m_current_level <= Level::INFO) {
            log(Level::INFO, std::vformat(fmt_str, std::make_format_args(args...)));
        }
    }

    template<typename... Args>
    void warning(const std::string& fmt_str, Args&&... args) {
        if (m_current_level <= Level::WARNING) {
            log(Level::WARNING, std::vformat(fmt_str, std::make_format_args(args...)));
        }
    }

    template<typename... Args>
    void error(const std::string& fmt_str, Args&&... args) {
        if (m_current_level <= Level::ERROR) {
            log(Level::ERROR, std::vformat(fmt_str, std::make_format_args(args...)));
        }
    }

private:
    explicit Logger(Level level);

    void log(Level level, const std::string& message);
    std::string level_to_string(Level level);

    Level m_current_level;

    static std::shared_ptr<Logger> m_global_logger;
};

Level parseLogLevel(const std::string& level_str);

} // namespace Logger
