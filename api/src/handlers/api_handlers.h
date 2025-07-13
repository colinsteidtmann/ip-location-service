#pragma once
#include <crow.h>
#include <memory>
#include <sw/redis++/redis++.h>
#include "../database/database_pool.h"
#include "../utils/rate_limiter.h"

class ApiHandlers {
public:
    explicit ApiHandlers(std::unique_ptr<DatabasePool> db_pool);
    
    template <typename App>
    void register_routes(App& app) {

        CROW_ROUTE(app, "/health")([this]() {
            return handle_health_check();
        });

        CROW_ROUTE(app, "/")([this]() {
            return handle_root();
        });

        CROW_ROUTE(app, "/ip-location")([this](const crow::request& req) {
            return handle_ip_location(req);
        });

        CROW_ROUTE(app, "/metrics")([this]() {
            return handle_metrics();
        });
    }

    // public for testing (an alternative could be making them friends)
    crow::response handle_health_check();
    crow::response handle_root();
    crow::response handle_ip_location(const crow::request& req);
    crow::response handle_metrics();

private:
    std::unique_ptr<DatabasePool> m_db_pool;
    std::unique_ptr<RateLimiter> m_rate_limiter;
    std::unique_ptr<sw::redis::Redis> m_redis_client;
    
    std::string get_client_ip(const crow::request& req);
    crow::json::wvalue create_error_response(const std::string& error, const std::string& code = "INTERNAL_ERROR");
    
    std::string get_from_cache(const std::string& ip);
    void cache_result(const std::string& ip, const std::string& result, int ttl_seconds = 3600); // 1 hour default TTL
};