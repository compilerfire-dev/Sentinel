#pragma once

#include <filesystem>
#include <optional>

namespace SentinelShared {

std::optional<std::filesystem::path> SelectJsonFile(
    const std::filesystem::path& currentPath = {}
);

} // namespace SentinelShared
