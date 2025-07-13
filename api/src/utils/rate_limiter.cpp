#include "rate_limiter.h"
#include <algorithm>

const std::chrono::minutes RateLimiter::CLEANUP_INTERVAL{5};

RateLimiter::RateLimiter(int max_requests, int window_seconds) 
    : m_max_requests(max_requests), m_window(window_seconds), m_last_cleanup(std::chrono::steady_clock::now()) {
}

bool RateLimiter::is_allowed(const std::string& client_ip) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::steady_clock::now();
    
    if (now - m_last_cleanup > CLEANUP_INTERVAL) {
        cleanup_old_requests();
        m_last_cleanup = now;
    }
    
    auto& client_requests = m_requests[client_ip];
    
    while (!client_requests.empty() && now - client_requests.front() > m_window) {
        client_requests.pop_front();
    }
    
    if (client_requests.size() >= static_cast<size_t>(m_max_requests)) {
        return false;
    }
    
    client_requests.push_back(now);
    return true;
}

void RateLimiter::cleanup_old_requests() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = m_requests.begin(); it != m_requests.end();) {
        auto& client_requests = it->second;
        
        while (!client_requests.empty() && now - client_requests.front() > m_window) {
            client_requests.pop_front();
        }
        
        if (client_requests.empty()) {
            it = m_requests.erase(it);
        } else {
            ++it;
        }
    }
}