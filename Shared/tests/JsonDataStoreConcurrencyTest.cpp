#include "JsonDataStore.hpp"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <iostream>
#include <string>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool IncrementSection(
    const std::filesystem::path& path,
    const std::string& section,
    int iterations
) {
    for (int index = 0; index < iterations; ++index) {
        std::string error;
        const bool updated = SentinelShared::JsonDataStore::Update(
            path,
            [&](nlohmann::json& root, std::string& mutationError) {
                const int current = root.value(section, 0);
                root[section] = current + 1;
                mutationError.clear();
                return true;
            },
            error
        );
        if (!updated) {
            std::cerr << section << " update failed: " << error << '\n';
            return false;
        }
    }
    return true;
}

} // namespace

int main() {
    constexpr int Iterations = 100;
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() /
        ("sentinel-json-store-test-" +
         std::to_string(static_cast<long long>(::getpid())) + ".json");

    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::filesystem::remove(SentinelShared::JsonDataStore::LockPath(path), ignored);

    const pid_t first = ::fork();
    if (first == 0) {
        _exit(IncrementSection(path, "sentinel", Iterations) ? 0 : 1);
    }
    if (first < 0) {
        std::cerr << "fork() failed for first writer\n";
        return 1;
    }

    const pid_t second = ::fork();
    if (second == 0) {
        _exit(IncrementSection(path, "sentinelTasks", Iterations) ? 0 : 1);
    }
    if (second < 0) {
        std::cerr << "fork() failed for second writer\n";
        return 1;
    }

    int firstStatus = 0;
    int secondStatus = 0;
    ::waitpid(first, &firstStatus, 0);
    ::waitpid(second, &secondStatus, 0);

    if (!WIFEXITED(firstStatus) || WEXITSTATUS(firstStatus) != 0 ||
        !WIFEXITED(secondStatus) || WEXITSTATUS(secondStatus) != 0) {
        std::cerr << "One of the concurrent writers failed.\n";
        return 1;
    }

    nlohmann::json root;
    bool exists = false;
    std::string error;
    if (!SentinelShared::JsonDataStore::Read(path, root, exists, error)) {
        std::cerr << "Final read failed: " << error << '\n';
        return 1;
    }

    const bool valid = exists &&
        root.value("sentinel", 0) == Iterations &&
        root.value("sentinelTasks", 0) == Iterations;

    std::filesystem::remove(path, ignored);
    std::filesystem::remove(SentinelShared::JsonDataStore::LockPath(path), ignored);

    if (!valid) {
        std::cerr << "Concurrent updates were lost: " << root.dump() << '\n';
        return 1;
    }

    return 0;
}
