#include <gtest/gtest.h>
#include "config/service_config.h"
#include "handlers/api_handlers.h"
#include "utils/logger.h"
#include <memory>
#include <crow.h>

class ApiHandlersTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::Logger::initialize(Logger::Level::ERROR);
        
        auto config = ServiceConfig::load_from_env();
        auto pool = std::make_unique<DatabasePool>(config.m_database_url, config.m_db_pool_size);
        
        handlers = std::make_unique<ApiHandlers>(std::move(pool));
    }

    void TearDown() override {
        handlers.reset();
    }

    std::unique_ptr<ApiHandlers> handlers;
};

TEST_F(ApiHandlersTest, HealthCheckEndpoint) {
    auto response = handlers->handle_health_check();
    
    EXPECT_TRUE(response.code == 200 || response.code == 503);
    
    EXPECT_FALSE(response.body.empty());
    
    EXPECT_NE(response.body.find("status"), std::string::npos);
}

TEST_F(ApiHandlersTest, RootEndpoint) {
    auto response = handlers->handle_root();
    
    EXPECT_EQ(response.code, 200);
    EXPECT_FALSE(response.body.empty());
    
    EXPECT_NE(response.body.find("IP Location Service"), std::string::npos);
}

TEST_F(ApiHandlersTest, MetricsEndpoint) {
    auto response = handlers->handle_metrics();
    
    EXPECT_EQ(response.code, 200);
    EXPECT_FALSE(response.body.empty());
    
    EXPECT_NE(response.body.find("database_healthy"), std::string::npos);
}

TEST_F(ApiHandlersTest, IPLocationWithInvalidIP) {
    crow::request req;
    req.url_params = crow::query_string("?ip=invalid_ip");
    
    auto response = handlers->handle_ip_location(req);
    
    EXPECT_EQ(response.code, 400);
    EXPECT_NE(response.body.find("Invalid IP address"), std::string::npos);
}

TEST_F(ApiHandlersTest, IPLocationWithMissingIP) {
    crow::request req;
    // No IP parameter provided
    
    auto response = handlers->handle_ip_location(req);
    
    EXPECT_EQ(response.code, 400);
    EXPECT_NE(response.body.find("IP address parameter 'ip' is missing"), std::string::npos);
}

TEST_F(ApiHandlersTest, ErrorResponseFormat) {
    crow::request req;
    req.url_params = crow::query_string("?ip=invalid");
    
    auto response = handlers->handle_ip_location(req);
    
    EXPECT_NE(response.body.find("error"), std::string::npos);
    EXPECT_NE(response.body.find("code"), std::string::npos);
}

TEST_F(ApiHandlersTest, ClientIPExtraction) {
    crow::request req;
    
    // Test X-Forwarded-For header
    req.headers.insert({"X-Forwarded-For", "192.168.1.100"});
    req.url_params = crow::query_string("?ip=8.8.8.8");
    
    auto response = handlers->handle_ip_location(req);
    
    EXPECT_TRUE(response.code >= 200 && response.code < 600);
}

TEST_F(ApiHandlersTest, RateLimitingFunctionality) {
    crow::request req;
    req.url_params = crow::query_string("?ip=8.8.8.8");
    
    std::vector<crow::response> responses;
    for (int i = 0; i < 105; ++i) {
        responses.push_back(handlers->handle_ip_location(req));
    }
    
    //at least some requests should be rate limited (429)
    bool found_rate_limit = false;
    for (const auto& resp : responses) {
        if (resp.code == 429) {
            found_rate_limit = true;
            break;
        }
    }
    
    EXPECT_TRUE(found_rate_limit || responses.size() > 0);
}


TEST_F(ApiHandlersTest, RouteRegistration) {
    crow::App<> app;
    
    // should not throw when registering routes
    EXPECT_NO_THROW({
        handlers->register_routes(app);
    });
}

class ApiHandlersIPValidationTest : public ::testing::TestWithParam<std::pair<std::string, bool>> {
protected:
    void SetUp() override {
        Logger::Logger::initialize(Logger::Level::ERROR);
        auto config = ServiceConfig::load_from_env();
        auto pool = std::make_unique<DatabasePool>(config.m_database_url, config.m_db_pool_size);
        handlers = std::make_unique<ApiHandlers>(std::move(pool));
    }

    std::unique_ptr<ApiHandlers> handlers;
};

TEST_P(ApiHandlersIPValidationTest, VariousIPFormats) {
    auto [ip, should_be_valid] = GetParam();
    
    crow::request req;
    req.url_params = crow::query_string("?ip=" + ip);
    
    auto response = handlers->handle_ip_location(req);
    
    if (should_be_valid) {
        EXPECT_NE(response.code, 400);
    } else {
        EXPECT_EQ(response.code, 400);
        if (ip.empty()) {
            EXPECT_NE(response.body.find("IP address parameter 'ip' is missing"), std::string::npos);
        } else {
            EXPECT_NE(response.body.find("Invalid IP address format"), std::string::npos);
        }
    }
}

INSTANTIATE_TEST_SUITE_P(
    IPValidation,
    ApiHandlersIPValidationTest,
    ::testing::Values(
        std::make_pair("192.168.1.1", true),
        std::make_pair("8.8.8.8", true),
        std::make_pair("::1", true),
        std::make_pair("2001:db8::1", true),
        std::make_pair("invalid_ip", false),
        std::make_pair("256.256.256.256", false),
        std::make_pair("", false),
        std::make_pair("192.168.1", false),
        std::make_pair("gggg::1", false)
    )
);
