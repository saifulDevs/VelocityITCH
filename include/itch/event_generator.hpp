#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <memory>
#include "types.hpp"
#include "prng.hpp"
#include "state_manager.hpp"

namespace itch {

struct Instrument {
    uint32_t orderbook_id;
    std::string symbol;
    std::string long_name;
    std::string isin;
    uint32_t base_price; // in cents/sub-units
};

class EventGenerator {
private:
    XorShift128Plus prng_;
    DenseOrderTracker order_tracker_;
    
    std::vector<Instrument> instruments_;
    uint64_t next_order_number_{1000000ULL};
    uint64_t next_match_id_{5000000ULL};
    
    // Configurable probability distribution thresholds (out of 100)
    uint32_t add_ratio_{70};     // 0..69: Add Order
    uint32_t exec_ratio_{20};    // 70..89: Order Executed
    uint32_t del_ratio_{5};      // 90..94: Order Delete
    uint32_t replace_ratio_{5};  // 95..99: Order Replace

    inline uint32_t get_current_timestamp() const noexcept {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        return static_cast<uint32_t>(nanos & 0xFFFFFFFF);
    }

public:
    explicit EventGenerator(size_t initial_capacity = 1000000)
        : prng_(987654321ULL, 123456789ULL), order_tracker_(initial_capacity) {
        
        // Populate standard Dhaka Stock Exchange (DSE) reference instruments
        instruments_ = {
            {101, "GP",         "Grameenphone Ltd.",            "BD0481GP0004", 32000},
            {102, "SQURPHARMA", "Square Pharmaceuticals Ltd.",   "BD0458SQ0002", 21550},
            {103, "BATBC",      "British American Tobacco BD",  "BD0450BAT009", 51800},
            {104, "BRACBANK",   "BRAC Bank Limited",            "BD0483BRAC01",  3850},
            {105, "BEXIMCO",    "Beximco Limited",              "BD0451BEX008", 11500}
        };
    }

    // Generate System Event 'O' (Start of Messages)
    SystemEventMessage generate_start_of_messages() const noexcept {
        SystemEventMessage msg{};
        msg.message_type = static_cast<char>(MessageType::SystemEvent);
        msg.timestamp = host_to_be32(get_current_timestamp());
        set_alpha(msg.group, "MAIN", 8);
        msg.event_code = static_cast<char>(SystemEventCode::StartOfMessages);
        msg.orderbook_id = host_to_be32(0);
        return msg;
    }

    // Generate System Event 'S' (Start of System Hours)
    SystemEventMessage generate_start_of_system_hours() const noexcept {
        SystemEventMessage msg{};
        msg.message_type = static_cast<char>(MessageType::SystemEvent);
        msg.timestamp = host_to_be32(get_current_timestamp());
        set_alpha(msg.group, "MAIN", 8);
        msg.event_code = static_cast<char>(SystemEventCode::StartOfSystemHours);
        msg.orderbook_id = host_to_be32(0);
        return msg;
    }

    // Generate Orderbook Directory [R] for all registered instruments
    std::vector<OrderbookDirectoryMessage> generate_directory() const {
        std::vector<OrderbookDirectoryMessage> directories;
        directories.reserve(instruments_.size());

        for (const auto& inst : instruments_) {
            OrderbookDirectoryMessage msg{};
            msg.message_type = static_cast<char>(MessageType::OrderbookDirectory);
            msg.timestamp = host_to_be32(get_current_timestamp());
            msg.orderbook_id = host_to_be32(inst.orderbook_id);
            set_alpha(msg.symbol, inst.symbol, 32);
            set_alpha(msg.long_name, inst.long_name, 32);
            set_alpha(msg.isin, inst.isin, 12);
            msg.financial_status = 'A';
            set_alpha(msg.currency, "BDT", 3);
            msg.price_decimals = host_to_be16(2);
            msg.nominal_decimals = host_to_be16(2);
            msg.lot_size = host_to_be32(100);
            directories.push_back(msg);
        }
        return directories;
    }

    // High-speed Next Event Generator (Populates buffer with raw packed message)
    // Returns length of written message in bytes
    size_t generate_next_event(uint8_t* buffer) {
        uint32_t roll = prng_.next_range(0, 99);

        // Fall back to Add Order if no active orders exist to execute/delete/replace
        if (order_tracker_.empty() || roll < add_ratio_) {
            return generate_add_order(buffer);
        } else if (roll < add_ratio_ + exec_ratio_) {
            return generate_order_executed(buffer);
        } else if (roll < add_ratio_ + exec_ratio_ + del_ratio_) {
            return generate_order_delete(buffer);
        } else {
            return generate_order_replace(buffer);
        }
    }

private:
    size_t generate_add_order(uint8_t* buffer) {
        auto* msg = reinterpret_cast<AddOrderMessage*>(buffer);
        msg->message_type = static_cast<char>(MessageType::AddOrder);
        msg->timestamp = host_to_be32(get_current_timestamp());
        
        uint64_t order_num = next_order_number_++;
        msg->order_number = host_to_be64(order_num);

        const auto& inst = instruments_[prng_.next_range(0, instruments_.size() - 1)];
        msg->orderbook_id = host_to_be32(inst.orderbook_id);
        msg->side = (prng_.next_u32() % 2 == 0) ? 'B' : 'S';
        
        uint32_t qty = static_cast<uint32_t>(prng_.next_range(1, 50) * 100);
        msg->quantity = host_to_be32(qty);

        int32_t delta = static_cast<int32_t>(prng_.next_range(0, 100)) - 50;
        uint32_t price = static_cast<uint32_t>(std::max(10, static_cast<int32_t>(inst.base_price) + delta));
        msg->price = host_to_be32(price);

        set_alpha(msg->participant_id, "DSE1", 4);

        // Track active order
        order_tracker_.insert(order_num);
        return sizeof(AddOrderMessage);
    }

    size_t generate_order_executed(uint8_t* buffer) {
        uint64_t order_num = 0;
        if (!order_tracker_.get_random_order(prng_, order_num)) {
            return generate_add_order(buffer);
        }

        auto* msg = reinterpret_cast<OrderExecutedMessage*>(buffer);
        msg->message_type = static_cast<char>(MessageType::OrderExecuted);
        msg->timestamp = host_to_be32(get_current_timestamp());
        msg->order_number = host_to_be64(order_num);
        
        uint32_t exec_qty = static_cast<uint32_t>(prng_.next_range(1, 10) * 100);
        msg->executed_quantity = host_to_be32(exec_qty);
        msg->match_id = host_to_be64(next_match_id_++);

        // Remove executed order with 50% probability (simulating complete fill vs partial fill)
        if (prng_.next_u32() % 2 == 0) {
            order_tracker_.remove(order_num);
        }
        return sizeof(OrderExecutedMessage);
    }

    size_t generate_order_delete(uint8_t* buffer) {
        uint64_t order_num = 0;
        if (!order_tracker_.get_random_order(prng_, order_num)) {
            return generate_add_order(buffer);
        }

        auto* msg = reinterpret_cast<OrderDeleteMessage*>(buffer);
        msg->message_type = static_cast<char>(MessageType::OrderDelete);
        msg->timestamp = host_to_be32(get_current_timestamp());
        msg->order_number = host_to_be64(order_num);

        order_tracker_.remove(order_num);
        return sizeof(OrderDeleteMessage);
    }

    size_t generate_order_replace(uint8_t* buffer) {
        uint64_t orig_order_num = 0;
        if (!order_tracker_.get_random_order(prng_, orig_order_num)) {
            return generate_add_order(buffer);
        }

        auto* msg = reinterpret_cast<OrderReplaceMessage*>(buffer);
        msg->message_type = static_cast<char>(MessageType::OrderReplace);
        msg->timestamp = host_to_be32(get_current_timestamp());
        msg->orig_order_number = host_to_be64(orig_order_num);
        
        uint64_t new_order_num = next_order_number_++;
        msg->new_order_number = host_to_be64(new_order_num);

        uint32_t new_qty = static_cast<uint32_t>(prng_.next_range(1, 50) * 100);
        msg->quantity = host_to_be32(new_qty);
        uint32_t new_price = static_cast<uint32_t>(prng_.next_range(1000, 50000));
        msg->price = host_to_be32(new_price);

        order_tracker_.replace(orig_order_num, new_order_num);
        return sizeof(OrderReplaceMessage);
    }

public:
    inline size_t active_orders_count() const noexcept {
        return order_tracker_.size();
    }
};

} // namespace itch
