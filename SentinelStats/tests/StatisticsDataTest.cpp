#include "JsonDataStore.hpp"
#include "StatisticsData.hpp"

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unistd.h>

namespace {

std::int64_t LocalEpoch(
    int year,
    int month,
    int day,
    int hour,
    int minute,
    int second = 0
) {
    std::tm local{};
    local.tm_year = year - 1900;
    local.tm_mon = month - 1;
    local.tm_mday = day;
    local.tm_hour = hour;
    local.tm_min = minute;
    local.tm_sec = second;
    local.tm_isdst = -1;
    return static_cast<std::int64_t>(std::mktime(&local));
}

int Fail(const std::string& message) {
    std::cerr << message << '\n';
    return 1;
}

} // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("sentinel-stats-test-" +
         std::to_string(static_cast<long long>(::getpid())) + ".json");

    const std::int64_t firstStart = LocalEpoch(2026, 8, 3, 23, 30);
    const std::int64_t firstEnd = LocalEpoch(2026, 8, 4, 1, 30);
    const std::int64_t secondStart = LocalEpoch(2026, 8, 4, 10, 0);
    const std::int64_t secondEnd = LocalEpoch(2026, 8, 4, 11, 0);

    if (firstStart <= 0 || firstEnd <= firstStart ||
        secondStart <= firstEnd || secondEnd <= secondStart) {
        return Fail("could not construct local test timestamps");
    }

    nlohmann::json root = {
        {"sharedTasks", {
            {"version", 1},
            {"tasks", nlohmann::json::array({
                {
                    {"id", "night-work"},
                    {"name", "Night Work"},
                    {"created_at_epoch", LocalEpoch(2026, 8, 3, 12, 0)},
                    {"elapsed_seconds", 10800},
                    {"running", false},
                    {"completed", true},
                    {"completed_at_epoch", LocalEpoch(2026, 8, 4, 11, 0)},
                    {"time_fragments", nlohmann::json::array({
                        {
                            {"started_at_epoch", firstStart},
                            {"ended_at_epoch", firstEnd},
                            {"duration_seconds", firstEnd - firstStart}
                        },
                        {
                            {"started_at_epoch", secondStart},
                            {"ended_at_epoch", secondEnd},
                            {"duration_seconds", secondEnd - secondStart}
                        }
                    })}
                }
            })}
        }}
    };

    {
        std::ofstream output(path);
        if (!output) return Fail("could not create temporary statistics JSON");
        output << root.dump(2) << '\n';
    }

    StatisticsData data;
    std::string error;
    if (!data.Load(path, error)) {
        std::filesystem::remove(path);
        std::filesystem::remove(SentinelShared::JsonDataStore::LockPath(path));
        return Fail(error);
    }

    const auto& snapshot = data.Snapshot();
    if (snapshot.totalTasks != 1 || snapshot.completedTasks != 1) {
        return Fail("task totals were not parsed correctly");
    }
    if (snapshot.totalFragments != 2 || snapshot.totalTrackedSeconds != 10800) {
        return Fail("fragment/tracked-time totals were not parsed correctly");
    }
    if (snapshot.taskAnalytics.size() != 1 ||
        snapshot.taskAnalytics.front().trackedSeconds != 10800 ||
        snapshot.taskAnalytics.front().fragmentCount != 2) {
        return Fail("per-task analytics were not derived correctly");
    }

    if (snapshot.dailyWork.size() != 2) {
        return Fail("midnight-spanning work was not split across two days");
    }
    if (snapshot.dailyWork[0].trackedSeconds != 1800) {
        return Fail("first day should contain 30 minutes of the crossing fragment");
    }
    if (snapshot.dailyWork[1].trackedSeconds != 9000) {
        return Fail("second day should contain 2.5 hours of tracked work");
    }

    if (snapshot.weeklyWork.size() != 1 ||
        snapshot.weeklyWork.front().trackedSeconds != 10800) {
        return Fail("Monday-based weekly aggregation was not calculated correctly");
    }

    std::filesystem::remove(path);
    std::filesystem::remove(SentinelShared::JsonDataStore::LockPath(path));
    return 0;
}
