#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string_view>
#include <string>

namespace itch {

// 1. Abstract Transport Interface (UDP Multicast, UDP Unicast, TCP, Shared Memory)
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool send_packet(const void* buf, size_t len) noexcept = 0;
};

// 2. Abstract Encoder Interface (MoldUDP64, Raw ITCH, SoupBinTCP)
class IEncoder {
public:
    virtual ~IEncoder() = default;
    virtual bool append_message(const void* msg_data, size_t msg_len, std::vector<uint8_t>& out_packet) = 0;
    virtual bool finalize_packet(std::vector<uint8_t>& out_packet) = 0;
    virtual void skip_sequence_numbers(uint64_t count) noexcept = 0;
    virtual uint64_t current_sequence_number() const noexcept = 0;
};

// 3. Abstract Stream Recorder Interface (Binary File, PCAP, CSV)
class IRecorder {
public:
    virtual ~IRecorder() = default;
    virtual bool open(const std::string& file_path) = 0;
    virtual void close() = 0;
    virtual void record_message(const void* msg_data, size_t msg_len) = 0;
    virtual bool is_active() const noexcept = 0;
};

// 4. Abstract Market Scenario Interface (Synthetic Generator, Historical Replay, Auction Stress)
class IMarketScenario {
public:
    virtual ~IMarketScenario() = default;
    virtual size_t next_event(uint8_t* buffer) = 0;
    virtual bool is_complete() const noexcept = 0;
};

} // namespace itch
