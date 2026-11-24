#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <cstdint>  // uint64_t

class Timer {
public:
    using Nanos = std::chrono::nanoseconds;
    using Micros = std::chrono::microseconds;
    using Millis = std::chrono::milliseconds;
    using Seconds = std::chrono::seconds;
    using Minutes = std::chrono::minutes;
    using Hours = std::chrono::hours;

    Timer() : mLastTime{std::chrono::steady_clock::now()} {}

    void restart() { mLastTime = std::chrono::steady_clock::now(); }

    template <typename T>
    uint64_t glance() const {
        auto newTime = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<T>(newTime - mLastTime).count();
    }

    template <typename T>
    uint64_t click() {
        auto newTime = std::chrono::steady_clock::now();
        uint64_t ret = std::chrono::duration_cast<T>(newTime - mLastTime).count();
        mLastTime = newTime;
        return ret;
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> mLastTime;
};

#endif  // TIMER_H
