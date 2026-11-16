#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <type_traits>
#include <utility>

namespace bench {
using u8 = std::uint8_t;
using u64 = std::uint64_t;
constexpr int ways = 16;

inline u64 mix(u64 x) {
    x ^= x >> 30;
    x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27;
    x *= 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}

// A permutation of occupied slot IDs, with policy-specific rank semantics.
struct Order {
    u8 id[ways]{};
    void move(int from, int to) {
        if (unsigned(from) >= ways || unsigned(to) >= ways) std::abort();
        if (from == to) return;
        u8 slot = id[from];
        if (from < to) std::memmove(id + from, id + from + 1, to - from);
        else std::memmove(id + to + 1, id + to, from - to);
        id[to] = slot;
    }
    int rank(int slot) const {
        return int(std::find(id, id + ways, slot) - id);
    }
};

struct PCR : Order {
    // Oldest first. Low two bits are credit, bit 2 is seen.
    u8 state[ways]{}, phase{}, recent[3]{255,255,255};
    void hit(int slot) {
        auto &v = state[slot];
        if (v < 4) v = 4 + std::min(3, v + 1);
    }
    int admit(int n) {
        int slot = n;
        if (n == ways) {
            int best = 0, low = 6;
            for (int r = 0; r < ways; ++r) {
                int score = (state[id[r]] & 3) + 2 * (id[r] == recent[0] || id[r] == recent[1] || id[r] == recent[2]);
                if (score < low) { low = score; best = r; }
                if (!score) break;
            }
            slot = id[best];
            move(best, ways - 1);
        } else id[n] = slot;
        for (auto &old : recent) if (old == slot) old = 255;
        recent[2] = recent[1]; recent[1] = recent[0]; recent[0] = slot;
        state[slot] = 4;
        if (++phase == ways) {
            phase = 0;
            for (auto &v : state)
                v = v >= 4 ? v - 4 : std::max(0, v - 1);
        }
        return slot;
    }
};

struct PCR2 {
    // Reuse evidence expires after the current and previous pressure epochs.
    std::uint16_t current{}, previous{};
    u8 phase{}, trial{};
    void hit(int slot) { current |= 1u << slot; }
    int admit(int n) {
        if (n < ways) return n;
        const unsigned reused = current | previous;
        if ((phase & 7) == 7 || (reused & (1u << trial))) {
            unsigned cold = (~reused) & 0xffff;
            unsigned after = cold & (0xffffu << (trial + 1));
            // Wrap to the first cold slot. If all are hot, advance one slot.
            unsigned candidates = after ? after : cold;
            trial = candidates ? std::countr_zero(candidates) : (trial + 1) % ways;
        }
        const unsigned keep = ~(1u << trial);
        previous &= keep;
        current &= keep;
        if (++phase == ways) {
            previous = current;
            current = 0;
            phase = 0;
        }
        return trial;
    }
};

struct LRU : Order {
    // MRU first.
    void hit(int slot) { move(rank(slot), 0); }
    int admit(int n) {
        int last = std::min(n, ways - 1);
        if (n < ways) id[last] = n;
        int slot = id[last];
        move(last, 0);
        return slot;
    }
};

struct LFU : LRU {
    u64 count[ways]{};
    void hit(int slot) { ++count[slot]; LRU::hit(slot); }
    int admit(int n) {
        int slot = n;
        if (n < ways) { id[n] = n; move(n, 0); }
        else {
            int victim = ways - 1;
            for (int r = ways - 2; r >= 0; --r)
                if (count[id[r]] < count[id[victim]]) victim = r;
            slot = id[victim];
            move(victim, 0);
        }
        count[slot] = 1;
        return slot;
    }
};

struct AdaptiveClimb : Order {
    int jump = ways;
    void hit(int slot) {
        jump = std::max(1, jump - 1);
        int r = rank(slot);
        move(r, std::max(0, r - jump));
    }
    int admit(int n) {
        jump = std::min(ways, jump + 1);
        int last = std::min(n, ways - 1);
        if (n < ways) id[last] = n;
        int slot = id[last];
        move(last, std::min(last, ways - jump));
        return slot;
    }
};

struct SIEVE : Order {
    // Oldest first. After removal, its successor slides into its rank.
    std::uint16_t visited{};
    u8 hand{};
    void hit(int slot) { visited |= 1u << slot; }
    int admit(int n) {
        if (n < ways) { id[n] = n; return n; }
        while (visited & (1u << id[hand])) {
            visited &= ~(1u << id[hand]);
            hand = (hand + 1) % ways;
        }
        int slot = id[hand];
        int r = hand;
        hand = r == ways - 1 ? 0 : r;
        move(r, ways - 1);
        return slot;
    }
};

struct WTinyLFU : Order {
    // Window slot 0. Main order: protected MRU..LRU, probation MRU..LRU.
    u8 prot{}, events{};
    u8 sketch[4][8]{};
    int frequency(u64 h) const {
        int f = 15;
        for (int row = 0; row < 4; ++row, h >>= 16) {
            int col = h & 15;
            f = std::min(f, (sketch[row][col/2] >> (4*(col%2))) & 15);
        }
        return f;
    }
    void record(u64 h) {
        for (int row = 0; row < 4; ++row, h >>= 16) {
            int col = h & 15, shift = 4*(col%2);
            auto &b = sketch[row][col/2];
            if (((b >> shift) & 15) < 15) b += 1u << shift;
        }
        if (++events == 160) {
            for (auto &row : sketch)
                for (auto &b : row) b = (b >> 1) & 0x77;
            events = 80; // halving retains half a sample
        }
    }
    void hit(int slot) {
        if (!slot) return;
        int r = rank(slot);
        if (r >= prot && prot < 12) ++prot;
        move(r, 0);
    }
    // Old window entry competes with probation LRU; new request enters window.
    template<class Entries>
    int admit(int n, Entries &entries) {
        if (!n) return 0;
        int r = std::min(n - 1, ways - 2);
        if (n < ways) id[r] = n;
        int dest = id[r];
        if (n < ways || frequency(mix(entries[0].first)) >
                        frequency(mix(entries[dest].first))) {
            entries[dest] = entries[0];
            move(r, prot);
        }
        return 0;
    }
};

struct Result { bool hit; u64 value; };
template<class Policy>
struct Set {
    std::pair<u64,u64> entries[ways]{};
    Policy policy;
    u8 size{};
    Result access(u64 key, u64 hash) {
        if constexpr (std::is_same_v<Policy,WTinyLFU>) policy.record(hash);
        for (int i = 0; i < size; ++i) {
            if (entries[i].first == key) {
                policy.hit(i);
                return {true, entries[i].second};
            }
        }
        int slot;
        if constexpr (std::is_same_v<Policy,WTinyLFU>) slot = policy.admit(size, entries);
        else slot = policy.admit(size);
        if (size < ways) ++size;
        entries[slot] = {key, key ^ 0x12345678ULL};
        return {false, entries[slot].second};
    }
};
} // namespace bench


