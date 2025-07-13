#include <iostream>
#include <string>
#include <string_view>
#include <crow.h>
#include "crow/middlewares/cors.h"
#include <pqxx/pqxx>
#include <cstdlib>
#include <thread>
#include <chrono>
#include <vector>
#include <mutex>
#include <memory>
#include <atomic>
#include <regex>

// Connection pool for better performance and thread safety
class DatabasePool {
public:
    inline static const std::string PREPARED_IP_LOOKUP_NAME = "ip_lookup_query";
private:
    std::vector<std::unique_ptr<pqxx::connection>> pool;
    std::mutex pool_mutex;
    std::string conn_str;
    std::atomic<bool> is_healthy{true};
    
    static const int POOL_SIZE = 10;
    static const int MAX_RETRIES = 10;
    static const int RETRY_DELAY_SECONDS = 3;

public:
    DatabasePool(const std::string& connection_string) : conn_str(connection_string) {
        initialize_pool();
    }

    ~DatabasePool() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        pool.clear();
    }

    void initialize_pool() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        pool.clear();
        
        for (int i = 0; i < POOL_SIZE; ++i) {
            auto conn = create_connection();
            if (conn) {
                pool.push_back(std::move(conn));
            }
        }
        
        if (pool.empty()) {
            std::cerr << "Failed to create any database connections!" << std::endl;
            is_healthy = false;
        } else {
            is_healthy = true;
            std::cout << "Database pool initialized with " << pool.size() << " connections." << std::endl;
        }
    }

    std::unique_ptr<pqxx::connection> get_connection() {
        std::lock_guard<std::mutex> lock(pool_mutex);
        
        if (!pool.empty()) {
            auto conn = std::move(pool.back());
            pool.pop_back();
            
            // Check if connection is still valid
            if (conn && conn->is_open()) {
                return conn;
            }
        }
        
        // If no connection available or connection is broken, create a new one
        return create_connection();
    }

    void return_connection(std::unique_ptr<pqxx::connection> conn) {
        if (!conn || !conn->is_open()) {
            return; // Don't return broken connections
        }
        
        std::lock_guard<std::mutex> lock(pool_mutex);
        if (pool.size() < POOL_SIZE) {
            pool.push_back(std::move(conn));
        }
    }

    bool health_check() {
        auto conn = get_connection();
        if (!conn) {
            is_healthy = false;
            return false;
        }
        
        try {
            pqxx::work W(*conn);
            pqxx::result R = W.exec("SELECT 1");
            W.commit();
            return_connection(std::move(conn));
            is_healthy = true;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Health check failed: " << e.what() << std::endl;
            is_healthy = false;
            return false;
        }
    }

    bool is_pool_healthy() const {
        return is_healthy.load();
    }

private:
    std::unique_ptr<pqxx::connection> create_connection() {
        for (int i = 0; i < MAX_RETRIES; ++i) {
            try {
                auto conn = std::make_unique<pqxx::connection>(conn_str);
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
                std::cerr << "Database connection attempt " << (i + 1) << " failed: " << e.what() << std::endl;
                if (i < MAX_RETRIES - 1) {
                    std::this_thread::sleep_for(std::chrono::seconds(RETRY_DELAY_SECONDS));
                }
            }
        }
        return nullptr;
    }
};

// Global database pool
std::unique_ptr<DatabasePool> db_pool;

// IP address validation function
bool is_valid_ip(const std::string& ip) {
    // Basic regex for IPv4 and IPv6
    std::regex ipv4_regex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    std::regex ipv6_regex(R"(^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$|^::1$|^::$)");
    
    if (std::regex_match(ip, ipv4_regex) || std::regex_match(ip, ipv6_regex)) {
        return true;
    }
    
    // More comprehensive IPv6 validation (simplified)
    if (ip.find(':') != std::string::npos) {
        // Basic IPv6 validation - could be enhanced
        return ip.length() >= 2 && ip.length() <= 39;
    }
    
    return false;
}

// Rate limiting (simple in-memory implementation)
class RateLimiter {
private:
    std::mutex mutex;
    std::map<std::string, std::deque<std::chrono::steady_clock::time_point>> requests;
    const int max_requests = 100;
    const std::chrono::seconds window{60};

public:
    bool is_allowed(const std::string& client_ip) {
        std::lock_guard<std::mutex> lock(mutex);
        auto now = std::chrono::steady_clock::now();
        
        auto& client_requests = requests[client_ip];
        
        // Remove old requests
        while (!client_requests.empty() && 
               now - client_requests.front() > window) {
            client_requests.pop_front();
        }
        
        if (client_requests.size() >= max_requests) {
            return false;
        }
        
        client_requests.push_back(now);
        return true;
    }
};

RateLimiter rate_limiter;

int main() {
    // 1. Get database connection string from environment variable
    const char* base_db_url = std::getenv("DATABASE_URL");
    if (!base_db_url) {
        std::cerr << "Error: DATABASE_URL environment variable not set." << std::endl;
        return 1;
    }

    // Construct the full connection string
    std::string full_conn_str = std::string(base_db_url) + 
        "?connect_timeout=10&application_name=IPLocationService";

    // 2. Initialize database pool
    std::cout << "Initializing database connection pool..." << std::endl;
    db_pool = std::make_unique<DatabasePool>(full_conn_str);

    if (!db_pool->is_pool_healthy()) {
        std::cerr << "Failed to initialize database pool. Exiting." << std::endl;
        return 1;
    }

    // 3. Start Crow server
    crow::SimpleApp app;

    // Enable CORS for all routes
    app.get_middleware<crow::CORSHandler>().global()
        .headers("Content-Type", "Authorization")
        .methods("GET"_method, "POST"_method, "OPTIONS"_method)
        .origin("*");

    // Health check endpoint
    CROW_ROUTE(app, "/health")([]() {
        if (db_pool && db_pool->health_check()) {
            return crow::response(200, "{\"status\":\"healthy\"}");
        } else {
            return crow::response(503, "{\"status\":\"unhealthy\"}");
        }
    });

    // Root endpoint
    CROW_ROUTE(app, "/")([]() {
        return crow::response(200, "{\"message\":\"IP Location Service API\",\"version\":\"1.0\"}");
    });

    // IP location lookup endpoint
    CROW_ROUTE(app, "/ip-location")([&](const crow::request& req) {
        // Rate limiting
        std::string client_ip = req.get_header_value("X-Forwarded-For");
        if (client_ip.empty()) {
            client_ip = req.get_header_value("X-Real-IP");
        }
        if (client_ip.empty()) {
            client_ip = "unknown";
        }

        if (!rate_limiter.is_allowed(client_ip)) {
            return crow::response(429, "{\"error\":\"Rate limit exceeded\"}");
        }

        // Get IP from query parameter
        std::string ip_str = req.url_params.get("ip");
        if (ip_str.empty()) {
            return crow::response(400, "{\"error\":\"IP address parameter 'ip' is missing\"}");
        }

        // Validate IP address format
        if (!is_valid_ip(ip_str)) {
            return crow::response(400, "{\"error\":\"Invalid IP address format\"}");
        }

        try {
            // Get connection from pool
            auto conn = db_pool->get_connection();
            if (!conn) {
                return crow::response(500, "{\"error\":\"Database connection unavailable\"}");
            }

            // Execute query
            pqxx::work W(*conn);
            pqxx::result R = W.exec_prepared(
                DatabasePool::PREPARED_IP_LOOKUP_NAME,
                ip_str
            );
            W.commit();

            // Return connection to pool
            db_pool->return_connection(std::move(conn));

            if (!R.empty()) {
                crow::json::wvalue response_json;
                response_json["ip"] = ip_str;

                // Extract fields, handling potential NULLs
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

                return crow::response(200, response_json);
            } else {
                return crow::response(404, "{\"error\":\"IP address location not found\"}");
            }

        } catch (const pqxx::broken_connection& e) {
            std::cerr << "DB query failed due to broken connection: " << e.what() << std::endl;
            return crow::response(500, "{\"error\":\"Database connection lost\"}");
        } catch (const std::exception& e) {
            std::cerr << "DB query error: " << e.what() << std::endl;
            return crow::response(500, "{\"error\":\"Database query error\"}");
        }
    });

    // Metrics endpoint (basic)
    CROW_ROUTE(app, "/metrics")([]() {
        crow::json::wvalue metrics;
        metrics["database_healthy"] = db_pool->is_pool_healthy();
        metrics["uptime"] = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return crow::response(200, metrics);
    });

    std::cout << "Server starting on port 8080..." << std::endl;
    app.port(8080).multithreaded().run();

    return 0;
}