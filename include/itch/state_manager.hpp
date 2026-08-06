#pragma once

#include <vector>
#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include "prng.hpp"

namespace itch {

// Open-Addressing Flat Hash Map for HFT cache-coherency (Zero dynamic node allocation)
class FlatIndexMap {
private:
    struct Slot {
        uint64_t key{0};
        uint32_t value{0};
        bool occupied{false};
    };

    std::vector<Slot> slots_;
    size_t size_{0};
    size_t mask_{0};

    inline static uint64_t hash_u64(uint64_t x) noexcept {
        x ^= x >> 30;
        x *= 0xbf58476d1ce4e5b9ULL;
        x ^= x >> 27;
        x *= 0x94d049bb133111ebULL;
        x ^= x >> 31;
        return x;
    }

    void rehash(size_t new_cap) {
        std::vector<Slot> old_slots = std::move(slots_);
        slots_.assign(new_cap, Slot{});
        mask_ = new_cap - 1;
        size_ = 0;

        for (const auto& slot : old_slots) {
            if (slot.occupied) {
                insert(slot.key, slot.value);
            }
        }
    }

public:
    explicit FlatIndexMap(size_t initial_cap = 1048576) {
        size_t cap = 1;
        while (cap < initial_cap) cap <<= 1;
        slots_.resize(cap);
        mask_ = cap - 1;
    }

    inline bool find(uint64_t key, uint32_t& out_val) const noexcept {
        size_t idx = hash_u64(key) & mask_;
        while (slots_[idx].occupied) {
            if (slots_[idx].key == key) {
                out_val = slots_[idx].value;
                return true;
            }
            idx = (idx + 1) & mask_;
        }
        return false;
    }

    inline void insert(uint64_t key, uint32_t val) {
        if ((size_ + 1) * 2 > slots_.size()) {
            rehash(slots_.size() * 2);
        }

        size_t idx = hash_u64(key) & mask_;
        while (slots_[idx].occupied) {
            if (slots_[idx].key == key) {
                slots_[idx].value = val;
                return;
            }
            idx = (idx + 1) & mask_;
        }

        slots_[idx].key = key;
        slots_[idx].value = val;
        slots_[idx].occupied = true;
        size_++;
    }

    inline bool erase(uint64_t key) noexcept {
        size_t idx = hash_u64(key) & mask_;
        while (slots_[idx].occupied) {
            if (slots_[idx].key == key) {
                slots_[idx].occupied = false;
                size_--;
                
                // Rehash cluster to maintain linear probing continuity
                size_t next_idx = (idx + 1) & mask_;
                while (slots_[next_idx].occupied) {
                    Slot item = slots_[next_idx];
                    slots_[next_idx].occupied = false;
                    size_--;
                    insert(item.key, item.value);
                    next_idx = (next_idx + 1) & mask_;
                }
                return true;
            }
            idx = (idx + 1) & mask_;
        }
        return false;
    }

    inline size_t size() const noexcept { return size_; }
};

// HFT State Manager: O(1) Contiguous Active Order Tracking
// Uses Swap-and-Pop pattern with FlatIndexMap for O(1) random sampling, insertion, and deletion.
class DenseOrderTracker {
private:
    std::vector<uint64_t> active_orders_;
    FlatIndexMap index_map_;

public:
    explicit DenseOrderTracker(size_t initial_capacity = 1000000)
        : index_map_(initial_capacity) {
        active_orders_.reserve(initial_capacity);
    }

    // Insert new order: O(1) append to contiguous array + flat index map
    inline void insert(uint64_t order_number) {
        uint32_t vec_index = static_cast<uint32_t>(active_orders_.size());
        active_orders_.push_back(order_number);
        index_map_.insert(order_number, vec_index);
    }

    // Remove order: O(1) Swap-and-Pop pattern
    inline bool remove(uint64_t order_number) {
        uint32_t idx_to_remove = 0;
        if (!index_map_.find(order_number, idx_to_remove)) {
            return false;
        }

        uint64_t last_order = active_orders_.back();
        uint32_t last_idx = static_cast<uint32_t>(active_orders_.size() - 1);

        if (idx_to_remove != last_idx) {
            // Swap last element into the position of removed element
            active_orders_[idx_to_remove] = last_order;
            index_map_.insert(last_order, idx_to_remove);
        }

        // Pop last element
        active_orders_.pop_back();
        index_map_.erase(order_number);
        return true;
    }

    // Replace order: O(1) swap old order with new order
    inline bool replace(uint64_t orig_order_number, uint64_t new_order_number) {
        if (!remove(orig_order_number)) return false;
        insert(new_order_number);
        return true;
    }

    // Random selection: O(1) constant time picking from contiguous vector
    inline bool get_random_order(XorShift128Plus& prng, uint64_t& out_order_number) const noexcept {
        if (active_orders_.empty()) return false;
        uint64_t rand_idx = prng.next_range(0, active_orders_.size() - 1);
        out_order_number = active_orders_[rand_idx];
        return true;
    }

    inline size_t size() const noexcept { return active_orders_.size(); }
    inline bool empty() const noexcept { return active_orders_.empty(); }

    void reserve(size_t capacity) {
        active_orders_.reserve(capacity);
    }
};

} // namespace itch
