#include "api_handlers.h"
#include "../utils/ip_validator.h"
#include "../utils/logger.h"
#include <chrono>
#include <sw/redis++/redis++.h>

ApiHandlers::ApiHandlers(std::unique_ptr<DatabasePool> db_pool) 
    : m_db_pool(std::move(db_pool)) {
    m_rate_limiter = std::make_unique<RateLimiter>(100, 60); // 100 requests per minute
    
    try {
        std::string redis_url = std::getenv("REDIS_URL") ? std::getenv("REDIS_URL") : "redis://redis:6379";
        m_redis_client = std::make_unique<sw::redis::Redis>(redis_url);
        
        m_redis_client->ping();
        auto logger = Logger::Logger::get_logger();
        logger->info("Redis connection established successfully");
    } catch (const std::exception& e) {
        auto logger = Logger::Logger::get_logger();
        logger->error("Failed to connect to Redis: {}", e.what());
        m_redis_client = nullptr;
    }
}

crow::response ApiHandlers::handle_health_check() {
    crow::json::wvalue health;
    health["status"] = "healthy";
    health["timestamp"] = std::time(nullptr);
    
    //database
    bool db_healthy = m_db_pool && m_db_pool->health_check();
    health["database"]["status"] = db_healthy ? "healthy" : "unhealthy";
    
    //redis
    bool redis_healthy = false;
    if (m_redis_client) {
        try {
            m_redis_client->ping();
            redis_healthy = true;
        } catch (const std::exception& e) {
            auto logger = Logger::Logger::get_logger();
            logger->warning("Redis health check failed: {}", e.what());
        }
    }
    health["cache"]["status"] = redis_healthy ? "healthy" : "unhealthy";
    
    if (db_healthy && redis_healthy) {
        return crow::response(200, health);
    } else if (db_healthy) {
        // Database is healthy but Redis is not --> still operational
        health["status"] = "degraded";
        return crow::response(200, health);
    } else {
        health["status"] = "unhealthy";
        return crow::response(503, health);
    }
}

crow::response ApiHandlers::handle_root() {
    return crow::response(200, "{\"message\":\"IP Location Service API\",\"version\":\"1.0\"}");
}

crow::response ApiHandlers::handle_ip_location(const crow::request& req) {
    auto logger = Logger::Logger::get_logger();
    
    std::string client_ip = get_client_ip(req);
    if (!m_rate_limiter->is_allowed(client_ip)) {
        return crow::response(429, create_error_response("Rate limit exceeded", "RATE_LIMIT_EXCEEDED"));
    }

    const char* ip_raw = req.url_params.get("ip");
    std::string ip_str = ip_raw ? ip_raw : "";
    if (ip_str.empty()) {
        return crow::response(400, create_error_response("IP address parameter 'ip' is missing", "MISSING_PARAMETER"));
    }

    if (!IpValidator::is_valid_ip(ip_str)) {
        return crow::response(400, create_error_response("Invalid IP address format", "INVALID_IP_FORMAT"));
    }

    try {
        // try to get from cache first
        std::string cached_result = get_from_cache(ip_str);
        if (!cached_result.empty()) {
            logger->debug("Cache hit for IP: {}", ip_str);
            return crow::response(200, cached_result);
        }

        logger->debug("Cache miss for IP: {}", ip_str);

        auto conn = m_db_pool->get_connection();
        if (!conn) {
            logger->error("Database connection unavailable for IP: {}", ip_str);
            return crow::response(500, create_error_response("Database connection unavailable", "DB_CONNECTION_ERROR"));
        }

        pqxx::work W(*conn);
        pqxx::result R = W.exec_prepared(DatabasePool::PREPARED_IP_LOOKUP_NAME, ip_str);
        W.commit();

        m_db_pool->return_connection(std::move(conn));

        if (!R.empty()) {
            crow::json::wvalue response_json;
            response_json["ip"] = ip_str;

            if (!R[0]["country"].is_null()) {
                response_json["country"] = R[0]["country"].as<std::string>();
            }
            if (!R[0]["city"].is_null()) {
                response_json["city"] = R[0]["city"].as<std::string>();
            }
            if (!R[0]["region"].is_null()) {
                response_json["region"] = R[0]["region"].as<std::string>();
            }
            if (!R[0]["latitude"].is_null()) {
                response_json["latitude"] = R[0]["latitude"].as<double>();
            }
            if (!R[0]["longitude"].is_null()) {
                response_json["longitude"] = R[0]["longitude"].as<double>();
            }
            if (!R[0]["postal_code"].is_null()) {
                response_json["postal_code"] = R[0]["postal_code"].as<std::string>();
            }
            if (!R[0]["timezone"].is_null()) {
                response_json["timezone"] = R[0]["timezone"].as<std::string>();
            }

            std::string response_str = response_json.dump();
            cache_result(ip_str, response_str);

            return crow::response(200, response_json);
        } else {
            std::string not_found_response = create_error_response("IP address location not found", "IP_NOT_FOUND").dump();
            cache_result(ip_str, not_found_response, 300); // 5 minutes cache timeout for not found
            
            return crow::response(404, create_error_response("IP address location not found", "IP_NOT_FOUND"));
        }

    } catch (const pqxx::broken_connection& e) {
        logger->error("DB query failed due to broken connection: {}", e.what());
        return crow::response(500, create_error_response("Database connection lost", "DB_CONNECTION_LOST"));
    } catch (const std::exception& e) {
        logger->error("DB query error: {}", e.what());
        return crow::response(500, create_error_response("Database query error", "DB_QUERY_ERROR"));
    }
}

crow::response ApiHandlers::handle_metrics() {
    crow::json::wvalue metrics;
    metrics["database_healthy"] = m_db_pool->is_pool_healthy();
    
    if (m_redis_client) {
        try {
            m_redis_client->ping();
            metrics["redis_healthy"] = true;
            
            // Get Redis info
            auto info = m_redis_client->info("memory");
            // Parse memory usage if needed
            metrics["redis_connected"] = true;
        } catch (const std::exception& e) {
            metrics["redis_healthy"] = false;
            metrics["redis_connected"] = false;
        }
    } else {
        metrics["redis_healthy"] = false;
        metrics["redis_connected"] = false;
    }
    
    metrics["uptime"] = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    return crow::response(200, metrics);
}

std::string ApiHandlers::get_client_ip(const crow::request& req) {
    std::string client_ip = req.get_header_value("X-Forwarded-For");
    if (client_ip.empty()) {
        client_ip = req.get_header_value("X-Real-IP");
    }
    if (client_ip.empty()) {
        client_ip = "unknown";
    }
    return client_ip;
}

crow::json::wvalue ApiHandlers::create_error_response(const std::string& error, const std::string& code) {
    crow::json::wvalue response;
    response["error"] = error;
    response["code"] = code;
    response["timestamp"] = std::time(nullptr);
    return response;
}

std::string ApiHandlers::get_from_cache(const std::string& ip) {
    if (!m_redis_client) {
        return "";
    }
    
    try {
        std::string cache_key = "ip_location:" + ip;
        auto cached_value = m_redis_client->get(cache_key);
        
        if (cached_value) {
            return *cached_value;
        }
    } catch (const std::exception& e) {
        auto logger = Logger::Logger::get_logger();
        logger->warning("Redis cache read error for IP {}: {}", ip, e.what());
    }
    
    return "";
}

void ApiHandlers::cache_result(const std::string& ip, const std::string& result, int ttl_seconds) {
    if (!m_redis_client) {
        return;
    }
    
    try {
        std::string cache_key = "ip_location:" + ip;
        m_redis_client->setex(cache_key, ttl_seconds, result);
    } catch (const std::exception& e) {
        auto logger = Logger::Logger::get_logger();
        logger->warning("Redis cache write error for IP {}: {}", ip, e.what());
    }
}