#pragma once

#include <string>
#include <cstdint>
#include <iostream>

namespace itch {

enum class NetworkMode { Multicast, Unicast };
enum class FramingMode { Raw, MoldUDP64 };

struct Config {
    std::string ip{"239.255.0.1"};
    uint16_t port{12345};
    NetworkMode net_mode{NetworkMode::Multicast};
    FramingMode framing_mode{FramingMode::MoldUDP64};
    uint64_t target_rate_mps{1000000};
    uint64_t max_count{0};

    // Market Kinetics & Session Lifecycle
    std::string replay_file{""};
    double replay_speed{1.0};
    bool enable_session_flow{false};
    bool enable_price_walk{false};
    bool enable_diurnal{false};

    // Recording Output
    std::string record_file{""};

    // Network Chaos & Fault Injection
    double packet_drop_rate{0.0};    // 0.0 to 100.0%
    uint64_t sequence_gap_step{0};   // Skip gap every N packets
    uint32_t sequence_gap_size{5};   // Skip size
    uint64_t max_jitter_us{0};       // Latency jitter delay in micros

    void print() const {
        std::cout << "=================================================================\n"
                  << "   VelocityITCH Engine Configuration\n"
                  << "=================================================================\n"
                  << " Target Address   : " << ip << ":" << port << " (" 
                  << (net_mode == NetworkMode::Multicast ? "UDP Multicast" : "UDP Unicast") << ")\n"
                  << " Framing Mode     : " << (framing_mode == FramingMode::MoldUDP64 ? "MoldUDP64" : "Raw ITCH") << "\n"
                  << " Target MPS Rate  : " << (target_rate_mps == 0 ? "UNLIMITED (Max Speed)" : std::to_string(target_rate_mps) + " MPS") << "\n"
                  << " Mode             : " << (replay_file.empty() ? "Synthetic Generator" : "Historical Replay: " + replay_file) << "\n"
                  << " Stream Recorder  : " << (record_file.empty() ? "Disabled" : "Recording to " + record_file) << "\n"
                  << " Market Kinetics  : " << (enable_price_walk ? "Random Walk Price | " : "") << (enable_diurnal ? "Diurnal Volume | " : "") << (enable_session_flow ? "Session Lifecycle" : "Continuous") << "\n"
                  << " Chaos Faults     : Drop: " << packet_drop_rate << "% | Seq Gap Step: " << sequence_gap_step << " | Jitter: " << max_jitter_us << "us\n"
                  << "=================================================================\n\n";
    }
};

} // namespace itch
