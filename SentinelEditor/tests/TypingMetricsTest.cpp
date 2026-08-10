#include "TypingMetrics.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <string>

namespace {

int Fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    using namespace std::chrono_literals;
    using Clock = TypingMetrics::Clock;

    TypingMetrics metrics;
    const Clock::time_point base{1h};

    metrics.RecordLine(base);
    metrics.RecordLine(base + 30s);
    metrics.RecordLine(base + 90s);

    for (int index = 0; index < 15; ++index) {
        metrics.RecordCharacter(base + 80s + std::chrono::milliseconds(index));
    }

    auto snapshot = metrics.GetSnapshot(base + 90s);
    if (snapshot.linesLastMinute != 2) {
        return Fail("LPM window should contain two line events");
    }
    if (snapshot.linesLastHour != 3) {
        return Fail("LPH window should contain three line events");
    }
    if (snapshot.charactersLast30Seconds != 15 ||
        std::abs(snapshot.charactersPerMinute30Seconds - 30.0) > 0.001) {
        return Fail("30-second character rate was calculated incorrectly");
    }

    snapshot = metrics.GetSnapshot(base + 121s);
    if (snapshot.linesLastMinute != 1) {
        return Fail("rolling minute window did not expire old line events");
    }
    if (snapshot.charactersLast30Seconds != 0 ||
        snapshot.charactersPerMinute30Seconds != 0.0) {
        return Fail("rolling 30-second character window did not expire old events");
    }

    snapshot = metrics.GetSnapshot(base + 3700s);
    if (snapshot.linesLastHour != 0) {
        return Fail("rolling hour window did not expire old line events");
    }

    metrics.RecordCharacter(base + 3701s);
    metrics.RecordLine(base + 3701s);
    metrics.Reset();
    snapshot = metrics.GetSnapshot(base + 3701s);
    if (snapshot.linesLastMinute != 0 || snapshot.linesLastHour != 0 ||
        snapshot.charactersLast30Seconds != 0) {
        return Fail("Reset did not clear typing metrics");
    }

    return 0;
}
