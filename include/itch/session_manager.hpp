#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include "types.hpp"

namespace itch {

enum class MarketSessionState {
    PreOpen,
    OpeningAuction,
    ContinuousTrading,
    TradingHalt,
    TradingResume,
    ClosingAuction,
    MarketClose
};

class SessionManager {
private:
    MarketSessionState current_state_{MarketSessionState::PreOpen};

public:
    SessionManager() = default;

    inline MarketSessionState current_state() const noexcept {
        return current_state_;
    }

    inline void transition_to(MarketSessionState new_state) noexcept {
        current_state_ = new_state;
    }

    static std::string state_to_string(MarketSessionState state) {
        switch (state) {
            case MarketSessionState::PreOpen:            return "PRE_OPEN";
            case MarketSessionState::OpeningAuction:     return "OPENING_AUCTION";
            case MarketSessionState::ContinuousTrading:  return "CONTINUOUS_TRADING";
            case MarketSessionState::TradingHalt:        return "TRADING_HALT";
            case MarketSessionState::TradingResume:      return "TRADING_RESUME";
            case MarketSessionState::ClosingAuction:     return "CLOSING_AUCTION";
            case MarketSessionState::MarketClose:        return "MARKET_CLOSE";
        }
        return "UNKNOWN";
    }

    // Helper to generate Session Event message according to state
    SystemEventMessage create_system_event(MarketSessionState state, uint32_t timestamp_nanos) const noexcept {
        SystemEventMessage msg{};
        msg.message_type = static_cast<char>(MessageType::SystemEvent);
        msg.timestamp = host_to_be32(timestamp_nanos);
        set_alpha(msg.group, "MAIN", 8);
        msg.orderbook_id = host_to_be32(0);

        switch (state) {
            case MarketSessionState::PreOpen:
                msg.event_code = 'P'; // Pre-open
                break;
            case MarketSessionState::OpeningAuction:
                msg.event_code = 'Q'; // Auction
                break;
            case MarketSessionState::ContinuousTrading:
                msg.event_code = static_cast<char>(SystemEventCode::StartOfSystemHours);
                break;
            case MarketSessionState::TradingHalt:
                msg.event_code = 'H'; // Halt
                break;
            case MarketSessionState::TradingResume:
                msg.event_code = 'R'; // Resume
                break;
            case MarketSessionState::ClosingAuction:
                msg.event_code = 'A'; // Closing Auction
                break;
            case MarketSessionState::MarketClose:
                msg.event_code = static_cast<char>(SystemEventCode::EndOfSystemHours);
                break;
        }
        return msg;
    }
};

} // namespace itch
