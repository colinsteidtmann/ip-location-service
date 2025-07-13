#pragma once
#include <string>

class IpValidator {
public:
    static bool is_valid_ip(const std::string& ip);
    static bool is_valid_ipv4(const std::string& ip);
    static bool is_valid_ipv6(const std::string& ip);
};