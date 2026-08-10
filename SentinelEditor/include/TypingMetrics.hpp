#pragma once

#include <chrono>
#include <cstddef>
#include <deque>

class TypingMetrics {
public:
    using Clock = std::chrono::steady_clock;

    struct Snapshot {
        std::size_t linesLastMinute{0};
        std::size_t linesLastHour{0};
        std::size_t charactersLast30Seconds{0};
        double charactersPerMinute30Seconds{0.0};
    };

    void RecordCharacter(Clock::time_point when = Clock::now());
    void RecordLine(Clock::time_point when = Clock::now());
    Snapshot GetSnapshot(Clock::time_point now = Clock::now());
    void Reset();

private:
    void Prune(Clock::time_point now);

    std::deque<Clock::time_point> characterEvents_;
    std::deque<Clock::time_point> lineEvents_;
};
