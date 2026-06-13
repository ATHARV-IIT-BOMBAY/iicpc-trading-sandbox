#pragma once
#include <cstdint>

// Enforcing strict uint64_t for IDs and Timestamps to prevent overflow
struct Order {
    uint64_t order_id;
    char symbol[8]; // Fixed char array is faster than std::string for latency
    uint8_t side;   // 0 for BUY, 1 for SELL
    double price;
    uint32_t quantity;
    uint64_t timestamp_ns;
};

struct ExecutionReport {
    uint64_t order_id;
    uint8_t status; // 0 for ACK, 1 for FILLED, 2 for REJECTED
    double fill_price;
    uint64_t latency_ns;
};