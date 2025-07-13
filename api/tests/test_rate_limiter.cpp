#include <gtest/gtest.h>
#include "utils/rate_limiter.h"
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>

class RateLimiterTest : public ::testing::Test {
protected:
    void SetUp() override {
        //rate limiter with 3 requests per 2 seconds for testing
        rate_limiter = std::make_unique<RateLimiter>(3, 2);
    }

    void TearDown() override {
        rate_limiter.reset();
    }

    std::unique_ptr<RateLimiter> rate_limiter;
};

TEST_F(RateLimiterTest, AllowsRequestsWithinLimit) {
    std::string client_ip = "192.168.1.1";
    
    //First 3 requests should be allowed
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
}

TEST_F(RateLimiterTest, BlocksRequestsOverLimit) {
    std::string client_ip = "192.168.1.1";
    
    //Use up the limit
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    
    //Fourth request should be blocked
    EXPECT_FALSE(rate_limiter->is_allowed(client_ip));
    EXPECT_FALSE(rate_limiter->is_allowed(client_ip));
}

TEST_F(RateLimiterTest, DifferentClientsIndependent) {
    std::string client1 = "192.168.1.1";
    std::string client2 = "192.168.1.2";
    
    //Each client should have their own limit
    EXPECT_TRUE(rate_limiter->is_allowed(client1));
    EXPECT_TRUE(rate_limiter->is_allowed(client2));
    EXPECT_TRUE(rate_limiter->is_allowed(client1));
    EXPECT_TRUE(rate_limiter->is_allowed(client2));
    EXPECT_TRUE(rate_limiter->is_allowed(client1));
    EXPECT_TRUE(rate_limiter->is_allowed(client2));
    
    //Both should be blocked after hitting their limits
    EXPECT_FALSE(rate_limiter->is_allowed(client1));
    EXPECT_FALSE(rate_limiter->is_allowed(client2));
}

TEST_F(RateLimiterTest, WindowReset) {
    std::string client_ip = "192.168.1.1";
    
    //Use up the limit
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
    EXPECT_FALSE(rate_limiter->is_allowed(client_ip));
    
    //Wait for window to reset
    std::this_thread::sleep_for(std::chrono::milliseconds(2100));
    
    //Should be allowed again
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
}

TEST_F(RateLimiterTest, CleanupOldRequests) {
    std::string client_ip = "192.168.1.1";
    
    //Make some requests
    rate_limiter->is_allowed(client_ip);
    rate_limiter->is_allowed(client_ip);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    rate_limiter->cleanup_old_requests();
    EXPECT_TRUE(rate_limiter->is_allowed(client_ip));
}

TEST_F(RateLimiterTest, DefaultConstructor) {
    RateLimiter default_limiter;
    std::string client_ip = "192.168.1.1";
    
    // default values (100 requests per 60 seconds)
    for (int i = 0; i < 100; ++i) {
        EXPECT_TRUE(default_limiter.is_allowed(client_ip));
    }
    
    //101st request should be blocked
    EXPECT_FALSE(default_limiter.is_allowed(client_ip));
}

TEST_F(RateLimiterTest, EmptyClientIP) {
    std::string empty_ip = "";
    
    //should handle empty IP gracefully
    EXPECT_TRUE(rate_limiter->is_allowed(empty_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(empty_ip));
    EXPECT_TRUE(rate_limiter->is_allowed(empty_ip));
    EXPECT_FALSE(rate_limiter->is_allowed(empty_ip));
}

TEST_F(RateLimiterTest, ThreadSafety) {
    std::string client_ip = "192.168.1.1";
    std::atomic<int> allowed_count{0};
    std::atomic<int> blocked_count{0};
    
    auto worker = [&]() {
        for (int i = 0; i < 10; ++i) {
            if (rate_limiter->is_allowed(client_ip)) {
                allowed_count++;
            } else {
                blocked_count++;
            }
        }
    };
    
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(worker);
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    //total requests should be 50, but only 3 should be allowed
    EXPECT_EQ(allowed_count + blocked_count, 50);
    EXPECT_EQ(allowed_count, 3);
    EXPECT_EQ(blocked_count, 47);
}
