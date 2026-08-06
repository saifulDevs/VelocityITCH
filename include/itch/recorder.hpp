#pragma once

#include <string>
#include <fstream>
#include <iostream>
#include "types.hpp"
#include "interfaces.hpp"
#include "logger.hpp"

namespace itch {

// Polymorphic Implementation of IRecorder Interface
class BinaryStreamRecorder : public IRecorder {
private:
    std::ofstream file_;
    bool is_active_{false};

public:
    BinaryStreamRecorder() = default;

    explicit BinaryStreamRecorder(const std::string& file_path) {
        open(file_path);
    }

    ~BinaryStreamRecorder() override {
        close();
    }

    bool open(const std::string& file_path) override {
        if (file_path.empty()) return false;
        file_.open(file_path, std::ios::binary | std::ios::out);
        is_active_ = file_.is_open();
        if (is_active_) {
            Logger::info("StreamRecorder: Recording live ITCH stream to binary file: " + file_path);
        } else {
            Logger::error("StreamRecorder: Failed to open recording file: " + file_path);
        }
        return is_active_;
    }

    void close() override {
        if (is_active_ && file_.is_open()) {
            file_.close();
            is_active_ = false;
            Logger::info("StreamRecorder: Recording closed successfully.");
        }
    }

    void record_message(const void* msg_data, size_t msg_len) override {
        if (!is_active_) return;

        uint16_t be_len = host_to_be16(static_cast<uint16_t>(msg_len));
        file_.write(reinterpret_cast<const char*>(&be_len), sizeof(be_len));
        file_.write(reinterpret_cast<const char*>(msg_data), msg_len);
    }

    bool is_active() const noexcept override { return is_active_; }
};

} // namespace itch
