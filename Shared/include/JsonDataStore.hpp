#pragma once

#include <filesystem>
#include <functional>
#include <string>

#include <nlohmann/json.hpp>

namespace SentinelShared {

class JsonDataStore {
public:
    using Json = nlohmann::json;
    using Mutator = std::function<bool(Json& root, std::string& errorMessage)>;

    // Reads a JSON document while holding a shared lock. A missing file is
    // reported through `exists` and produces an empty object in `root`.
    static bool Read(
        const std::filesystem::path& path,
        Json& root,
        bool& exists,
        std::string& errorMessage
    );

    // Holds an exclusive sidecar lock for the complete read-modify-write
    // transaction, then commits with fsync + atomic rename in the same folder.
    static bool Update(
        const std::filesystem::path& path,
        const Mutator& mutator,
        std::string& errorMessage
    );

    static std::filesystem::path LockPath(const std::filesystem::path& path);
};

} // namespace SentinelShared
