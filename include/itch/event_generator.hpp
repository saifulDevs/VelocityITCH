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

} // namespace itch

#include "real_instruments.hpp"

namespace itch {

class EventGenerator {
private:
    XorShift128Plus prng_;
    DenseOrderTracker order_tracker_;
    ProtocolMode protocol_mode_;
    
    std::vector<Instrument> instruments_;
    uint64_t next_order_number_{1000000ULL};
    uint64_t next_match_id_{5000000ULL};
    
    // Configurable probability distribution thresholds (out of 100)
    uint32_t add_ratio_{50};     // 0..49: Add Order
    uint32_t exec_ratio_{40};    // 50..89: Order Executed
    uint32_t del_ratio_{5};      // 90..94: Order Delete
    uint32_t replace_ratio_{5};  // 95..99: Order Replace

public:
    void set_ratios(uint32_t add_pct, uint32_t exec_pct, uint32_t del_pct = 5, uint32_t replace_pct = 5) {
        add_ratio_ = add_pct;
        exec_ratio_ = exec_pct;
        del_ratio_ = del_pct;
        replace_ratio_ = replace_pct;
    }

    inline uint32_t get_current_timestamp() const noexcept {
        auto now = std::chrono::high_resolution_clock::now();
        auto nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
        return static_cast<uint32_t>(nanos & 0xFFFFFFFF);
    }

public:
    explicit EventGenerator(ProtocolMode mode, size_t initial_capacity = 1000000)
        : prng_(987654321ULL, 123456789ULL), order_tracker_(initial_capacity), protocol_mode_(mode) {
        
        if (protocol_mode_ == ProtocolMode::Index) {
            instruments_ = {
                {100, "DSEX", "DSE Broad Index", "BD000000DSEX", 600000},
                {102, "DSES", "DSE Shariah Index", "BD000000DSES", 130000},
                {101, "DS30", "DSE 30 Index", "BD000000DS30", 210000}
            };
        } else {
            instruments_ = itch::get_real_instruments();
        }
    }

    // Generate System Event 'O' (Start of Messages)
    SystemEventMessage generate_start_of_messages() const noexcept {
        SystemEventMessage msg{};
        msg.message_type = static_cast<char>(MessageType::SystemEvent);
        msg.timestamp = host_to_be32(get_current_timestamp());
        set_alpha(msg.group, "PUBLIC", 8);
        msg.event_code = static_cast<char>(SystemEventCode::StartOfMessages);
        msg.orderbook_id = host_to_be32(0);
        return msg;
    }

    // Generate System Event 'S' (Start of System Hours)
    SystemEventMessage generate_start_of_system_hours() const noexcept {
        SystemEventMessage msg{};
        msg.message_type = static_cast<char>(MessageType::SystemEvent);
        msg.timestamp = host_to_be32(get_current_timestamp());
        set_alpha(msg.group, "PUBLIC", 8);
        msg.event_code = static_cast<char>(SystemEventCode::StartOfSystemHours);
        msg.orderbook_id = host_to_be32(0);
        return msg;
    }

    // Generate System Event 'Q' (Start of Market Hours / Open)
    SystemEventMessage generate_start_of_market_hours() const noexcept {
        SystemEventMessage msg{};
        msg.message_type = static_cast<char>(MessageType::SystemEvent);
        msg.timestamp = host_to_be32(get_current_timestamp());
        set_alpha(msg.group, "PUBLIC", 8);
        msg.event_code = 'Q';
        msg.orderbook_id = host_to_be32(0);
        return msg;
    }

    // Generate Company Directory [P] for all registered instruments
    std::vector<CompanyDirectoryMessage> generate_company_directories() const {
        std::vector<CompanyDirectoryMessage> directories;
        directories.reserve(instruments_.size());

        for (const auto& inst : instruments_) {
            CompanyDirectoryMessage msg{};
            msg.message_type = 'P';
            msg.timestamp = host_to_be32(get_current_timestamp());
            msg.company = host_to_be32(inst.orderbook_id);
            set_alpha(msg.company_name, inst.long_name, 30);
            set_alpha(msg.company_id, inst.symbol, 30);
            directories.push_back(msg);
        }
        return directories;
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
            msg.price_type = '1';
            set_alpha(msg.isin, inst.isin, 12);
            set_alpha(msg.sec_code, inst.symbol, 12);
            set_alpha(msg.currency, "BDT", 3);
            if (protocol_mode_ == ProtocolMode::Index) {
                set_alpha(msg.group, "INDEX", 8);
            } else {
                set_alpha(msg.group, "PUBLIC", 8);
            }
            msg.minimum_quantity = host_to_be64(100);
            msg.quantity_tick_size_table_id = host_to_be32(1);
            msg.price_tick_size_table_id = host_to_be32(1);
            msg.price_decimals = host_to_be32(2);
            msg.delisting_date = host_to_be32(0);
            msg.delisting_time = host_to_be32(0);
            bool is_idx = (protocol_mode_ == ProtocolMode::Index || inst.symbol == "DSEX" || inst.symbol == "DSES" || inst.symbol == "DS30");
            msg.market_type = is_idx ? 'I' : 'S';
            msg.company = host_to_be32(inst.orderbook_id);
            msg.listing_type = '1';
            set_alpha(msg.sector, "TECH", 12);
            set_alpha(msg.instr, "EQ", 12);
            set_alpha(msg.security_name, inst.long_name, 60);
            msg.maturity_date = host_to_be32(0);
            msg.yield_decimals = host_to_be32(0);
            
            directories.push_back(msg);
        }
        return directories;
    }

    std::vector<OrderbookTradingActionMessage> generate_trading_actions() const {
        std::vector<OrderbookTradingActionMessage> actions;
        for (const auto& inst : instruments_) {
            OrderbookTradingActionMessage msg{};
            msg.message_type = 'H';
            msg.timestamp = host_to_be32(get_current_timestamp());
            msg.orderbook_id = host_to_be32(inst.orderbook_id);
            msg.trading_state = 'T';
            msg.reason = ' ';
            actions.push_back(msg);
        }
        return actions;
    }

    std::vector<AddOrderMessage> generate_ref_prices() const {
        std::vector<AddOrderMessage> refs;
        for (const auto& inst : instruments_) {
            AddOrderMessage msg{};
            msg.message_type = 'A';
            msg.timestamp = host_to_be32(get_current_timestamp());
            msg.order_number = host_to_be64(0);
            msg.side = 'B';
            msg.quantity = host_to_be64(0);
            msg.orderbook_id = host_to_be32(inst.orderbook_id);
            msg.price = host_to_be32(inst.base_price);
            msg.yield = host_to_be32(0);
            refs.push_back(msg);
        }
        return refs;
    }


    // High-speed Next Event Generator (Populates buffer with raw packed message)
    // Returns length of written message in bytes
    size_t generate_next_event(uint8_t* buffer) {
        if (protocol_mode_ == ProtocolMode::Index) {
            return generate_index_value(buffer);
        }

        uint32_t roll = prng_.next_range(0, 99);

        // 5% chance of generating an Index value update in unified stream
        if (roll < 5) {
            return generate_index_value(buffer);
        }

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
    size_t generate_index_value(uint8_t* buffer) {
        auto* msg = reinterpret_cast<IndexValueMessage*>(buffer);
        msg->message_type = 'Z';
        msg->timestamp = host_to_be32(get_current_timestamp());

        static const uint32_t index_ob_ids[] = {100, 101, 102};
        uint32_t chosen_ob = index_ob_ids[prng_.next_range(0, 2)];
        msg->index_orderbook = host_to_be32(chosen_ob);
        
        // Random fluctuation for index value
        int32_t delta = static_cast<int32_t>(prng_.next_range(0, 500)) - 250;
        uint64_t val = static_cast<uint64_t>(std::max(1000, static_cast<int32_t>(600000) + delta));
        msg->value = host_to_be64(val);
        
        return sizeof(IndexValueMessage);
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
        
        uint64_t qty = static_cast<uint64_t>(prng_.next_range(1, 50) * 100);
        msg->quantity = host_to_be64(qty);

        int32_t delta = static_cast<int32_t>(prng_.next_range(0, 100)) - 50;
        uint32_t price = static_cast<uint32_t>(std::max(10, static_cast<int32_t>(inst.base_price) + delta));
        msg->price = host_to_be32(price);

        msg->yield = host_to_be32(0);

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
        
        uint64_t exec_qty = static_cast<uint64_t>(prng_.next_range(1, 10) * 100);
        msg->executed_quantity = host_to_be64(exec_qty);
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

        uint64_t new_qty = static_cast<uint64_t>(prng_.next_range(1, 50) * 100);
        msg->quantity = host_to_be64(new_qty);
        uint32_t new_price = static_cast<uint32_t>(prng_.next_range(1000, 50000));
        msg->price = host_to_be32(new_price);
        msg->yield = host_to_be32(0);

        order_tracker_.replace(orig_order_num, new_order_num);
        return sizeof(OrderReplaceMessage);
    }

public:
    inline size_t active_orders_count() const noexcept {
        return order_tracker_.size();
    }
};

} // namespace itch
