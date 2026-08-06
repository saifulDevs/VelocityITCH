#pragma once

#include <string>
#include <iostream>
#include <cstring>
#include <stdexcept>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include "config.hpp"
#include "interfaces.hpp"

namespace itch {

// Polymorphic POSIX UDP Implementation of ITransport Interface
class UdpTransport : public ITransport {
private:
    int socket_fd_{-1};
    sockaddr_in dest_addr_{};
    NetworkMode mode_{NetworkMode::Multicast};
    std::string ip_;
    uint16_t port_;

public:
    UdpTransport(std::string ip, uint16_t port, NetworkMode mode)
        : ip_(std::move(ip)), port_(port), mode_(mode) {
        init_socket();
    }

    ~UdpTransport() override {
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
        }
    }

private:
    void init_socket() {
        socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            throw std::runtime_error("Failed to create UDP socket: " + std::string(strerror(errno)));
        }

        int reuse = 1;
        if (::setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            std::cerr << "Warning: SO_REUSEADDR failed\n";
        }

        int sndbuf = 16 * 1024 * 1024;
        if (::setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
            std::cerr << "Warning: SO_SNDBUF failed\n";
        }

        std::memset(&dest_addr_, 0, sizeof(dest_addr_));
        dest_addr_.sin_family = AF_INET;
        dest_addr_.sin_port = htons(port_);
        if (::inet_pton(AF_INET, ip_.c_str(), &dest_addr_.sin_addr) <= 0) {
            throw std::runtime_error("Invalid IP address: " + ip_);
        }

        if (mode_ == NetworkMode::Multicast) {
            unsigned char ttl = 64;
            if (::setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_TTL, &ttl, sizeof(ttl)) < 0) {
                std::cerr << "Warning: IP_MULTICAST_TTL failed\n";
            }

            unsigned char loop = 1;
            if (::setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
                std::cerr << "Warning: IP_MULTICAST_LOOP failed\n";
            }
        }
    }

public:
    bool send_packet(const void* buf, size_t len) noexcept override {
        ssize_t sent = ::sendto(socket_fd_, buf, len, 0,
                               reinterpret_cast<const sockaddr*>(&dest_addr_), sizeof(dest_addr_));
        return (sent >= 0);
    }
};

} // namespace itch
