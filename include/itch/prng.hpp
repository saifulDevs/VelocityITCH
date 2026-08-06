#pragma once

#include <cstdint>
#include <chrono>

namespace itch {

// High-Performance XorShift128+ Pseudo-Random Number Generator (PRNG)
// Optimized for HFT load testing with minimal state (16 bytes) and zero allocation overhead.
class XorShift128Plus {
private:
    uint64_t state_[2];

public:
    explicit XorShift128Plus(uint64_t seed1 = 123456789ULL, uint64_t seed2 = 987654321ULL) {
        if (seed1 == 0 && seed2 == 0) {
            seed1 = std::chrono::steady_clock::now().time_since_epoch().count();
            seed2 = seed1 ^ 0x9E3779B97F4A7C15ULL;
        }
        state_[0] = seed1;
        state_[1] = seed2;
    }

    // Generate 64-bit random integer
    inline uint64_t next_u64() noexcept {
        uint64_t s1 = state_[0];
        const uint64_t s0 = state_[1];
        const uint64_t result = s0 + s1;
        state_[0] = s0;
        s1 ^= s1 << 23;
        state_[1] = s1 ^ s0 ^ (s1 >> 17) ^ (s0 >> 26);
        return result;
    }

    // Generate 32-bit random integer
    inline uint32_t next_u32() noexcept {
        return static_cast<uint32_t>(next_u64());
    }

    // Uniform integer in range [min_val, max_val] inclusive
    inline uint64_t next_range(uint64_t min_val, uint64_t max_val) noexcept {
        if (min_val >= max_val) return min_val;
        uint64_t range = max_val - min_val + 1;
        return min_val + (next_u64() % range);
    }

    // Uniform double in range [0.0, 1.0)
    inline double next_double() noexcept {
        return (next_u64() >> 11) * 0x1.0p-53;
    }
};

} // namespace itch
