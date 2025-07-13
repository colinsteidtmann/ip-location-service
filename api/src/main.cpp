#include <iostream>
#include <memory>
#include <crow.h>
#include "crow/middlewares/cors.h"
#include "config/service_config.h"
#include "database/database_pool.h"
#include "handlers/api_handlers.h"
#include "utils/logger.h"

int main(int argc, char* argv[]) {
    try {

        Logger::Level log_level = Logger::Level::INFO;

        if (argc > 1) {
            try {
                log_level = Logger::parseLogLevel(argv[1]);
            } catch (const std::invalid_argument& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                std::cerr << "Usage: " << argv[0] << " [debug|info|warning|error]" << std::endl;
                return 1;
            }
        }

        Logger::Logger::initialize(log_level);
        auto logger = Logger::Logger::get_logger();
        logger->info("Starting IP Location Service...");

        auto config = ServiceConfig::load_from_env();

        logger->info("Initializing database connection pool...");
        auto db_pool = std::make_unique<DatabasePool>(config.m_database_url, config.m_db_pool_size);
        
        if (!db_pool->is_pool_healthy()) {
            logger->error("Failed to initialize database pool. Exiting.");
            return 1;
        }

        crow::App<crow::CORSHandler> app;
        
        app.get_middleware<crow::CORSHandler>().global()
            .headers("Content-Type", "Authorization")
            .methods("GET"_method, "POST"_method, "OPTIONS"_method)
            .origin("*");

        ApiHandlers handlers(std::move(db_pool));
        handlers.register_routes(app);

        logger->info("Server starting on port {}...", config.m_server_port);
        app.port(config.m_server_port).multithreaded().run();

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
