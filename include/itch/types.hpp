#pragma once

#include <cstdint>
#include <cstring>
#include <string_view>
#include <algorithm>

namespace itch {

// Big-Endian Byte-Swapping Utilities
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
inline uint16_t host_to_be16(uint16_t v) { return v; }
inline uint32_t host_to_be32(uint32_t v) { return v; }
inline uint64_t host_to_be64(uint64_t v) { return v; }
inline uint16_t be16_to_host(uint16_t v) { return v; }
inline uint32_t be32_to_host(uint32_t v) { return v; }
inline uint64_t be64_to_host(uint64_t v) { return v; }
#else
inline uint16_t host_to_be16(uint16_t v) { return __builtin_bswap16(v); }
inline uint32_t host_to_be32(uint32_t v) { return __builtin_bswap32(v); }
inline uint64_t host_to_be64(uint64_t v) { return __builtin_bswap64(v); }
inline uint16_t be16_to_host(uint16_t v) { return __builtin_bswap16(v); }
inline uint32_t be32_to_host(uint32_t v) { return __builtin_bswap32(v); }
inline uint64_t be64_to_host(uint64_t v) { return __builtin_bswap64(v); }
#endif

// Alpha field helper: Left-justified, right-padded with ASCII spaces
inline void set_alpha(char* dest, std::string_view src, size_t max_len) {
    size_t copy_len = std::min(src.size(), max_len);
    std::memcpy(dest, src.data(), copy_len);
    if (copy_len < max_len) {
        std::memset(dest + copy_len, ' ', max_len - copy_len);
    }
}

// ITCH Message Types
enum class MessageType : char {
    SystemEvent        = 'S',
    OrderbookDirectory = 'R',
    AddOrder           = 'A',
    OrderExecuted      = 'E',
    OrderReplace       = 'U',
    OrderDelete        = 'D'
};

// System Event Codes
enum class SystemEventCode : char {
    StartOfMessages   = 'O',
    StartOfSystemHours = 'S',
    EndOfSystemHours   = 'E',
    EndOfMessages     = 'C'
};

#pragma pack(push, 1)

// System Event Message [S] - 18 bytes
struct SystemEventMessage {
    char     message_type;      // 'S'
    uint32_t timestamp;         // Nanoseconds/Seconds since midnight
    char     group[8];          // Market group (e.g., "MAIN    ")
    char     event_code;        // System Event Code ('O', 'S', 'E', 'C')
    uint32_t orderbook_id;      // Orderbook ID (0 for system-wide)
};

// Orderbook Directory Message [R] - 97 bytes
struct OrderbookDirectoryMessage {
    char     message_type;      // 'R'
    uint32_t timestamp;         // Timestamp
    uint32_t orderbook_id;      // Unique Orderbook ID
    char     symbol[32];        // Ticker Symbol (e.g., "GP                              ")
    char     long_name[32];     // Full Company Name
    char     isin[12];          // ISIN Code
    char     financial_status;  // Financial Status ('A' = Active)
    char     currency[3];       // Currency ("BDT")
    uint16_t price_decimals;    // Price Decimal Places (e.g., 2)
    uint16_t nominal_decimals;  // Nominal Decimal Places
    uint32_t lot_size;          // Trading Lot Size (e.g., 1 or 100)
};

// Add Order Message [A] - 30 bytes
struct AddOrderMessage {
    char     message_type;      // 'A'
    uint32_t timestamp;         // Timestamp
    uint64_t order_number;      // Unique Order Reference Number
    uint32_t orderbook_id;      // Orderbook ID
    char     side;              // 'B' = Buy, 'S' = Sell
    uint32_t quantity;          // Shares quantity
    uint32_t price;             // Price (in price decimals format, e.g. 150.50 -> 15050)
    char     participant_id[4]; // Broker/Participant ID
};

// Order Executed Message [E] - 25 bytes
struct OrderExecutedMessage {
    char     message_type;      // 'E'
    uint32_t timestamp;         // Timestamp
    uint64_t order_number;      // Existing Order Reference Number
    uint32_t executed_quantity; // Executed quantity
    uint64_t match_id;          // Unique Match/Trade Execution ID
};

// Order Replace Message [U] - 29 bytes
struct OrderReplaceMessage {
    char     message_type;      // 'U'
    uint32_t timestamp;         // Timestamp
    uint64_t orig_order_number; // Reference to original Order Number
    uint64_t new_order_number;  // New assigned Order Number
    uint32_t quantity;          // Updated quantity
    uint32_t price;             // Updated price
};

// Order Delete Message [D] - 13 bytes
struct OrderDeleteMessage {
    char     message_type;      // 'D'
    uint32_t timestamp;         // Timestamp
    uint64_t order_number;      // Reference to Order Number to cancel/delete
};

// MoldUDP64 Packet Header - 20 bytes
struct MoldUDP64Header {
    char     session[10];       // Session Identifier (left-justified, space-padded)
    uint64_t sequence_num;      // Sequence number of 1st message in packet
    uint16_t message_count;     // Number of messages contained in payload
};

// MoldUDP64 Block Header (prepended to each ITCH message in MoldUDP64 mode) - 2 bytes
struct MoldUDP64BlockHeader {
    uint16_t message_length;    // Byte length of the following ITCH message
};

#pragma pack(pop)

// Compile-time assertion of byte sizes to strictly validate DSE ITCH protocol layout
static_assert(sizeof(SystemEventMessage) == 18, "SystemEventMessage size mismatch");
static_assert(sizeof(OrderbookDirectoryMessage) == 97, "OrderbookDirectoryMessage size mismatch");
static_assert(sizeof(AddOrderMessage) == 30, "AddOrderMessage size mismatch");
static_assert(sizeof(OrderExecutedMessage) == 25, "OrderExecutedMessage size mismatch");
static_assert(sizeof(OrderReplaceMessage) == 29, "OrderReplaceMessage size mismatch");
static_assert(sizeof(OrderDeleteMessage) == 13, "OrderDeleteMessage size mismatch");
static_assert(sizeof(MoldUDP64Header) == 20, "MoldUDP64Header size mismatch");
static_assert(sizeof(MoldUDP64BlockHeader) == 2, "MoldUDP64BlockHeader size mismatch");

} // namespace itch
