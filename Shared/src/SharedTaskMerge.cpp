#include "SharedTaskMerge.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace SentinelShared {
namespace {

bool ParseTaskArray(
    const nlohmann::json& tasks,
    std::unordered_map<std::string, nlohmann::json>& byId,
    std::vector<std::string>& order,
    std::string& errorMessage
) {
    if (!tasks.is_array()) {
        errorMessage = "Canonical shared task collection must be an array.";
        return false;
    }

    byId.clear();
    order.clear();
    for (const auto& item : tasks) {
        if (!item.is_object()) {
            errorMessage = "Shared task entries must be JSON objects.";
            return false;
        }
        const std::string id = item.value("id", std::string{});
        if (id.empty()) {
            errorMessage = "Shared task ID cannot be empty.";
            return false;
        }
        if (byId.contains(id)) {
            errorMessage = "Duplicate global task ID: " + id;
            return false;
        }
        byId.emplace(id, item);
        order.push_back(id);
    }

    errorMessage.clear();
    return true;
}

std::string Dump(const nlohmann::json& value) {
    return value.dump();
}

} // namespace

bool BuildSharedTaskBaseline(
    const nlohmann::json& tasks,
    SharedTaskBaseline& baseline,
    std::string& errorMessage
) {
    std::unordered_map<std::string, nlohmann::json> byId;
    std::vector<std::string> order;
    if (!ParseTaskArray(tasks, byId, order, errorMessage)) return false;

    baseline.clear();
    for (const auto& id : order) baseline[id] = Dump(byId.at(id));
    errorMessage.clear();
    return true;
}

bool MergeCanonicalTasks(
    nlohmann::json& sharedTaskWorld,
    const nlohmann::json& localTasks,
    const SharedTaskBaseline& baseline,
    SharedTaskBaseline& nextBaseline,
    std::string& errorMessage
) {
    if (!sharedTaskWorld.is_object()) sharedTaskWorld = nlohmann::json::object();
    if (!sharedTaskWorld.contains("tasks")) sharedTaskWorld["tasks"] = nlohmann::json::array();

    std::unordered_map<std::string, nlohmann::json> latestById;
    std::vector<std::string> latestOrder;
    if (!ParseTaskArray(sharedTaskWorld["tasks"], latestById, latestOrder, errorMessage)) {
        return false;
    }

    std::unordered_map<std::string, nlohmann::json> localById;
    std::vector<std::string> localOrder;
    if (!ParseTaskArray(localTasks, localById, localOrder, errorMessage)) return false;

    for (const auto& [id, baselineDump] : baseline) {
        const auto local = localById.find(id);
        const auto latest = latestById.find(id);
        const bool localExists = local != localById.end();
        const bool latestExists = latest != latestById.end();
        const bool localChanged = !localExists || Dump(local->second) != baselineDump;
        const bool externalChanged = !latestExists || Dump(latest->second) != baselineDump;

        if (localChanged && externalChanged) {
            const bool sameResult =
                (!localExists && !latestExists) ||
                (localExists && latestExists && Dump(local->second) == Dump(latest->second));
            if (!sameResult) {
                errorMessage = "Concurrent shared-task conflict for ID '" + id +
                    "'. Reload the JSON dataset before changing this task again.";
                return false;
            }
        }

        if (localChanged && !externalChanged) {
            if (localExists) latestById[id] = local->second;
            else latestById.erase(id);
        }
    }

    for (const auto& id : localOrder) {
        if (baseline.contains(id)) continue;
        const auto& local = localById.at(id);
        const auto latest = latestById.find(id);
        if (latest != latestById.end() && Dump(latest->second) != Dump(local)) {
            errorMessage = "Global task ID '" + id +
                "' was created by another process. Reload before reusing that ID.";
            return false;
        }
        latestById[id] = local;
    }

    nlohmann::json merged = nlohmann::json::array();
    std::unordered_set<std::string> emitted;
    for (const auto& id : latestOrder) {
        const auto iterator = latestById.find(id);
        if (iterator == latestById.end()) continue;
        merged.push_back(iterator->second);
        emitted.insert(id);
    }
    for (const auto& id : localOrder) {
        if (emitted.contains(id)) continue;
        const auto iterator = latestById.find(id);
        if (iterator == latestById.end()) continue;
        merged.push_back(iterator->second);
        emitted.insert(id);
    }

    sharedTaskWorld["tasks"] = std::move(merged);

    // Keep the next baseline aligned with this process's local view only.
    // Externally added tasks remain outside the baseline until the process
    // reloads, which prevents a later stale save from treating them as deletions.
    nextBaseline.clear();
    for (const auto& id : localOrder) nextBaseline[id] = Dump(localById.at(id));

    errorMessage.clear();
    return true;
}

} // namespace SentinelShared
