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
                return (len == sizeof(SystemEventMessage));
            case 'R':
                return (len == sizeof(OrderbookDirectoryMessage));
            case 'A': {
                if (len != sizeof(AddOrderMessage)) return false;
                auto* add_msg = reinterpret_cast<const AddOrderMessage*>(data);
                if (be64_to_host(add_msg->order_number) == 0) return false;
                if (be32_to_host(add_msg->quantity) == 0) return false;
                if (be32_to_host(add_msg->price) == 0) return false;
                return (add_msg->side == 'B' || add_msg->side == 'S');
            }
            case 'E': {
                if (len != sizeof(OrderExecutedMessage)) return false;
                auto* exec_msg = reinterpret_cast<const OrderExecutedMessage*>(data);
                if (be64_to_host(exec_msg->order_number) == 0) return false;
                if (be32_to_host(exec_msg->executed_quantity) == 0) return false;
                return true;
            }
            case 'U': {
                if (len != sizeof(OrderReplaceMessage)) return false;
                auto* repl_msg = reinterpret_cast<const OrderReplaceMessage*>(data);
                if (be64_to_host(repl_msg->orig_order_number) == 0) return false;
                if (be64_to_host(repl_msg->new_order_number) == 0) return false;
                if (be32_to_host(repl_msg->quantity) == 0) return false;
                if (be32_to_host(repl_msg->price) == 0) return false;
                return true;
            }
            case 'D': {
                if (len != sizeof(OrderDeleteMessage)) return false;
                auto* del_msg = reinterpret_cast<const OrderDeleteMessage*>(data);
                return (be64_to_host(del_msg->order_number) > 0);
            }
            default:
                Logger::warn("Validation Failed: Unknown ITCH Message Type");
                return false;
        }
    }
};

} // namespace itch
