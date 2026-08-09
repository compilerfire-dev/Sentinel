#include "StatisticsData.hpp"

#include "JsonDataStore.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>

using nlohmann::json;

namespace {

std::int64_t ReadEpoch(const json& value) {
    if (value.is_number_integer()) return value.get<std::int64_t>();
    return 0;
}

std::uint64_t ReadElapsedSeconds(const json& item) {
    if (item.contains("elapsed_seconds") && item["elapsed_seconds"].is_number_unsigned()) {
        return item["elapsed_seconds"].get<std::uint64_t>();
    }
    if (item.contains("elapsed_seconds") && item["elapsed_seconds"].is_number_integer()) {
        const auto value = item["elapsed_seconds"].get<std::int64_t>();
        return value > 0 ? static_cast<std::uint64_t>(value) : 0;
    }
    return 0;
}

void AccumulateTaskArray(const json& tasks, StatisticsSnapshot& snapshot, std::vector<std::int64_t>& completionTimes) {
    if (!tasks.is_array()) return;

    for (const auto& task : tasks) {
        if (!task.is_object()) continue;
        ++snapshot.totalTasks;

        const bool completed = task.value("completed", false);
        const bool running = task.value("running", false);
        if (completed) ++snapshot.completedTasks;
        if (running) ++snapshot.runningTasks;
        snapshot.totalTrackedSeconds += ReadElapsedSeconds(task);

        if (completed) {
            std::int64_t epoch = 0;
            if (task.contains("completed_at_epoch")) epoch = ReadEpoch(task["completed_at_epoch"]);
            else if (task.contains("completion_epoch")) epoch = ReadEpoch(task["completion_epoch"]);
            if (epoch > 0) completionTimes.push_back(epoch);
        }
    }
}

void AccumulateTreeNodes(const json& nodes, StatisticsSnapshot& snapshot, std::vector<std::int64_t>& completionTimes) {
    if (!nodes.is_array()) return;

    for (const auto& node : nodes) {
        if (!node.is_object()) continue;
        const std::string type = node.value("type", node.value("kind", std::string{}));
        if (type == "folder" || type == "Folder") continue;

        ++snapshot.totalTasks;
        const bool completed = node.value("completed", false);
        const bool running = node.value("running", false);
        if (completed) ++snapshot.completedTasks;
        if (running) ++snapshot.runningTasks;
        snapshot.totalTrackedSeconds += ReadElapsedSeconds(node);

        if (completed) {
            std::int64_t epoch = 0;
            if (node.contains("completed_at_epoch")) epoch = ReadEpoch(node["completed_at_epoch"]);
            else if (node.contains("completion_epoch")) epoch = ReadEpoch(node["completion_epoch"]);
            if (epoch > 0) completionTimes.push_back(epoch);
        }
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
    std::vector<std::int64_t> completionTimes;

    if (root.contains("sentinel") && root["sentinel"].is_object() && root["sentinel"].contains("tasks")) {
        AccumulateTaskArray(root["sentinel"]["tasks"], next, completionTimes);
    } else if (root.contains("tasks")) {
        AccumulateTaskArray(root["tasks"], next, completionTimes);
    }

    if (root.contains("sentinelTasks") && root["sentinelTasks"].is_object() && root["sentinelTasks"].contains("nodes")) {
        AccumulateTreeNodes(root["sentinelTasks"]["nodes"], next, completionTimes);
    }

    std::sort(completionTimes.begin(), completionTimes.end());
    std::size_t cumulative = 0;
    for (const auto epoch : completionTimes) {
        ++cumulative;
        next.completedTaskHistory.push_back({epoch, static_cast<double>(cumulative)});
    }

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
