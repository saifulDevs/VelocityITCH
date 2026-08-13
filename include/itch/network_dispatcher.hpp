#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <iostream>
#include <cstring>
#include <stdexcept>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#include "types.hpp"
#include "config.hpp"
#include "chaos_injector.hpp"

namespace itch {

class NetworkDispatcher {
private:
    int socket_fd_{-1};
    sockaddr_in dest_addr_{};
    NetworkMode mode_{NetworkMode::Multicast};
    FramingMode framing_{FramingMode::MoldUDP64};

    std::string ip_;
    uint16_t port_;
    
    // MoldUDP64 state
    char session_name_[10]{'D', 'S', 'E', '_', 'I', 'T', 'C', 'H', '0', '1'};
    uint64_t next_sequence_num_{1ULL};

    // Buffer for MoldUDP64 batch packet construction (Max MTU payload 1472 bytes)
    static constexpr size_t MAX_PACKET_SIZE = 1472;
    uint8_t packet_buffer_[MAX_PACKET_SIZE];
    size_t current_packet_len_{0};
    uint16_t current_msg_count_{0};

    // Rate Limiter
    uint64_t target_mps_{0}; // 0 = unlimited / max throughput
    std::chrono::high_resolution_clock::time_point last_rate_check_;
    uint64_t msgs_in_current_window_{0};

    // Chaos & Fault Injector
    ChaosInjector chaos_injector_;

public:
    NetworkDispatcher(std::string ip, uint16_t port, NetworkMode mode, FramingMode framing, 
                      uint64_t target_mps = 0, ChaosConfig chaos_cfg = {})
        : ip_(std::move(ip)), port_(port), mode_(mode), framing_(framing), 
          target_mps_(target_mps), chaos_injector_(chaos_cfg) {
        
        init_socket();
        last_rate_check_ = std::chrono::high_resolution_clock::now();
    }

    ~NetworkDispatcher() {
        if (socket_fd_ >= 0) {
            ::close(socket_fd_);
        }
    }

    void update_chaos_config(const ChaosConfig& cfg) {
        chaos_injector_.update_config(cfg);
    }

private:
    void init_socket() {
        socket_fd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
        if (socket_fd_ < 0) {
            throw std::runtime_error("Failed to create UDP socket: " + std::string(strerror(errno)));
        }

        int reuse = 1;
        if (::setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
            std::cerr << "Warning: SO_REUSEADDR setsockopt failed\n";
        }

        int sndbuf = 16 * 1024 * 1024;
        if (::setsockopt(socket_fd_, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf)) < 0) {
            std::cerr << "Warning: SO_SNDBUF setsockopt failed\n";
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
                std::cerr << "Warning: IP_MULTICAST_TTL setsockopt failed\n";
            }

            unsigned char loop = 1;
            if (::setsockopt(socket_fd_, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop)) < 0) {
                std::cerr << "Warning: IP_MULTICAST_LOOP setsockopt failed\n";
            }
        }
    }

public:
    void send_message(const void* msg_data, size_t msg_len) {
        if (framing_ == FramingMode::Raw) {
            send_raw_packet(msg_data, msg_len);
        } else {
            size_t block_size = sizeof(MoldUDP64BlockHeader) + msg_len;

            if (current_packet_len_ + block_size > MAX_PACKET_SIZE) {
                flush_moldudp64_packet();
            }

            if (current_packet_len_ == 0) {
                current_packet_len_ = sizeof(MoldUDP64Header);
                current_msg_count_ = 0;
            }

            auto* block = reinterpret_cast<MoldUDP64BlockHeader*>(packet_buffer_ + current_packet_len_);
            block->message_length = host_to_be16(static_cast<uint16_t>(msg_len));
            current_packet_len_ += sizeof(MoldUDP64BlockHeader);

            std::memcpy(packet_buffer_ + current_packet_len_, msg_data, msg_len);
            current_packet_len_ += msg_len;
            current_msg_count_++;
        }

        rate_limit_throttle();
    }

    inline void send_raw_packet(const void* buf, size_t len) {
        if (chaos_injector_.should_drop_packet()) {
            return;
        }

        chaos_injector_.apply_jitter();

        ssize_t sent = ::sendto(socket_fd_, buf, len, 0,
                               reinterpret_cast<const sockaddr*>(&dest_addr_), sizeof(dest_addr_));
        if (sent < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // Transient non-fatal error
            }
        }
    }

    void flush_moldudp64_packet() {
        if (current_msg_count_ == 0 || current_packet_len_ == 0) return;

        uint64_t gap_skip = chaos_injector_.evaluate_sequence_gap();
        next_sequence_num_ += gap_skip;

        auto* header = reinterpret_cast<MoldUDP64Header*>(packet_buffer_);
        std::memcpy(header->session, session_name_, 10);
        header->sequence_num = host_to_be64(next_sequence_num_);
        header->message_count = host_to_be16(current_msg_count_);

        send_raw_packet(packet_buffer_, current_packet_len_);

        next_sequence_num_ += current_msg_count_;
        current_packet_len_ = 0;
        current_msg_count_ = 0;
    }

    void send_batch(const std::vector<std::pair<const void*, size_t>>& batch) {
        for (const auto& [data, len] : batch) {
            send_message(data, len);
        }
    }

private:
    inline void rate_limit_throttle() {
        if (target_mps_ == 0) return;

        msgs_in_current_window_++;
        uint64_t batch_size = std::min<uint64_t>(1000ULL, std::max<uint64_t>(1ULL, target_mps_ / 20));
        if (msgs_in_current_window_ >= batch_size) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(now - last_rate_check_).count();
            
            double expected_us = (static_cast<double>(batch_size) / static_cast<double>(target_mps_)) * 1000000.0;
            
            if (elapsed_us < expected_us) {
                auto sleep_us = static_cast<int64_t>(expected_us - elapsed_us);
                if (sleep_us > 100) {
                    std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
                } else {
                    while (std::chrono::duration_cast<std::chrono::microseconds>(
                               std::chrono::high_resolution_clock::now() - last_rate_check_).count() < expected_us) {
                        #if defined(__x86_64__) || defined(_M_X64)
                        __builtin_ia32_pause();
                        #endif
                    }
                }
            }

            last_rate_check_ = std::chrono::high_resolution_clock::now();
            msgs_in_current_window_ = 0;
        }
    }
};

} // namespace itch
