#include "TaskDataStore.hpp"

#include "JsonDataStore.hpp"
#include "SharedTaskWorld.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <unordered_map>
#include <utility>
#include <vector>

using nlohmann::json;

namespace {

json ColorToJson(const RgbColor& color) {
    return {{"red", color.red}, {"green", color.green}, {"blue", color.blue}};
}

RgbColor ColorFromJson(const json& value, const RgbColor& fallback) {
    return {
        value.value("red", fallback.red),
        value.value("green", fallback.green),
        value.value("blue", fallback.blue)
    };
}

json FragmentToJson(const SentinelShared::TimeFragment& fragment) {
    return {
        {"started_at_epoch", SentinelShared::EpochSeconds(fragment.startedAt)},
        {"ended_at_epoch", fragment.IsOpen() ? 0 : SentinelShared::EpochSeconds(fragment.endedAt)},
        {"duration_seconds", fragment.duration.count()}
    };
}

SentinelShared::TimeFragment FragmentFromJson(const json& value) {
    SentinelShared::TimeFragment fragment;
    const auto started = value.value("started_at_epoch", 0LL);
    const auto ended = value.value("ended_at_epoch", 0LL);
    const auto duration = std::max(0LL, value.value("duration_seconds", 0LL));
    fragment.startedAt = SentinelShared::TimePointFromEpoch(started);
    fragment.endedAt = ended > 0
        ? SentinelShared::TimePointFromEpoch(ended)
        : SentinelShared::TimeFragment::Clock::time_point{};
    fragment.duration = std::chrono::seconds(duration);
    return fragment;
}

json TaskNodeToSharedJson(const TaskNode& node) {
    const auto completedEpoch = node.completed
        ? SentinelShared::EpochSeconds(node.completedAt)
        : 0LL;

    json item = {
        {"id", node.id},
        {"name", node.name},
        {"created_at_epoch", SentinelShared::EpochSeconds(node.createdAt)},
        {"elapsed_seconds", node.Elapsed().count()},
        {"running", node.running},
        {"completed", node.completed},
        {"completed_at_epoch", completedEpoch},
        {"time_fragments", json::array()}
    };

    for (const auto& fragment : node.TimeFragments()) {
        item["time_fragments"].push_back(FragmentToJson(fragment));
    }

    if (node.foregroundColor && node.backgroundColor) {
        item["color"] = {
            {"foreground", ColorToJson(*node.foregroundColor)},
            {"background", ColorToJson(*node.backgroundColor)}
        };
    }

    return item;
}

bool RestoreTaskFromSharedJson(
    const json& item,
    TaskNode& node,
    std::string& errorMessage
) {
    try {
        node.name = item.value("name", node.id);
        const auto elapsed = std::chrono::seconds(
            item.value("elapsed_seconds", 0LL)
        );
        const bool completed = item.value("completed", false);
        const bool running = item.value("running", false) && !completed;
        const auto createdEpoch = item.value("created_at_epoch", 0LL);
        const auto completedEpoch = item.value("completed_at_epoch", 0LL);

        std::vector<SentinelShared::TimeFragment> fragments;
        if (item.contains("time_fragments") && item["time_fragments"].is_array()) {
            for (const auto& fragmentJson : item["time_fragments"]) {
                if (fragmentJson.is_object()) {
                    fragments.push_back(FragmentFromJson(fragmentJson));
                }
            }
        }

        node.RestoreTiming(
            elapsed,
            completed,
            running,
            SentinelShared::TimePointFromEpoch(createdEpoch),
            SentinelShared::TimePointFromEpoch(completedEpoch),
            std::move(fragments)
        );

        node.foregroundColor.reset();
        node.backgroundColor.reset();
        if (item.contains("color") && item["color"].is_object()) {
            const auto& color = item["color"];
            if (color.contains("foreground") && color.contains("background")) {
                node.foregroundColor = ColorFromJson(
                    color["foreground"], RgbColor{255, 255, 255}
                );
                node.backgroundColor = ColorFromJson(
                    color["background"], RgbColor{0, 0, 0}
                );
            }
        }

        errorMessage.clear();
        return true;
    } catch (const std::exception& exception) {
        errorMessage = "Invalid canonical shared task: " + std::string(exception.what());
        return false;
    }
}

} // namespace

TaskDataStore::TaskDataStore(std::filesystem::path path)
    : path_(std::move(path)) {}

bool TaskDataStore::Load(
    TaskTree& tree,
    TreeDisplaySettings& displaySettings,
    std::unordered_map<std::string, RgbColor>& definedColors,
    std::chrono::seconds& autoSaveInterval,
    std::string& errorMessage
) const {
    try {
        bool purgedLegacyWorld = false;
        if (!SentinelShared::EnsureSharedTaskWorld(
                path_,
                purgedLegacyWorld,
                errorMessage)) {
            return false;
        }

        json root;
        bool exists = false;
        if (!SentinelShared::JsonDataStore::Read(path_, root, exists, errorMessage)) {
            return false;
        }
        if (!exists || !root.contains("sharedTasks") ||
            !root["sharedTasks"].is_object() ||
            !root["sharedTasks"].contains("tasks") ||
            !root["sharedTasks"]["tasks"].is_array()) {
            errorMessage = "Shared task world is missing or invalid.";
            return false;
        }

        TaskTree loadedTree;
        TreeDisplaySettings loadedDisplay;
        std::unordered_map<std::string, RgbColor> loadedColors;
        std::chrono::seconds loadedAutoSaveInterval{1};

        const json* layout = nullptr;
        if (root.contains("sentinelTasks") && root["sentinelTasks"].is_object()) {
            layout = &root["sentinelTasks"];
            loadedAutoSaveInterval = std::chrono::seconds(
                std::max(1LL, layout->value("auto_save_seconds", 1LL))
            );

            if (layout->contains("display") && (*layout)["display"].is_object()) {
                const auto& display = (*layout)["display"];
                if (display.contains("foreground")) {
                    loadedDisplay.foreground = ColorFromJson(
                        display["foreground"], loadedDisplay.foreground
                    );
                }
                if (display.contains("background")) {
                    loadedDisplay.background = ColorFromJson(
                        display["background"], loadedDisplay.background
                    );
                }
            }

            if (layout->contains("defined_colors") && (*layout)["defined_colors"].is_object()) {
                for (auto iterator = (*layout)["defined_colors"].begin();
                     iterator != (*layout)["defined_colors"].end(); ++iterator) {
                    loadedColors[iterator.key()] = ColorFromJson(
                        iterator.value(), RgbColor{255, 255, 255}
                    );
                }
            }
        }

        std::unordered_map<std::string, json> canonicalById;
        std::vector<std::string> canonicalOrder;
        for (const auto& item : root["sharedTasks"]["tasks"]) {
            if (!item.is_object()) continue;
            const std::string id = item.value("id", std::string{});
            if (id.empty()) {
                errorMessage = "Canonical shared task has an empty ID.";
                return false;
            }
            if (canonicalById.contains(id)) {
                errorMessage = "Duplicate global task ID: " + id;
                return false;
            }
            canonicalById.emplace(id, item);
            canonicalOrder.push_back(id);
        }

        SentinelShared::SharedTaskBaseline loadedBaseline;
        std::string baselineError;
        if (!SentinelShared::BuildSharedTaskBaseline(
                root["sharedTasks"]["tasks"],
                loadedBaseline,
                baselineError)) {
            errorMessage = baselineError;
            return false;
        }

        std::vector<json> taskPlacements;
        if (layout && layout->contains("nodes")) {
            if (!(*layout)["nodes"].is_array()) {
                errorMessage = "sentinelTasks.nodes must be an array.";
                return false;
            }

            // Folders are layout-only and are restored first so task placement
            // can safely reference any folder parent.
            for (const auto& item : (*layout)["nodes"]) {
                if (!item.is_object()) continue;
                const std::string type = item.value("type", std::string{});
                if (type != "folder") {
                    if (type == "task") taskPlacements.push_back(item);
                    continue;
                }

                const std::string id = item.at("id").get<std::string>();
                const std::string name = item.value("name", id);
                std::string parent;
                if (item.contains("parent") && item["parent"].is_string()) {
                    parent = item["parent"].get<std::string>();
                }

                if (canonicalById.contains(id)) {
                    errorMessage = "Folder ID collides with global task ID: " + id;
                    return false;
                }

                std::string addError;
                if (!loadedTree.AddNode(NodeKind::Folder, id, parent, name, addError)) {
                    errorMessage = "Could not load folder '" + id + "': " + addError;
                    return false;
                }

                if (TaskNode* node = loadedTree.GetNode(id)) {
                    node->description = item.value("description", std::string{});
                    const auto createdEpoch = item.value("created_at_epoch", 0LL);
                    if (createdEpoch > 0) {
                        node->createdAt = SentinelShared::TimePointFromEpoch(createdEpoch);
                    }
                    if (item.contains("color") && item["color"].is_object()) {
                        const auto& color = item["color"];
                        if (color.contains("foreground") && color.contains("background")) {
                            node->foregroundColor = ColorFromJson(
                                color["foreground"], RgbColor{255, 255, 255}
                            );
                            node->backgroundColor = ColorFromJson(
                                color["background"], RgbColor{0, 0, 0}
                            );
                        }
                    }
                }
            }
        }

        // Restore explicit tree placement for canonical tasks.
        for (const auto& placement : taskPlacements) {
            const std::string id = placement.value("id", std::string{});
            const auto canonical = canonicalById.find(id);
            if (canonical == canonicalById.end()) {
                // Stale placement for a task erased from the shared registry.
                continue;
            }
            if (loadedTree.GetNode(id)) {
                errorMessage = "Duplicate SentinelTasks node/global task ID: " + id;
                return false;
            }

            std::string parent;
            if (placement.contains("parent") && placement["parent"].is_string()) {
                parent = placement["parent"].get<std::string>();
            }
            const std::string name = canonical->second.value("name", id);

            std::string addError;
            if (!loadedTree.AddNode(NodeKind::Task, id, parent, name, addError)) {
                errorMessage = "Could not place shared task '" + id + "': " + addError;
                return false;
            }
            if (TaskNode* node = loadedTree.GetNode(id)) {
                node->description = placement.value("description", std::string{});
            }
        }

        // Any task created from Sentinel has no tree placement yet. It appears
        // at SentinelTasks root automatically with the same global ID.
        for (const auto& id : canonicalOrder) {
            if (loadedTree.GetNode(id)) continue;
            const auto& item = canonicalById.at(id);
            const std::string name = item.value("name", id);
            std::string addError;
            if (!loadedTree.AddNode(NodeKind::Task, id, "root", name, addError)) {
                errorMessage = "Could not materialize shared task '" + id + "': " + addError;
                return false;
            }
        }

        // Canonical task state always wins over tree metadata.
        for (const auto& id : canonicalOrder) {
            TaskNode* node = loadedTree.GetNode(id);
            if (!node || node->kind != NodeKind::Task) {
                errorMessage = "Shared task is not represented as a task node: " + id;
                return false;
            }
            std::string restoreError;
            if (!RestoreTaskFromSharedJson(canonicalById.at(id), *node, restoreError)) {
                errorMessage = restoreError;
                return false;
            }
        }

        tree = std::move(loadedTree);
        sharedTaskBaseline_ = std::move(loadedBaseline);
        displaySettings = loadedDisplay;
        definedColors = std::move(loadedColors);
        autoSaveInterval = loadedAutoSaveInterval;
        errorMessage.clear();
        (void)purgedLegacyWorld;
        return true;
    } catch (const std::exception& exception) {
        errorMessage = "Failed to load shared SentinelTasks world: " + std::string(exception.what());
        return false;
    }
}

bool TaskDataStore::Save(
    const TaskTree& tree,
    const TreeDisplaySettings& displaySettings,
    const std::unordered_map<std::string, RgbColor>& definedColors,
    std::chrono::seconds autoSaveInterval,
    std::string& errorMessage
) const {
    json layout;
    layout["version"] = 3;
    layout["auto_save_seconds"] = std::max<long long>(1, autoSaveInterval.count());
    layout["display"] = {
        {"foreground", ColorToJson(displaySettings.foreground)},
        {"background", ColorToJson(displaySettings.background)}
    };
    layout["defined_colors"] = json::object();
    for (const auto& [id, color] : definedColors) {
        layout["defined_colors"][id] = ColorToJson(color);
    }
    layout["nodes"] = json::array();

    json localSharedTasks = json::array();

    for (const auto& visible : tree.Flatten()) {
        if (!visible.node) continue;
        const TaskNode& node = *visible.node;

        if (node.kind == NodeKind::Task) {
            // Task state lives only in the canonical registry. The tree stores
            // only placement/description linkage using that same global ID.
            localSharedTasks.push_back(TaskNodeToSharedJson(node));
            layout["nodes"].push_back({
                {"id", node.id},
                {"type", "task"},
                {"parent", node.parentId},
                {"description", node.description}
            });
            continue;
        }

        json folder = {
            {"id", node.id},
            {"name", node.name},
            {"description", node.description},
            {"parent", node.parentId},
            {"type", "folder"},
            {"created_at_epoch", SentinelShared::EpochSeconds(node.createdAt)}
        };
        if (node.foregroundColor && node.backgroundColor) {
            folder["color"] = {
                {"foreground", ColorToJson(*node.foregroundColor)},
                {"background", ColorToJson(*node.backgroundColor)}
            };
        }
        layout["nodes"].push_back(std::move(folder));
    }

    SentinelShared::SharedTaskBaseline nextBaseline;
    const bool saved = SentinelShared::JsonDataStore::Update(
        path_,
        [layout = std::move(layout),
         localSharedTasks = std::move(localSharedTasks),
         this,
         &nextBaseline](json& root, std::string& mutationError) mutable {
            if (!root.contains("sharedTasks") || !root["sharedTasks"].is_object()) {
                root["sharedTasks"] = json::object();
            }
            auto& shared = root["sharedTasks"];
            shared["version"] = SentinelShared::SharedTaskWorldVersion;
            if (!SentinelShared::MergeCanonicalTasks(
                    shared,
                    localSharedTasks,
                    sharedTaskBaseline_,
                    nextBaseline,
                    mutationError)) {
                return false;
            }

            root["sentinelTasks"] = std::move(layout);
            if (root.contains("sentinel") && root["sentinel"].is_object()) {
                root["sentinel"].erase("tasks");
            }
            root.erase("tasks");
            mutationError.clear();
            return true;
        },
        errorMessage
    );

    if (saved) sharedTaskBaseline_ = std::move(nextBaseline);
    return saved;
}

void TaskDataStore::SetPath(std::filesystem::path path) {
    path_ = std::move(path);
}

const std::filesystem::path& TaskDataStore::Path() const noexcept {
    return path_;
}
