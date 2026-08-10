#include "StatisticsData.hpp"

#include "JsonDataStore.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <ctime>
#include <map>

using nlohmann::json;

namespace {

std::int64_t ReadEpoch(const json& value) {
    if (value.is_number_integer()) return value.get<std::int64_t>();
    return 0;
}

std::uint64_t ReadNonNegativeInteger(const json& value) {
    if (value.is_number_unsigned()) return value.get<std::uint64_t>();
    if (value.is_number_integer()) {
        const auto number = value.get<std::int64_t>();
        return number > 0 ? static_cast<std::uint64_t>(number) : 0;
    }
    return 0;
}

std::uint64_t ReadElapsedSeconds(const json& item) {
    if (!item.contains("elapsed_seconds")) return 0;
    return ReadNonNegativeInteger(item["elapsed_seconds"]);
}

std::int64_t CompletionEpoch(const json& item) {
    if (item.contains("completed_at_epoch")) return ReadEpoch(item["completed_at_epoch"]);
    if (item.contains("completion_epoch")) return ReadEpoch(item["completion_epoch"]);
    return 0;
}

std::size_t ReadTaskFragments(
    const json& item,
    const std::string& source,
    StatisticsSnapshot& snapshot
) {
    if (!item.contains("time_fragments") || !item["time_fragments"].is_array()) return 0;

    TaskFragmentSeries series;
    series.id = item.value("id", std::string{});
    series.name = item.value("name", series.id);
    series.source = source;
    if (item.contains("created_at_epoch")) {
        series.createdAtEpoch = ReadEpoch(item["created_at_epoch"]);
    }

    for (const auto& fragmentJson : item["time_fragments"]) {
        if (!fragmentJson.is_object()) continue;

        const auto started = fragmentJson.contains("started_at_epoch")
            ? ReadEpoch(fragmentJson["started_at_epoch"])
            : 0;
        if (started <= 0) continue;

        const auto storedEnd = fragmentJson.contains("ended_at_epoch")
            ? ReadEpoch(fragmentJson["ended_at_epoch"])
            : 0;
        std::uint64_t duration = fragmentJson.contains("duration_seconds")
            ? ReadNonNegativeInteger(fragmentJson["duration_seconds"])
            : 0;

        if (duration == 0 && storedEnd > started) {
            duration = static_cast<std::uint64_t>(storedEnd - started);
        }

        TaskFragment fragment;
        fragment.startedAtEpoch = started;
        fragment.durationSeconds = duration;
        fragment.open = storedEnd <= 0;
        fragment.endedAtEpoch = storedEnd > 0
            ? storedEnd
            : started + static_cast<std::int64_t>(duration);
        if (fragment.endedAtEpoch < fragment.startedAtEpoch) {
            fragment.endedAtEpoch = fragment.startedAtEpoch;
        }

        series.fragments.push_back(fragment);
        ++snapshot.totalFragments;
    }

    std::sort(series.fragments.begin(), series.fragments.end(), [](const auto& left, const auto& right) {
        return left.startedAtEpoch < right.startedAtEpoch;
    });

    const std::size_t count = series.fragments.size();
    if (!series.fragments.empty()) snapshot.taskFragments.push_back(std::move(series));
    return count;
}

void AccumulateTaskObject(
    const json& task,
    const std::string& source,
    StatisticsSnapshot& snapshot,
    std::vector<std::int64_t>& creationTimes,
    std::vector<std::int64_t>& completionTimes
) {
    if (!task.is_object()) return;

    ++snapshot.totalTasks;

    const bool completed = task.value("completed", false);
    const bool running = task.value("running", false);
    const auto tracked = ReadElapsedSeconds(task);
    if (completed) ++snapshot.completedTasks;
    if (running) ++snapshot.runningTasks;
    snapshot.totalTrackedSeconds += tracked;

    std::int64_t createdEpoch = 0;
    if (task.contains("created_at_epoch")) {
        createdEpoch = ReadEpoch(task["created_at_epoch"]);
        if (createdEpoch > 0) creationTimes.push_back(createdEpoch);
    }

    const std::int64_t completedEpoch = completed ? CompletionEpoch(task) : 0;
    if (completedEpoch > 0) completionTimes.push_back(completedEpoch);

    const std::size_t fragmentCount = ReadTaskFragments(task, source, snapshot);

    TaskAnalytics analytics;
    analytics.id = task.value("id", std::string{});
    analytics.name = task.value("name", analytics.id);
    analytics.createdAtEpoch = createdEpoch;
    analytics.completedAtEpoch = completedEpoch;
    analytics.trackedSeconds = tracked;
    analytics.fragmentCount = fragmentCount;
    analytics.running = running;
    analytics.completed = completed;
    snapshot.taskAnalytics.push_back(std::move(analytics));
}

void AccumulateTaskArray(
    const json& tasks,
    const std::string& source,
    StatisticsSnapshot& snapshot,
    std::vector<std::int64_t>& creationTimes,
    std::vector<std::int64_t>& completionTimes
) {
    if (!tasks.is_array()) return;
    for (const auto& task : tasks) {
        AccumulateTaskObject(
            task,
            source,
            snapshot,
            creationTimes,
            completionTimes
        );
    }
}

void AccumulateTreeNodes(
    const json& nodes,
    StatisticsSnapshot& snapshot,
    std::vector<std::int64_t>& creationTimes,
    std::vector<std::int64_t>& completionTimes
) {
    if (!nodes.is_array()) return;

    for (const auto& node : nodes) {
        if (!node.is_object()) continue;
        const std::string type = node.value("type", node.value("kind", std::string{}));
        if (type == "folder" || type == "Folder") continue;
        AccumulateTaskObject(
            node,
            "SentinelTasks",
            snapshot,
            creationTimes,
            completionTimes
        );
    }
}

std::vector<TimePointValue> BuildCumulativeHistory(std::vector<std::int64_t> epochs) {
    std::sort(epochs.begin(), epochs.end());
    std::vector<TimePointValue> result;
    result.reserve(epochs.size());
    std::size_t cumulative = 0;
    for (const auto epoch : epochs) {
        ++cumulative;
        result.push_back({epoch, static_cast<double>(cumulative)});
    }
    return result;
}

bool LocalTime(std::int64_t epoch, std::tm& local) {
    const std::time_t raw = static_cast<std::time_t>(epoch);
#ifdef _WIN32
    return localtime_s(&local, &raw) == 0;
#else
    return localtime_r(&raw, &local) != nullptr;
#endif
}

std::int64_t LocalDayStart(std::int64_t epoch) {
    std::tm local{};
    if (!LocalTime(epoch, local)) return epoch;
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    const std::time_t result = std::mktime(&local);
    return result == static_cast<std::time_t>(-1)
        ? epoch
        : static_cast<std::int64_t>(result);
}

std::int64_t NextLocalDay(std::int64_t dayStart) {
    std::tm local{};
    if (!LocalTime(dayStart, local)) return dayStart + 86400;
    local.tm_mday += 1;
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    const std::time_t result = std::mktime(&local);
    return result == static_cast<std::time_t>(-1)
        ? dayStart + 86400
        : static_cast<std::int64_t>(result);
}

std::int64_t LocalWeekStart(std::int64_t epoch) {
    const std::int64_t dayStart = LocalDayStart(epoch);
    std::tm local{};
    if (!LocalTime(dayStart, local)) return dayStart;

    // tm_wday: Sunday=0. Convert to days since Monday.
    const int daysSinceMonday = (local.tm_wday + 6) % 7;
    local.tm_mday -= daysSinceMonday;
    local.tm_hour = 0;
    local.tm_min = 0;
    local.tm_sec = 0;
    local.tm_isdst = -1;
    const std::time_t result = std::mktime(&local);
    return result == static_cast<std::time_t>(-1)
        ? dayStart - static_cast<std::int64_t>(daysSinceMonday) * 86400
        : static_cast<std::int64_t>(result);
}

void BuildPeriodWork(StatisticsSnapshot& snapshot) {
    std::map<std::int64_t, PeriodWork> daily;

    for (const auto& task : snapshot.taskFragments) {
        for (const auto& fragment : task.fragments) {
            std::int64_t cursor = fragment.startedAtEpoch;
            const std::int64_t end = fragment.endedAtEpoch;
            if (cursor <= 0 || end <= cursor) continue;

            while (cursor < end) {
                const std::int64_t dayStart = LocalDayStart(cursor);
                const std::int64_t nextDay = std::max(
                    dayStart + 1,
                    NextLocalDay(dayStart)
                );
                const std::int64_t sliceEnd = std::min(end, nextDay);
                if (sliceEnd <= cursor) break;

                auto& period = daily[dayStart];
                period.periodStartEpoch = dayStart;
                period.trackedSeconds += static_cast<std::uint64_t>(sliceEnd - cursor);
                ++period.fragmentSlices;
                cursor = sliceEnd;
            }
        }
    }

    snapshot.dailyWork.clear();
    snapshot.dailyWork.reserve(daily.size());
    for (const auto& [epoch, period] : daily) {
        (void)epoch;
        snapshot.dailyWork.push_back(period);
    }

    std::map<std::int64_t, PeriodWork> weekly;
    for (const auto& day : snapshot.dailyWork) {
        const std::int64_t weekStart = LocalWeekStart(day.periodStartEpoch);
        auto& week = weekly[weekStart];
        week.periodStartEpoch = weekStart;
        week.trackedSeconds += day.trackedSeconds;
        week.fragmentSlices += day.fragmentSlices;
    }

    snapshot.weeklyWork.clear();
    snapshot.weeklyWork.reserve(weekly.size());
    for (const auto& [epoch, period] : weekly) {
        (void)epoch;
        snapshot.weeklyWork.push_back(period);
    }
}

void ReadProjects(const json& root, StatisticsSnapshot& snapshot) {
    const json* projects = nullptr;
    if (root.contains("statistics") && root["statistics"].is_object() &&
        root["statistics"].contains("projects")) {
        projects = &root["statistics"]["projects"];
    } else if (root.contains("projects")) {
        projects = &root["projects"];
    }

    if (!projects || !projects->is_array()) return;

    for (const auto& projectJson : *projects) {
        if (!projectJson.is_object()) continue;
        ProjectSeries project;
        project.id = projectJson.value("id", std::string{});
        project.name = projectJson.value("name", project.id);
        if (project.name.empty()) project.name = "Unnamed project";

        if (projectJson.contains("loc_history") && projectJson["loc_history"].is_array()) {
            for (const auto& point : projectJson["loc_history"]) {
                if (!point.is_object()) continue;
                std::int64_t epoch = 0;
                if (point.contains("epoch")) epoch = ReadEpoch(point["epoch"]);
                else if (point.contains("timestamp")) epoch = ReadEpoch(point["timestamp"]);

                double lines = 0.0;
                if (point.contains("lines") && point["lines"].is_number()) lines = point["lines"].get<double>();
                else if (point.contains("loc") && point["loc"].is_number()) lines = point["loc"].get<double>();

                if (epoch > 0) project.locHistory.push_back({epoch, lines});
            }
        }

        std::sort(project.locHistory.begin(), project.locHistory.end(), [](const auto& a, const auto& b) {
            return a.epochSeconds < b.epochSeconds;
        });
        snapshot.projects.push_back(std::move(project));
    }
}

} // namespace

bool StatisticsData::Load(const std::filesystem::path& path, std::string& errorMessage) {
    json root;
    bool exists = false;
    if (!SentinelShared::JsonDataStore::Read(path, root, exists, errorMessage)) {
        return false;
    }
    if (!exists) {
        errorMessage = "Could not open JSON file: " + path.string();
        return false;
    }

    StatisticsSnapshot next;
    std::vector<std::int64_t> creationTimes;
    std::vector<std::int64_t> completionTimes;

    const bool hasCanonicalSharedTasks =
        root.contains("sharedTasks") &&
        root["sharedTasks"].is_object() &&
        root["sharedTasks"].contains("tasks") &&
        root["sharedTasks"]["tasks"].is_array();

    if (hasCanonicalSharedTasks) {
        AccumulateTaskArray(
            root["sharedTasks"]["tasks"],
            "Shared",
            next,
            creationTimes,
            completionTimes
        );
    } else {
        if (root.contains("sentinel") && root["sentinel"].is_object() &&
            root["sentinel"].contains("tasks")) {
            AccumulateTaskArray(
                root["sentinel"]["tasks"],
                "Sentinel",
                next,
                creationTimes,
                completionTimes
            );
        } else if (root.contains("tasks")) {
            AccumulateTaskArray(
                root["tasks"],
                "Sentinel",
                next,
                creationTimes,
                completionTimes
            );
        }

        if (root.contains("sentinelTasks") && root["sentinelTasks"].is_object() &&
            root["sentinelTasks"].contains("nodes")) {
            AccumulateTreeNodes(
                root["sentinelTasks"]["nodes"],
                next,
                creationTimes,
                completionTimes
            );
        }
    }

    next.createdTaskHistory = BuildCumulativeHistory(std::move(creationTimes));
    next.completedTaskHistory = BuildCumulativeHistory(std::move(completionTimes));

    std::sort(next.taskFragments.begin(), next.taskFragments.end(), [](const auto& left, const auto& right) {
        if (left.fragments.empty() != right.fragments.empty()) return !left.fragments.empty();
        if (left.fragments.empty()) return left.name < right.name;
        return left.fragments.front().startedAtEpoch < right.fragments.front().startedAtEpoch;
    });

    std::sort(next.taskAnalytics.begin(), next.taskAnalytics.end(), [](const auto& left, const auto& right) {
        if (left.trackedSeconds != right.trackedSeconds) {
            return left.trackedSeconds > right.trackedSeconds;
        }
        return left.name < right.name;
    });

    BuildPeriodWork(next);
    ReadProjects(root, next);

    snapshot_ = std::move(next);
    path_ = path;
    errorMessage.clear();
    return true;
}

const StatisticsSnapshot& StatisticsData::Snapshot() const noexcept {
    return snapshot_;
}

const std::filesystem::path& StatisticsData::Path() const noexcept {
    return path_;
}
