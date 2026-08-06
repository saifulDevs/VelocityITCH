#pragma once

#include <vector>
#include <deque>
#include <chrono>
#include <thread>
#include <iostream>
#include "prng.hpp"

namespace itch {

struct ChaosConfig {
    double packet_drop_rate{0.0};    // Percentage (0.0 to 100.0%)
    double reorder_rate{0.0};        // Percentage of packets to reorder
    uint64_t sequence_gap_step{0};   // Skip sequence numbers every N packets (0 = disabled)
    uint32_t sequence_gap_size{5};   // Number of sequence IDs to skip when gap occurs
    uint64_t max_jitter_us{0};       // Max latency jitter delay in microseconds
};

class ChaosInjector {
private:
    ChaosConfig config_;
    XorShift128Plus prng_;
    uint64_t packet_counter_{0};

public:
    explicit ChaosInjector(ChaosConfig config = {})
        : config_(config), prng_(7777777ULL, 8888888ULL) {}

    inline void update_config(const ChaosConfig& config) noexcept {
        config_ = config;
    }

    // Determine if current packet should be dropped (Packet Loss Simulation)
    inline bool should_drop_packet() noexcept {
        if (config_.packet_drop_rate <= 0.0) return false;
        double rand_val = prng_.next_double() * 100.0;
        return rand_val < config_.packet_drop_rate;
    }

    // Determine sequence gap increment for MoldUDP64 header
    inline uint64_t evaluate_sequence_gap() noexcept {
        packet_counter_++;
        if (config_.sequence_gap_step > 0 && (packet_counter_ % config_.sequence_gap_step == 0)) {
            std::cout << "[CHAOS INJECTOR] Injecting MoldUDP64 sequence gap (skipping " 
                      << config_.sequence_gap_size << " sequence numbers)\n";
            return config_.sequence_gap_size;
        }
        return 0;
    }

    // Apply network jitter delay if enabled
    inline void apply_jitter() noexcept {
        if (config_.max_jitter_us == 0) return;
        uint64_t jitter_us = prng_.next_range(10, config_.max_jitter_us);
        std::this_thread::sleep_for(std::chrono::microseconds(jitter_us));
    }
};

} // namespace itch
