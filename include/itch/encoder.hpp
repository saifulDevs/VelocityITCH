#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include "types.hpp"
#include "interfaces.hpp"

namespace itch {

// 1. MoldUDP64 Implementation of IEncoder Interface
class MoldUDP64Encoder : public IEncoder {
private:
    char session_name_[10]{'D', 'S', 'E', '_', 'I', 'T', 'C', 'H', '0', '1'};
    uint64_t next_sequence_num_{1ULL};

    static constexpr size_t MAX_PACKET_SIZE = 1472; // MTU max payload
    uint8_t packet_buffer_[MAX_PACKET_SIZE];
    size_t current_packet_len_{0};
    uint16_t current_msg_count_{0};

public:
    MoldUDP64Encoder() = default;

    inline void set_session_name(std::string_view session) {
        set_alpha(session_name_, session, 10);
    }

    void skip_sequence_numbers(uint64_t count) noexcept override {
        next_sequence_num_ += count;
    }

    uint64_t current_sequence_number() const noexcept override {
        return next_sequence_num_;
    }

    bool append_message(const void* msg_data, size_t msg_len, std::vector<uint8_t>& out_packet) override {
        size_t block_size = sizeof(MoldUDP64BlockHeader) + msg_len;
        bool buffer_flushed = false;

        if (current_packet_len_ + block_size > MAX_PACKET_SIZE) {
            finalize_packet(out_packet);
            buffer_flushed = true;
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

        return buffer_flushed;
    }

    bool finalize_packet(std::vector<uint8_t>& out_packet) override {
        if (current_msg_count_ == 0 || current_packet_len_ == 0) return false;

        auto* header = reinterpret_cast<MoldUDP64Header*>(packet_buffer_);
        std::memcpy(header->session, session_name_, 10);
        header->sequence_num = host_to_be64(next_sequence_num_);
        header->message_count = host_to_be16(current_msg_count_);

        out_packet.assign(packet_buffer_, packet_buffer_ + current_packet_len_);

        next_sequence_num_ += current_msg_count_;
        current_packet_len_ = 0;
        current_msg_count_ = 0;
        return true;
    }
};

// 2. Raw ITCH Implementation of IEncoder Interface
class RawItchEncoder : public IEncoder {
private:
    uint64_t seq_num_{1ULL};

public:
    void skip_sequence_numbers(uint64_t count) noexcept override { seq_num_ += count; }
    uint64_t current_sequence_number() const noexcept override { return seq_num_; }

    bool append_message(const void* msg_data, size_t msg_len, std::vector<uint8_t>& out_packet) override {
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(msg_data);
        out_packet.assign(ptr, ptr + msg_len);
        seq_num_++;
        return true; // Immediate pass-through
    }

    bool finalize_packet(std::vector<uint8_t>& out_packet) override {
        return !out_packet.empty();
    }
};

} // namespace itch
