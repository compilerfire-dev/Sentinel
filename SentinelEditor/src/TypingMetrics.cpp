#include "TypingMetrics.hpp"

#include <algorithm>

void TypingMetrics::RecordCharacter(Clock::time_point when) {
    characterEvents_.push_back(when);
    Prune(when);
}

void TypingMetrics::RecordLine(Clock::time_point when) {
    lineEvents_.push_back(when);
    Prune(when);
}

TypingMetrics::Snapshot TypingMetrics::GetSnapshot(Clock::time_point now) {
    Prune(now);

    Snapshot snapshot;
    snapshot.linesLastHour = lineEvents_.size();

    const auto minuteCutoff = now - std::chrono::minutes(1);
    snapshot.linesLastMinute = static_cast<std::size_t>(std::count_if(
        lineEvents_.begin(),
        lineEvents_.end(),
        [&](const auto& event) { return event >= minuteCutoff; }
    ));

    const auto thirtySecondCutoff = now - std::chrono::seconds(30);
    snapshot.charactersLast30Seconds = static_cast<std::size_t>(std::count_if(
        characterEvents_.begin(),
        characterEvents_.end(),
        [&](const auto& event) { return event >= thirtySecondCutoff; }
    ));

    // Convert a 30-second rolling character count to a per-minute rate.
    snapshot.charactersPerMinute30Seconds =
        static_cast<double>(snapshot.charactersLast30Seconds) * 2.0;

    return snapshot;
}

void TypingMetrics::Reset() {
    characterEvents_.clear();
    lineEvents_.clear();
}

void TypingMetrics::Prune(Clock::time_point now) {
    const auto characterCutoff = now - std::chrono::seconds(30);
    while (!characterEvents_.empty() && characterEvents_.front() < characterCutoff) {
        characterEvents_.pop_front();
    }

    const auto lineCutoff = now - std::chrono::hours(1);
    while (!lineEvents_.empty() && lineEvents_.front() < lineCutoff) {
        lineEvents_.pop_front();
    }
}
