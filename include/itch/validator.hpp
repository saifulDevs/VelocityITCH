#pragma once

#include <cstdint>
#include <cstddef>
#include <iostream>
#include "types.hpp"
#include "logger.hpp"

namespace itch {

class EventValidator {
public:
    static bool validate_event(const void* data, size_t len) noexcept {
        if (data == nullptr || len < 1) {
            Logger::error("Validation Failed: Null or zero-length message data");
            return false;
        }

        char msg_type = *reinterpret_cast<const char*>(data);

        switch (msg_type) {
            case 'S':
                if (len != sizeof(SystemEventMessage)) { Logger::error("Validation Failed: SystemEventMessage length mismatch. Expected " + std::to_string(sizeof(SystemEventMessage)) + ", got " + std::to_string(len)); return false; }
                return true;
            case 'O': // SystemEvent Start of Messages
                if (len != sizeof(SystemEventMessage)) { Logger::error("Validation Failed: SystemEventMessage (O) length mismatch."); return false; }
                return true;
            case 'P':
                if (len != sizeof(CompanyDirectoryMessage)) { Logger::error("Validation Failed: CompanyDirectoryMessage length mismatch."); return false; }
                return true;
            case 'R':
                if (len != sizeof(OrderbookDirectoryMessage)) { Logger::error("Validation Failed: OrderbookDirectoryMessage length mismatch."); return false; }
                return true;
            case 'A': {
                if (len != sizeof(AddOrderMessage)) { Logger::error("Validation Failed: AddOrderMessage length mismatch."); return false; }
                auto* add_msg = reinterpret_cast<const AddOrderMessage*>(data);
                // Allow order_number == 0 && quantity == 0 for Reference Price messages
                if (be64_to_host(add_msg->order_number) == 0 && be64_to_host(add_msg->quantity) != 0) { 
                    Logger::error("Validation Failed: AddOrder order_number == 0 with non-zero quantity"); return false; 
                }
                if (be32_to_host(add_msg->price) == 0) { Logger::error("Validation Failed: AddOrder price == 0"); return false; }
                if (add_msg->side != 'B' && add_msg->side != 'S') { Logger::error(std::string("Validation Failed: AddOrder side invalid: ") + add_msg->side); return false; }
                return true;
            }
            case 'H': {
                if (len != sizeof(OrderbookTradingActionMessage)) { Logger::error("Validation Failed: OrderbookTradingActionMessage length mismatch."); return false; }
                return true;
            }
            case 'E': {
                if (len != sizeof(OrderExecutedMessage)) { Logger::error("Validation Failed: OrderExecutedMessage length mismatch."); return false; }
                auto* exec_msg = reinterpret_cast<const OrderExecutedMessage*>(data);
                if (be64_to_host(exec_msg->order_number) == 0) { Logger::error("Validation Failed: OrderExecuted order_number == 0"); return false; }
                if (be64_to_host(exec_msg->executed_quantity) == 0) { Logger::error("Validation Failed: OrderExecuted quantity == 0"); return false; }
                return true;
            }
            case 'U': {
                if (len != sizeof(OrderReplaceMessage)) { Logger::error("Validation Failed: OrderReplaceMessage length mismatch."); return false; }
                auto* repl_msg = reinterpret_cast<const OrderReplaceMessage*>(data);
                if (be64_to_host(repl_msg->orig_order_number) == 0) { Logger::error("Validation Failed: OrderReplace orig_order_number == 0"); return false; }
                if (be64_to_host(repl_msg->new_order_number) == 0) { Logger::error("Validation Failed: OrderReplace new_order_number == 0"); return false; }
                if (be64_to_host(repl_msg->quantity) == 0) { Logger::error("Validation Failed: OrderReplace quantity == 0"); return false; }
                if (be32_to_host(repl_msg->price) == 0) { Logger::error("Validation Failed: OrderReplace price == 0"); return false; }
                return true;
            }
            case 'D': {
                if (len != sizeof(OrderDeleteMessage)) { Logger::error("Validation Failed: OrderDeleteMessage length mismatch."); return false; }
                auto* del_msg = reinterpret_cast<const OrderDeleteMessage*>(data);
                if (be64_to_host(del_msg->order_number) == 0) { Logger::error("Validation Failed: OrderDelete order_number == 0"); return false; }
                return true;
            }
            case 'Z': {
                if (len != sizeof(IndexValueMessage)) { Logger::error("Validation Failed: IndexValueMessage length mismatch."); return false; }
                auto* idx_msg = reinterpret_cast<const IndexValueMessage*>(data);
                if (be32_to_host(idx_msg->index_orderbook) == 0) { Logger::error("Validation Failed: IndexValue index_orderbook == 0"); return false; }
                return true;
            }
            default:
                Logger::warn("Validation Failed: Unknown ITCH Message Type: " + std::to_string((int)msg_type));
                return false;
        }
    }
};

} // namespace itch
