#pragma once
#include <string>
#include <cstdlib>

class ServiceConfig {
public:
    std::string m_database_url;
    int m_server_port;
    int m_db_pool_size;
    int m_rate_limit_requests;
    int m_rate_limit_window_seconds;
    std::string m_log_level;
    bool m_enable_metrics;
    std::string m_redis_url;

    static ServiceConfig load_from_env();

private:
    static std::string get_env_var(const char* name, const std::string& default_value = "");
    static int get_env_int(const char* name, int default_value);
    static bool get_env_bool(const char* name, bool default_value);
};