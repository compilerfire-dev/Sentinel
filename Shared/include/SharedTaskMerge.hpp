#pragma once

#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>

namespace SentinelShared {

using SharedTaskBaseline = std::unordered_map<std::string, std::string>;

bool BuildSharedTaskBaseline(
    const nlohmann::json& tasks,
    SharedTaskBaseline& baseline,
    std::string& errorMessage
);

// Merges a process-local task array against the latest canonical task array.
// `baseline` is the canonical snapshot this process originally loaded.
// Changes to different task IDs merge. Conflicting changes to the same task ID
// are rejected rather than silently overwriting another process.
bool MergeCanonicalTasks(
    nlohmann::json& sharedTaskWorld,
    const nlohmann::json& localTasks,
    const SharedTaskBaseline& baseline,
    SharedTaskBaseline& nextBaseline,
    std::string& errorMessage
);

} // namespace SentinelShared
