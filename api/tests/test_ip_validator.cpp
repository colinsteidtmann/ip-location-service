#include <gtest/gtest.h>
#include "utils/ip_validator.h"

class IpValidatorTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(IpValidatorTest, ValidIPv4Addresses) {
    EXPECT_TRUE(IpValidator::is_valid_ipv4("192.168.1.1"));
    EXPECT_TRUE(IpValidator::is_valid_ipv4("127.0.0.1"));
    EXPECT_TRUE(IpValidator::is_valid_ipv4("0.0.0.0"));
    EXPECT_TRUE(IpValidator::is_valid_ipv4("255.255.255.255"));
    EXPECT_TRUE(IpValidator::is_valid_ipv4("8.8.8.8"));
    EXPECT_TRUE(IpValidator::is_valid_ipv4("10.0.0.1"));
}

TEST_F(IpValidatorTest, InvalidIPv4Addresses) {
    EXPECT_FALSE(IpValidator::is_valid_ipv4("256.1.1.1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("192.168.1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("192.168.1.1.1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("192.168.-1.1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("192.168.1.256"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4(""));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("not.an.ip.address"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("192.168.1.a"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("192.168..1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv4("...."));
}

TEST_F(IpValidatorTest, ValidIPv6Addresses) {
    EXPECT_TRUE(IpValidator::is_valid_ipv6("::1"));
    EXPECT_TRUE(IpValidator::is_valid_ipv6("2001:db8::1"));
    EXPECT_TRUE(IpValidator::is_valid_ipv6("2001:0db8:0000:0000:0000:ff00:0042:8329"));
    EXPECT_TRUE(IpValidator::is_valid_ipv6("2001:db8:0:0:1:0:0:1"));
    EXPECT_TRUE(IpValidator::is_valid_ipv6("::"));
    EXPECT_TRUE(IpValidator::is_valid_ipv6("::ffff:192.0.2.1"));
    EXPECT_TRUE(IpValidator::is_valid_ipv6("fe80::"));
}

TEST_F(IpValidatorTest, InvalidIPv6Addresses) {
    EXPECT_FALSE(IpValidator::is_valid_ipv6(""));
    EXPECT_FALSE(IpValidator::is_valid_ipv6("gggg::1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv6("2001:db8::1::1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv6("2001:db8:0:0:1:0:0:1:extra"));
    EXPECT_FALSE(IpValidator::is_valid_ipv6("192.168.1.1"));
    EXPECT_FALSE(IpValidator::is_valid_ipv6("not.an.ipv6.address"));
}

TEST_F(IpValidatorTest, GeneralIPValidation) {
    //IPv4 addresses
    EXPECT_TRUE(IpValidator::is_valid_ip("192.168.1.1"));
    EXPECT_TRUE(IpValidator::is_valid_ip("8.8.8.8"));
    
    //IPv6 addresses
    EXPECT_TRUE(IpValidator::is_valid_ip("::1"));
    EXPECT_TRUE(IpValidator::is_valid_ip("2001:db8::1"));
    
    //Invalid addresses
    EXPECT_FALSE(IpValidator::is_valid_ip(""));
    EXPECT_FALSE(IpValidator::is_valid_ip("not.an.ip"));
    EXPECT_FALSE(IpValidator::is_valid_ip("256.256.256.256"));
}

TEST_F(IpValidatorTest, EdgeCases) {
    EXPECT_FALSE(IpValidator::is_valid_ip(""));
    EXPECT_FALSE(IpValidator::is_valid_ipv4(""));
    EXPECT_FALSE(IpValidator::is_valid_ipv6(""));
    
    EXPECT_FALSE(IpValidator::is_valid_ip(" "));
    EXPECT_FALSE(IpValidator::is_valid_ip("192.168.1.1 "));
    EXPECT_FALSE(IpValidator::is_valid_ip(" 192.168.1.1"));
    
    EXPECT_FALSE(IpValidator::is_valid_ip("192.168.1.1\n"));
    EXPECT_FALSE(IpValidator::is_valid_ip("192.168.1.1\t"));
}
