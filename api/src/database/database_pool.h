#pragma once
#include <pqxx/pqxx>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <string>

class DatabasePool {
public:
    static inline const std::string PREPARED_IP_LOOKUP_NAME = "ip_lookup_query";
    
    DatabasePool(const std::string& connection_string, int pool_size = 10);
    ~DatabasePool();
    
    std::unique_ptr<pqxx::connection> get_connection();
    void return_connection(std::unique_ptr<pqxx::connection> conn);
    bool health_check();
    bool is_pool_healthy() const;
    void initialize_pool();

private:
    std::vector<std::unique_ptr<pqxx::connection>> m_pool;
    std::mutex m_pool_mutex;
    std::string m_conn_str;
    std::atomic<bool> m_is_healthy{true};
    int m_pool_size;
    
    static inline const int MAX_RETRIES = 10;
    static inline const int RETRY_DELAY_SECONDS = 3;
    
    std::unique_ptr<pqxx::connection> create_connection();
};