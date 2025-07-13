#include <gtest/gtest.h>
#include "utils/logger.h"
#include <iostream>
#include <sstream>

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        original_cout = std::cout.rdbuf();
        std::cout.rdbuf(cout_stream.rdbuf());
        original_cerr = std::cerr.rdbuf();
        std::cerr.rdbuf(cerr_stream.rdbuf());
    }

    void TearDown() override {
        std::cout.rdbuf(original_cout);
        std::cerr.rdbuf(original_cerr);
    }

    std::stringstream cout_stream;
    std::streambuf* original_cout;
    std::stringstream cerr_stream;
    std::streambuf* original_cerr;
};

TEST_F(LoggerTest, InitializeLogger) {
    Logger::Logger::initialize(Logger::Level::DEBUG);
    auto logger = Logger::Logger::get_logger();
    ASSERT_NE(logger, nullptr);
}

TEST_F(LoggerTest, LogLevelParsing) {
    EXPECT_EQ(Logger::parseLogLevel("debug"), Logger::Level::DEBUG);
    EXPECT_EQ(Logger::parseLogLevel("DEBUG"), Logger::Level::DEBUG);
    EXPECT_EQ(Logger::parseLogLevel("info"), Logger::Level::INFO);
    EXPECT_EQ(Logger::parseLogLevel("INFO"), Logger::Level::INFO);
    EXPECT_EQ(Logger::parseLogLevel("warning"), Logger::Level::WARNING);
    EXPECT_EQ(Logger::parseLogLevel("WARNING"), Logger::Level::WARNING);
    EXPECT_EQ(Logger::parseLogLevel("error"), Logger::Level::ERROR);
    EXPECT_EQ(Logger::parseLogLevel("ERROR"), Logger::Level::ERROR);
    
    EXPECT_THROW(Logger::parseLogLevel("invalid"), std::invalid_argument);
}

TEST_F(LoggerTest, LoggingLevels) {
    Logger::Logger::initialize(Logger::Level::WARNING);
    auto logger = Logger::Logger::get_logger();

    cout_stream.str("");
    cout_stream.clear();
    cerr_stream.str("");
    cerr_stream.clear();

    logger->debug("Debug message");
    logger->info("Info message");
    logger->warning("Warning message");
    logger->error("Error message");

    std::string cout_output = cout_stream.str();
    std::string cerr_output = cerr_stream.str();

    EXPECT_EQ(cout_output.find("Debug message"), std::string::npos);
    EXPECT_EQ(cout_output.find("Info message"), std::string::npos);
    EXPECT_NE(cout_output.find("Warning message"), std::string::npos);
    EXPECT_NE(cerr_output.find("Error message"), std::string::npos);
}

TEST_F(LoggerTest, FormattedLogging) {
    Logger::Logger::initialize(Logger::Level::DEBUG);
    auto logger = Logger::Logger::get_logger();
    
    cout_stream.str("");
    cout_stream.clear();
    
    logger->info("Test {} with number {}", "string", 42);
    std::string output = cout_stream.str();
    EXPECT_NE(output.find("Test string with number 42"), std::string::npos);
}

TEST_F(LoggerTest, SingletonBehavior) {
    Logger::Logger::initialize(Logger::Level::INFO);
    auto logger1 = Logger::Logger::get_logger();
    auto logger2 = Logger::Logger::get_logger();
    
    EXPECT_EQ(logger1, logger2);
}
