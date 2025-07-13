#include "ip_validator.h"
#include <regex>
#include <arpa/inet.h>

bool IpValidator::is_valid_ip(const std::string& ip) {
    return is_valid_ipv4(ip) || is_valid_ipv6(ip);
}

bool IpValidator::is_valid_ipv4(const std::string& ip) {
    struct in_addr addr;
    return inet_pton(AF_INET, ip.c_str(), &addr) == 1;
}

bool IpValidator::is_valid_ipv6(const std::string& ip) {
    struct in6_addr addr6;
    return inet_pton(AF_INET6, ip.c_str(), &addr6) == 1;
}