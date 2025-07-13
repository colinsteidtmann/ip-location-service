#include "service_config.h"
#include <iostream>
#include <algorithm>

ServiceConfig ServiceConfig::load_from_env() {
    ServiceConfig config;
    
    //database configuration
    const char* db_url = std::getenv("DATABASE_URL");
    if (!db_url) {
        throw std::runtime_error("DATABASE_URL environment variable not set");
    }
    config.m_database_url = std::string(db_url) + "?connect_timeout=10&application_name=IPLocationService";
    
    //server configuration
    config.m_server_port = get_env_int("SERVER_PORT", 8080);
    config.m_db_pool_size = get_env_int("DB_POOL_SIZE", 10);
    
    //rate limiting
    config.m_rate_limit_requests = get_env_int("RATE_LIMIT_REQUESTS", 100);
    config.m_rate_limit_window_seconds = get_env_int("RATE_LIMIT_WINDOW", 60);
    
    //other
    config.m_log_level = get_env_var("LOG_LEVEL", "INFO");
    config.m_enable_metrics = get_env_bool("ENABLE_METRICS", true);
    config.m_redis_url = get_env_var("REDIS_URL", "");
    
    return config;
}

std::string ServiceConfig::get_env_var(const char* name, const std::string& default_value) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : default_value;
}

int ServiceConfig::get_env_int(const char* name, int default_value) {
    const char* value = std::getenv(name);
    if (!value) return default_value;
    
    try {
        return std::stoi(value);
    } catch (const std::exception&) {
        std::cerr << "Warning: Invalid integer value for " << name << ", using default: " << default_value << std::endl;
        return default_value;
    }
}

bool ServiceConfig::get_env_bool(const char* name, bool default_value) {
    const char* value = std::getenv(name);
    if (!value) return default_value;
    
    std::string str_value = value;
    std::transform(str_value.begin(), str_value.end(), str_value.begin(), ::tolower);
    
    return str_value == "true" || str_value == "1" || str_value == "yes" || str_value == "on";
}