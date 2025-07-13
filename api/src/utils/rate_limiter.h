#pragma once
#include <mutex>
#include <map>
#include <deque>
#include <chrono>
#include <string>

class RateLimiter {
public:
    RateLimiter(int max_requests = 100, int window_seconds = 60);
    bool is_allowed(const std::string& client_ip);
    void cleanup_old_requests();

private:
    std::mutex m_mutex;
    std::map<std::string, std::deque<std::chrono::steady_clock::time_point>> m_requests;
    const int m_max_requests;
    const std::chrono::seconds m_window;
    std::chrono::steady_clock::time_point m_last_cleanup;
    static const std::chrono::minutes CLEANUP_INTERVAL;
};