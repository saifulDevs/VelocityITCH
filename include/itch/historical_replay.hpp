#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <chrono>
#include <thread>
#include "types.hpp"
#include "network_dispatcher.hpp"

namespace itch {

class HistoricalReplayEngine {
private:
    std::string file_path_;
    double replay_speed_{1.0}; // 1.0 = real-time, 2.0 = 2x speed, 0.0 = max speed

public:
    explicit HistoricalReplayEngine(std::string file_path, double replay_speed = 1.0)
        : file_path_(std::move(file_path)), replay_speed_(replay_speed) {}

    // Replay binary ITCH capture log file over network dispatcher
    bool replay(NetworkDispatcher& dispatcher) {
        std::ifstream file(file_path_, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "[REPLAY ERROR] Could not open ITCH capture file: " << file_path_ << "\n";
            return false;
        }

        std::cout << "[REPLAY] Replaying historical ITCH log file: " << file_path_ 
                  << " (Speed: " << (replay_speed_ == 0.0 ? "MAX" : std::to_string(replay_speed_) + "x") << ")...\n";

        uint8_t buffer[65536];
        uint64_t msg_count = 0;
        auto start_time = std::chrono::high_resolution_clock::now();

        while (file.read(reinterpret_cast<char*>(buffer), sizeof(uint16_t))) {
            uint16_t msg_len = be16_to_host(*reinterpret_cast<uint16_t*>(buffer));
            if (msg_len == 0 || msg_len > 1024) break;

            if (!file.read(reinterpret_cast<char*>(buffer + sizeof(uint16_t)), msg_len)) {
                break;
            }

            dispatcher.send_message(buffer + sizeof(uint16_t), msg_len);
            msg_count++;

            if (replay_speed_ > 0.0 && msg_count % 100 == 0) {
                auto delay_us = static_cast<int64_t>(1000.0 / replay_speed_);
                std::this_thread::sleep_for(std::chrono::microseconds(delay_us));
            }
        }

        dispatcher.flush_moldudp64_packet();

        auto end_time = std::chrono::high_resolution_clock::now();
        double elapsed_sec = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count() / 1000000.0;

        std::cout << "[REPLAY COMPLETE] Processed " << msg_count << " historical messages in " 
                  << elapsed_sec << " seconds.\n";
        return true;
    }
};

} // namespace itch
