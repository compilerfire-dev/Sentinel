#pragma once

#include <filesystem>
#include <string>

namespace SentinelShared {

constexpr int SharedTaskWorldVersion = 1;

// Ensures the canonical shared task world exists. When upgrading from the
// pre-shared-task schema, legacy Sentinel/SentinelTasks task payloads are
// intentionally purged so two conflicting task domains are never merged.
// Non-task settings and statistics are preserved.
bool EnsureSharedTaskWorld(
    const std::filesystem::path& path,
    bool& purgedLegacyTaskWorld,
    std::string& errorMessage
);

} // namespace SentinelShared
