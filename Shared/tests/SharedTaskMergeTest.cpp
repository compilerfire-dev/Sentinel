#include "SharedTaskMerge.hpp"

#include <nlohmann/json.hpp>

#include <iostream>
#include <string>

namespace {

nlohmann::json Task(const char* id, int value) {
    return {{"id", id}, {"value", value}};
}

bool HasValue(const nlohmann::json& tasks, const std::string& id, int value) {
    for (const auto& task : tasks) {
        if (task.value("id", std::string{}) == id && task.value("value", -1) == value) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    std::string error;

    const nlohmann::json baselineTasks = nlohmann::json::array({Task("a", 1), Task("b", 1)});
    SentinelShared::SharedTaskBaseline baseline;
    if (!SentinelShared::BuildSharedTaskBaseline(baselineTasks, baseline, error)) {
        std::cerr << error << '\n';
        return 1;
    }

    nlohmann::json shared = {
        {"tasks", nlohmann::json::array({Task("a", 1), Task("b", 1), Task("external", 7)})}
    };
    const nlohmann::json local = nlohmann::json::array({Task("a", 1), Task("b", 2)});
    SentinelShared::SharedTaskBaseline next;
    if (!SentinelShared::MergeCanonicalTasks(shared, local, baseline, next, error)) {
        std::cerr << error << '\n';
        return 2;
    }
    if (!HasValue(shared["tasks"], "a", 1) ||
        !HasValue(shared["tasks"], "b", 2) ||
        !HasValue(shared["tasks"], "external", 7)) {
        std::cerr << "Different-task merge or external addition preservation failed.\n";
        return 3;
    }

    SentinelShared::SharedTaskBaseline deletionBaseline;
    const nlohmann::json deletionBaseTasks = nlohmann::json::array({Task("deleted", 1)});
    if (!SentinelShared::BuildSharedTaskBaseline(
            deletionBaseTasks,
            deletionBaseline,
            error)) {
        std::cerr << error << '\n';
        return 4;
    }
    nlohmann::json deletedShared = {{"tasks", nlohmann::json::array()}};
    SentinelShared::SharedTaskBaseline deletionNext;
    if (!SentinelShared::MergeCanonicalTasks(
            deletedShared,
            deletionBaseTasks,
            deletionBaseline,
            deletionNext,
            error) ||
        !deletedShared["tasks"].empty()) {
        std::cerr << "External deletion was resurrected by a stale unchanged view.\n";
        return 5;
    }

    SentinelShared::SharedTaskBaseline conflictBaseline;
    const nlohmann::json conflictBaseTasks = nlohmann::json::array({Task("same", 1)});
    if (!SentinelShared::BuildSharedTaskBaseline(
            conflictBaseTasks,
            conflictBaseline,
            error)) {
        std::cerr << error << '\n';
        return 6;
    }
    nlohmann::json conflictShared = {{"tasks", nlohmann::json::array({Task("same", 2)})}};
    const nlohmann::json conflictLocal = nlohmann::json::array({Task("same", 3)});
    SentinelShared::SharedTaskBaseline conflictNext;
    if (SentinelShared::MergeCanonicalTasks(
            conflictShared,
            conflictLocal,
            conflictBaseline,
            conflictNext,
            error)) {
        std::cerr << "Expected same-task concurrent modification conflict.\n";
        return 7;
    }

    return 0;
}
