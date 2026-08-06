#pragma once

#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include "prng.hpp"

namespace itch {

class PriceEngine {
private:
    XorShift128Plus prng_;

public:
    explicit PriceEngine(uint64_t seed = 42424242ULL) : prng_(seed, seed ^ 0xDEADBEEFULL) {}

    // Calculate next order price using Random Walk model with mean reversion around base price
    inline uint32_t calculate_next_price(uint32_t current_price, uint32_t base_price, char side) noexcept {
        // Random walk step (-5 to +5 price steps)
        int32_t step = static_cast<int32_t>(prng_.next_range(0, 10)) - 5;
        
        // Slight mean reversion pull towards base price
        int32_t drift = 0;
        if (current_price > base_price + 200) drift = -2;
        else if (current_price < base_price - 200) drift = 2;

        int32_t new_price_int = static_cast<int32_t>(current_price) + step + drift;
        uint32_t new_price = static_cast<uint32_t>(std::max(100, new_price_int));

        // Add spread offset based on Buy ('B') or Sell ('S')
        if (side == 'B') {
            return (new_price > 10) ? new_price - 5 : new_price;
        } else {
            return new_price + 5;
        }
    }

    // Calculate diurnal volume throughput multiplier based on market progress (0.0 to 1.0)
    // 0.0 (Open): 1.8x volume burst
    // 0.5 (Midday): 0.5x volume dip
    // 1.0 (Close): 2.2x volume surge
    static double calculate_volume_multiplier(double market_progress_ratio) noexcept {
        // U-shaped diurnal volume curve: 1.5 - sin(pi * progress)
        constexpr double PI = 3.14159265358979323846;
        double sin_val = std::sin(market_progress_ratio * PI);
        return 1.8 - (1.2 * sin_val);
    }
};

} // namespace itch
