#ifndef PCG_H
#define PCG_H

#include <bit>      // std::rotr
#include <cstdint>  // uint32_t, uint64_t
#include <limits>   // std::numeric_limits
#include <random>   // std::uniform_random_bit_generator (for static assertion)

// 32-bit random number generator with excellent statistical properties:
class PCG {
public:
    // Implementing the requirements for the UniformRandomBitGenerator concept:
    // https://en.cppreference.com/w/cpp/named_req/UniformRandomBitGenerator.html
    using result_type = uint32_t;
    constexpr static result_type min() { return 0; }
    constexpr static result_type max() { return std::numeric_limits<uint32_t>::max(); }

    explicit PCG(const uint64_t seed = 0) : mState{0x4d595df4d0f33173ull + seed + increment} {
        this->operator()();
    }

    constexpr result_type operator()() {
        uint64_t x = mState;
        const auto count = static_cast<int>(x >> 59);  // 59 = 64 - 5

        mState = x * multiplier + increment;
        x ^= x >> 18;                                  // 18 = (64 - 27)/2
        return std::rotr(static_cast<uint32_t>(x >> 27), count);  // 27 = 32 - 5
    }

    float rand01() { return static_cast<float>((*this)()) / static_cast<float>(max()); }

    void reset(const uint64_t seed = 0) {
        mState = 0x4d595df4d0f33173ull + seed + increment;
        this->operator()();
    }

private:
    static constexpr uint64_t multiplier = 6364136223846793005ull;
    static constexpr uint64_t increment = 1442695040888963407ull;
    uint64_t mState;
};

static_assert(std::uniform_random_bit_generator<PCG>);

#endif  // PCG_H
