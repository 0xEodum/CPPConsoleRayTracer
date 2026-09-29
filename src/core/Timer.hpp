#pragma once

#include <chrono>

namespace crt {

using Clock = std::chrono::steady_clock;

/// Monotonic stopwatch.
class Stopwatch {
public:
    Stopwatch() : start_(Clock::now()) {}

    void restart() { start_ = Clock::now(); }
    double seconds() const { return std::chrono::duration<double>(Clock::now() - start_).count(); }
    double milliseconds() const { return seconds() * 1000.0; }

private:
    Clock::time_point start_;
};

/// Exponential moving average, handy for smoothing FPS / throughput readouts.
class RunningAverage {
public:
    explicit RunningAverage(double smoothing = 0.1) : smoothing_(smoothing) {}

    void add(double value) {
        value_ = initialised_ ? value_ + (value - value_) * smoothing_ : value;
        initialised_ = true;
    }
    double value() const { return value_; }
    void reset() { initialised_ = false; value_ = 0.0; }

private:
    double smoothing_;
    double value_ = 0.0;
    bool initialised_ = false;
};

}  // namespace crt
