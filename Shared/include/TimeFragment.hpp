#pragma once

#include <chrono>
#include <cstdint>

namespace SentinelShared {

struct TimeFragment {
    using Clock = std::chrono::system_clock;

    Clock::time_point startedAt{};
    Clock::time_point endedAt{};
    std::chrono::seconds duration{0};

    bool IsOpen() const noexcept {
        return endedAt.time_since_epoch() == Clock::duration::zero();
    }
};

inline std::int64_t EpochSeconds(TimeFragment::Clock::time_point value) {
    return std::chrono::duration_cast<std::chrono::seconds>(
        value.time_since_epoch()
    ).count();
}

inline TimeFragment::Clock::time_point TimePointFromEpoch(std::int64_t epoch) {
    return TimeFragment::Clock::time_point(std::chrono::seconds(epoch));
}

} // namespace SentinelShared
