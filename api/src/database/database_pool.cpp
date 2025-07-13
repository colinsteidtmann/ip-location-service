#include "database_pool.h"
#include "../utils/logger.h"
#include <thread>
#include <chrono>
#include <iostream>

DatabasePool::DatabasePool(const std::string& connection_string, int pool_size) 
    : m_conn_str(connection_string), m_pool_size(pool_size) {
    initialize_pool();
}

DatabasePool::~DatabasePool() {
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    m_pool.clear();
}

void DatabasePool::initialize_pool() {
    auto logger = Logger::Logger::get_logger();
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    m_pool.clear();
    
    for (int i = 0; i < m_pool_size; ++i) {
        auto conn = create_connection();
        if (conn) {
            m_pool.push_back(std::move(conn));
        }
    }
    
    if (m_pool.empty()) {
        logger->error("Failed to create any database connections!");
        m_is_healthy = false;
    } else {
        m_is_healthy = true;
        logger->info("Database pool initialized with {} connections", m_pool.size());
    }
}

std::unique_ptr<pqxx::connection> DatabasePool::get_connection() {
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    
    if (!m_pool.empty()) {
        auto conn = std::move(m_pool.back());
        m_pool.pop_back();
        
        if (conn && conn->is_open()) {
            return conn;
        }
    }
    
    return create_connection();
}

void DatabasePool::return_connection(std::unique_ptr<pqxx::connection> conn) {
    if (!conn || !conn->is_open()) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(m_pool_mutex);
    if (m_pool.size() < static_cast<size_t>(m_pool_size)) {
        m_pool.push_back(std::move(conn));
    }
}

bool DatabasePool::health_check() {
    auto logger = Logger::Logger::get_logger();
    auto conn = get_connection();
    if (!conn) {
        m_is_healthy = false;
        return false;
    }
    
    try {
        pqxx::work W(*conn);
        pqxx::result R = W.exec("SELECT 1");
        W.commit();
        return_connection(std::move(conn));
        m_is_healthy = true;
        return true;
    } catch (const std::exception& e) {
        logger->error("Health check failed: {}", e.what());
        m_is_healthy = false;
        return false;
    }
}

bool DatabasePool::is_pool_healthy() const {
    return m_is_healthy.load();
}

std::unique_ptr<pqxx::connection> DatabasePool::create_connection() {
    auto logger = Logger::Logger::get_logger();
    
    for (int i = 0; i < MAX_RETRIES; ++i) {
        try {
            auto conn = std::make_unique<pqxx::connection>(m_conn_str);
            if (conn->is_open()) {
                // Prepare the statement immediately after connecting
                conn->prepare(
                    PREPARED_IP_LOOKUP_NAME,
                    "SELECT country, city, region, latitude, longitude, postal_code, timezone "
                    "FROM ip_locations "
                    "WHERE $1::inet >= start_ip AND $1::inet <= end_ip "
                    "ORDER BY start_ip "
                    "LIMIT 1"
                );
                return conn;
            }
        } catch (const std::exception& e) {
            logger->error("Database connection attempt {} failed: {}", i + 1, e.what());
            if (i < MAX_RETRIES - 1) {
                std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
            }
        }
    }
    return nullptr;
}