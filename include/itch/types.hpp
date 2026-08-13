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

// Orderbook Trading Action Message [H] - 11 bytes
struct OrderbookTradingActionMessage {
    char     message_type;      // 'H'
    uint32_t timestamp;         // 4
    uint32_t orderbook_id;      // 4
    char     trading_state;     // 1
    char     reason;            // 1
} __attribute__((packed));


// Orderbook Directory Message [R] - 171 bytes
struct OrderbookDirectoryMessage {
    char     message_type;      // 'R'
    uint32_t timestamp;         // 4
    uint32_t orderbook_id;      // 4
    char     price_type;        // 1
    char     isin[12];          // 12
    char     sec_code[12];      // 12
    char     currency[3];       // 3
    char     group[8];          // 8
    uint64_t minimum_quantity;  // 8
    uint32_t quantity_tick_size_table_id; // 4
    uint32_t price_tick_size_table_id;    // 4
    uint32_t price_decimals;    // 4
    uint32_t delisting_date;    // 4
    uint32_t delisting_time;    // 4
    char     market_type;       // 1
    uint32_t company;           // 4
    char     listing_type;      // 1
    char     sector[12];        // 12
    char     instr[12];         // 12
    char     security_name[60]; // 60
    uint32_t maturity_date;     // 4
    uint32_t yield_decimals;    // 4
} __attribute__((packed));

// Company Directory Message [P] - 69 bytes
struct CompanyDirectoryMessage {
    char     message_type;      // 'P'
    uint32_t timestamp;         // 4
    uint32_t company;           // 4
    char     company_name[30];  // 30
    char     company_id[30];    // 30
} __attribute__((packed));

// Add Order Message [A] - 34 bytes
struct AddOrderMessage {
    char     message_type;      // 'A'
    uint32_t timestamp;         // 4
    uint64_t order_number;      // 8
    char     side;              // 1
    uint64_t quantity;          // 8
    uint32_t orderbook_id;      // 4
    uint32_t price;             // 4
    uint32_t yield;             // 4
} __attribute__((packed));

// Order Executed Message [E] - 29 bytes
struct OrderExecutedMessage {
    char     message_type;      // 'E'
    uint32_t timestamp;         // 4
    uint64_t order_number;      // 8
    uint64_t executed_quantity; // 8
    uint64_t match_id;          // 8
} __attribute__((packed));

// Order Delete Message [D] - 13 bytes
struct OrderDeleteMessage {
    char     message_type;      // 'D'
    uint32_t timestamp;         // 4
    uint64_t order_number;      // 8
} __attribute__((packed));

// Order Replace Message [U] - 37 bytes
struct OrderReplaceMessage {
    char     message_type;      // 'U'
    uint32_t timestamp;         // 4
    uint64_t orig_order_number; // 8
    uint64_t new_order_number;  // 8
    uint64_t quantity;          // 8
    uint32_t price;             // 4
    uint32_t yield;             // 4
} __attribute__((packed));

// Index Data Specific Structs
struct IndexValueMessage { // 'Z' - 17 bytes
    char     message_type;      // 1
    uint32_t timestamp;         // 4
    uint32_t index_orderbook;   // 4
    uint64_t value;             // 8
} __attribute__((packed));

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
static_assert(sizeof(OrderbookDirectoryMessage) == 171, "OrderbookDirectoryMessage size mismatch");
static_assert(sizeof(CompanyDirectoryMessage) == 69, "CompanyDirectoryMessage size mismatch");
static_assert(sizeof(OrderbookTradingActionMessage) == 11, "OrderbookTradingActionMessage size mismatch");
static_assert(sizeof(AddOrderMessage) == 34, "AddOrderMessage size mismatch");
static_assert(sizeof(OrderExecutedMessage) == 29, "OrderExecutedMessage size mismatch");
static_assert(sizeof(OrderReplaceMessage) == 37, "OrderReplaceMessage size mismatch");
static_assert(sizeof(OrderDeleteMessage) == 13, "OrderDeleteMessage size mismatch");
static_assert(sizeof(IndexValueMessage) == 17, "IndexValueMessage size mismatch");
static_assert(sizeof(MoldUDP64Header) == 20, "MoldUDP64Header size mismatch");
static_assert(sizeof(MoldUDP64BlockHeader) == 2, "MoldUDP64BlockHeader size mismatch");

} // namespace itch
