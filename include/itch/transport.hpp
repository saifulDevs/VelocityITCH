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
#include <fcntl.h>
#include <vector>

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

// SoupBinTCP Server Implementation of ITransport Interface
class TcpServerTransport : public ITransport {
private:
    int server_fd_{-1};
    std::vector<int> client_fds_;
    std::vector<uint8_t> init_payload_;
    std::string ip_;
    uint16_t port_;

public:
    TcpServerTransport(std::string ip, uint16_t port)
        : ip_(std::move(ip)), port_(port) {
        init_server();
    }

    ~TcpServerTransport() override {
        for (int fd : client_fds_) {
            if (fd >= 0) ::close(fd);
        }
        if (server_fd_ >= 0) ::close(server_fd_);
    }

private:
    void init_server() {
        server_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd_ < 0) {
            throw std::runtime_error("Failed to create TCP socket: " + std::string(strerror(errno)));
        }

        int reuse = 1;
        if (::setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            std::cerr << "Warning: SO_REUSEADDR failed\n";
        }

        int flags = fcntl(server_fd_, F_GETFL, 0);
        fcntl(server_fd_, F_SETFL, flags | O_NONBLOCK);

        sockaddr_in addr{};
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        if (::inet_pton(AF_INET, ip_.c_str(), &addr.sin_addr) <= 0) {
            addr.sin_addr.s_addr = INADDR_ANY;
        }

        if (::bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            throw std::runtime_error("TCP Bind failed: " + std::string(strerror(errno)));
        }

        if (::listen(server_fd_, 10) < 0) {
            throw std::runtime_error("TCP Listen failed: " + std::string(strerror(errno)));
        }
        std::cout << "[TcpServer] Listening on " << ip_ << ":" << port_ << " for SoupBinTCP clients...\n";
    }

    void poll_new_clients() {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int cfd = ::accept(server_fd_, (struct sockaddr*)&client_addr, &client_len);
        if (cfd >= 0) {
            std::cout << "[TcpServer] Client connected from " << inet_ntoa(client_addr.sin_addr) << ":" << ntohs(client_addr.sin_port) << "\n";
            if (handle_handshake(cfd)) {
                client_fds_.push_back(cfd);
            } else {
                ::close(cfd);
            }
        }
    }

    bool handle_handshake(int cfd) {
        uint8_t header[3];
        if (::recv(cfd, header, 3, MSG_WAITALL) != 3) {
            std::cerr << "[TcpServer] Handshake read failed\n";
            return false;
        }

        uint16_t length = (uint16_t(header[0]) << 8) | uint16_t(header[1]);
        if (header[2] == 'L') {
            if (length > 1) {
                std::vector<uint8_t> body(length - 1);
                ::recv(cfd, body.data(), body.size(), MSG_WAITALL);
            }
            std::cout << "[TcpServer] Received Login Request ('L'). Sending Login Accept ('A').\n";
            
            uint16_t accept_len = 1 + 10 + 20;
            uint16_t be_len = htons(accept_len);
            std::vector<uint8_t> accept_pkt(2 + accept_len, ' ');
            std::memcpy(accept_pkt.data(), &be_len, 2);
            accept_pkt[2] = 'A';
            const char* session = "DSE_ITCH01";
            std::memcpy(accept_pkt.data() + 3, session, std::strlen(session));
            const char* seq = "                   1";
            std::memcpy(accept_pkt.data() + 13, seq, 20);

            if (::send(cfd, accept_pkt.data(), accept_pkt.size(), 0) < 0) {
                std::cerr << "[TcpServer] Failed to send Login Accept\n";
                return false;
            }

            // Send cached initialization payload (directories, ref prices, etc.)
            if (!init_payload_.empty()) {
                std::cout << "[TcpServer] Replaying " << init_payload_.size() << " bytes of directory/session init to client\n";
                ::send(cfd, init_payload_.data(), init_payload_.size(), 0);
            }
            return true;
        } else {
            std::cerr << "[TcpServer] Expected 'L' packet, got: " << header[2] << "\n";
            return false;
        }
    }

public:
    bool send_packet(const void* buf, size_t len) noexcept override {
        poll_new_clients();

        if (client_fds_.empty()) {
            // Buffer initial messages for future clients
            const uint8_t* p = static_cast<const uint8_t*>(buf);
            init_payload_.insert(init_payload_.end(), p, p + len);
            return true;
        }

        const uint8_t* ptr = static_cast<const uint8_t*>(buf);

        for (auto it = client_fds_.begin(); it != client_fds_.end(); ) {
            int cfd = *it;

            char drain_buf[64];
            while (::recv(cfd, drain_buf, sizeof(drain_buf), MSG_DONTWAIT) > 0) {}

            size_t remaining = len;
            const uint8_t* p = ptr;
            bool ok = true;

            while (remaining > 0) {
                ssize_t sent = ::send(cfd, p, remaining, MSG_NOSIGNAL);
                if (sent < 0) {
                    if (errno == EINTR) continue;
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        usleep(50);
                        continue;
                    }
                    ok = false;
                    break;
                }
                p += sent;
                remaining -= sent;
            }

            if (!ok) {
                std::cout << "[TcpServer] Client socket " << cfd << " disconnected.\n";
                ::close(cfd);
                it = client_fds_.erase(it);
            } else {
                ++it;
            }
        }
        return true;
    }
};

} // namespace itch
