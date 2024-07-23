/**
 * 用于TCP/UDP连接库
 */

#include <string.h>
#include <stdlib.h>

#include <iostream>
#include <exception>
#include <limits>
#include <tuple>
#include <vector>

#ifndef __Z_INTERNET
#define __Z_INTERNET

#pragma region Exceptions

class Ipv4TransferException : public std::exception {
public:
        explicit Ipv4TransferException(const std::string& msg) : __msg(msg) {}

        const char* what() const noexcept override {
                return __msg.c_str();
        }
private:
        std::string __msg;
};

class InvalidIpv4Address : public std::exception {
public:
        explicit InvalidIpv4Address(const std::string& ip) : __msg("Ip address: " + ip + " is invalid.") {}
        const char* what() const noexcept override {
                return __msg.c_str();
        }
private:
        std::string __msg;
};


#pragma region Ipv4 Code

// default in current path
// std::string __ip_config_url {"./config"};

// replaced by config.h

using ipv4_i = uint32_t;
using port_t = uint16_t;

// ipv4: total 32 bit
class ipv4 {
public:
        // prase from ipv4 string format like 'xxx.xxx.xxx.xxx:port'
        static ipv4 new_with_str(const std::string&);
        // prase from config file
        static ipv4 new_with_config() noexcept;

        ipv4(const ipv4&) = default;
        ipv4& operator=(const ipv4&) = default;
        ipv4(ipv4&&) = default;
        ipv4& operator=(ipv4&&) = default;

        virtual ~ipv4();

        operator std::string() const {
                if (__addr_str.empty()) {
                        if (this->__addr == 0) {
                                throw Ipv4TransferException("Transfer from empty ipv4 type, you shoule init first.");
                        }
                        this->__addr_str = ipv4::transfer_ipv4_to_str(this->__addr);
                }
                return this->__addr_str;
        }

        port_t port() const noexcept {
                return this->__port;
        }

        static ipv4_i transfer_str_to_ipv4(const std::string&);
        static std::string transfer_ipv4_to_str(const ipv4_i&);
private:
        ipv4_i __addr {0};
        port_t __port {0};
        mutable std::string __addr_str;

        // private NoArgsConstructor
        ipv4() noexcept {}
        // private AllArgsConstructor
        ipv4(const ipv4_i& addr, const port_t& port, const std::string& addr_str) noexcept
        : __addr(addr), __port(port), __addr_str(addr_str) {}
};

// TODO prase string
ipv4 ipv4::new_with_str(const std::string& addr) {
        auto is_digital = [](char c) -> bool {
                return (c >= '0' && c <= '9');
        };

        std::string port_str {""};

        int i = addr.size() - 1;
        for (; i >= 0; --i) {
                if (addr[i] == ':') break;
                if (is_digital(addr[i])) {
                        port_str += addr[i];
                } else {
                        throw InvalidIpv4Address(addr);
                }
        }

        // at least x.x.x.x
        if (i < 7) {
                throw InvalidIpv4Address(addr);
        }

        // system argument
        const int SYSTEM_MAX_PORT = std::numeric_limits<port_t>::max();
        int real_port = atoi(port_str.c_str());
        if (real_port == 0 || real_port > SYSTEM_MAX_PORT) {
                throw InvalidIpv4Address(addr);
        }

        std::string ip_part = addr.substr(0, i);
        auto ip = ipv4(
                ipv4::transfer_str_to_ipv4(ip_part),
                real_port, ip_part
        );

        return ip;
}

// TODO init with config
ipv4 ipv4::new_with_config() noexcept {
        auto ip = ipv4();

        return ip;
}

ipv4::~ipv4() {}

// check invalidation first, if invalid throw an InvalidIpv4Exception
// @param addr format is 'xxx.xxx.xxx.xxx' without port in tail
ipv4_i ipv4::transfer_str_to_ipv4(const std::string& addr) {
        std::vector<char> s;
        s.reserve(4);

        int cnt = 3;
        ipv4_i ret = 0;
        for (auto &c : addr) {
                if (c == '.') {
                        if (s.empty()) {
                                cnt--;
                                continue;
                        }

                        std::string builder {""};
                        for (auto &e : s) {
                                builder += e;
                        }

                        int num = atoi(builder.c_str());
                        if (num > 255) throw InvalidIpv4Address(addr);
                        ret |= (num << (cnt * 8));
                        s.clear();
                        cnt--;
                } else if (c >= '0' && c <= '9') {
                        s.push_back(c);
                } else {
                        throw InvalidIpv4Address(addr);
                }
        }

        std::string builder {""};
        for (auto &e : s) {
                builder += e;
        }
        int num = atoi(builder.c_str());
        if (num > 255) throw InvalidIpv4Address(addr);
        ret |= (num << (cnt * 8));

        if ((ret >> 12) == 0) throw InvalidIpv4Address(addr);
        return ret;
}

// TODO
std::string ipv4::transfer_ipv4_to_str(const ipv4_i& ip) {
        int head = ip >> 24;
        if (head == 0) {
                throw "can not transfer ipv4_i into string, invalid ip address " + std::to_string(ip);
        }

        std::string ret {std::to_string(head) + "."};
        ret +=    std::to_string((ip >> 16) & 0x000000FF) + "." 
                + std::to_string((ip >> 8) & 0x000000FF) + "."
                + std::to_string(ip & 0x000000FF);

        return ret;
}


#pragma region Tools

std::tuple<size_t, size_t> system_port_range() noexcept {
        char result_buffer[1024], command[1024];
        int rc = 0;
        FILE *fp;

        snprintf(command, sizeof(command), "");

        fp = popen(command, "r");
        if (fp == nullptr) {
                perror("execute popen failed.");
                exit(1);
        }


        std::string result;
        while (fgets(result_buffer, sizeof(result_buffer), fp) != nullptr) {
                if ('\n' == result_buffer[strlen(result_buffer) - 1]) {
                        result_buffer[strlen(result_buffer) - 1] = '\0';
                }
                result += std::string(result_buffer);
        }

        std::cout << result << std::endl;

        return {0, 0};
}

#endif
