#include "JsonDataStore.hpp"
#include "SharedTaskWorld.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("sentinel-shared-world-test-" + std::to_string(static_cast<long long>(::getpid())) + ".json");

    std::string error;
    if (!SentinelShared::JsonDataStore::Update(
            path,
            [](nlohmann::json& root, std::string& mutationError) {
                root["sentinel"] = {
                    {"auto_save_seconds", 20},
                    {"tasks", nlohmann::json::array({{{"id", "legacy-a"}, {"name", "A"}}})}
                };
                root["sentinelTasks"] = {
                    {"display", {{"foreground", {{"red", 255}}}}},
                    {"nodes", nlohmann::json::array({{{"id", "legacy-b"}, {"type", "task"}}})}
                };
                root["statistics"] = {
                    {"projects", nlohmann::json::array({{{"id", "keep-me"}}})}
                };
                mutationError.clear();
                return true;
            },
            error)) {
        std::cerr << error << '\n';
        return 1;
    }

    bool purged = false;
    if (!SentinelShared::EnsureSharedTaskWorld(path, purged, error)) {
        std::cerr << error << '\n';
        return 2;
    }
    if (!purged) {
        std::cerr << "Expected legacy task world to be purged.\n";
        return 3;
    }

    nlohmann::json root;
    bool exists = false;
    if (!SentinelShared::JsonDataStore::Read(path, root, exists, error) || !exists) {
        std::cerr << error << '\n';
        return 4;
    }

    const bool valid =
        root.contains("sharedTasks") &&
        root["sharedTasks"].value("version", 0) == SentinelShared::SharedTaskWorldVersion &&
        root["sharedTasks"]["tasks"].is_array() &&
        root["sharedTasks"]["tasks"].empty() &&
        root["sentinel"].value("auto_save_seconds", 0) == 20 &&
        !root["sentinel"].contains("tasks") &&
        root["sentinelTasks"]["nodes"].is_array() &&
        root["sentinelTasks"]["nodes"].empty() &&
        root.contains("statistics") &&
        root["statistics"]["projects"].size() == 1;

    std::filesystem::remove(path);
    std::filesystem::remove(SentinelShared::JsonDataStore::LockPath(path));

    if (!valid) {
        std::cerr << "Migrated JSON did not preserve settings/statistics or purge legacy tasks correctly.\n";
        return 5;
    }

    return 0;
}
