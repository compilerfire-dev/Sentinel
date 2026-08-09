#include "SharedTaskWorld.hpp"

#include "JsonDataStore.hpp"

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace SentinelShared {

bool EnsureSharedTaskWorld(
    const std::filesystem::path& path,
    bool& purgedLegacyTaskWorld,
    std::string& errorMessage
) {
    purgedLegacyTaskWorld = false;

    return JsonDataStore::Update(
        path,
        [&](json& root, std::string& mutationError) {
            const bool validWorld =
                root.contains("sharedTasks") &&
                root["sharedTasks"].is_object() &&
                root["sharedTasks"].value("version", 0) == SharedTaskWorldVersion &&
                root["sharedTasks"].contains("tasks") &&
                root["sharedTasks"]["tasks"].is_array();

            if (validWorld) {
                mutationError.clear();
                return true;
            }

            // Intentional destructive migration: do not guess how two legacy
            // task domains should be merged. Start one canonical task world.
            root["sharedTasks"] = {
                {"version", SharedTaskWorldVersion},
                {"tasks", json::array()}
            };

            if (root.contains("sentinel") && root["sentinel"].is_object()) {
                root["sentinel"].erase("tasks");
            }
            root.erase("tasks"); // legacy flat Sentinel schema

            if (root.contains("sentinelTasks") && root["sentinelTasks"].is_object()) {
                root["sentinelTasks"]["nodes"] = json::array();
            }

            purgedLegacyTaskWorld = true;
            mutationError.clear();
            return true;
        },
        errorMessage
    );
}

} // namespace SentinelShared
