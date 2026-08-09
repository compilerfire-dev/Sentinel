#include "JsonDataStore.hpp"

#include <cerrno>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace SentinelShared {
namespace {

std::string ErrnoMessage(const std::string& prefix) {
    return prefix + ": " + std::strerror(errno);
}

bool EnsureParentDirectory(
    const std::filesystem::path& path,
    std::string& errorMessage
) {
    const auto parent = path.has_parent_path()
        ? path.parent_path()
        : std::filesystem::path{"."};

    std::error_code error;
    std::filesystem::create_directories(parent, error);
    if (error) {
        errorMessage = "Could not create JSON directory '" +
            parent.string() + "': " + error.message();
        return false;
    }
    return true;
}

class FileLock {
public:
    FileLock() = default;
    FileLock(const FileLock&) = delete;
    FileLock& operator=(const FileLock&) = delete;

    ~FileLock() {
        if (fileDescriptor_ >= 0) {
            while (::flock(fileDescriptor_, LOCK_UN) == -1 && errno == EINTR) {}
            ::close(fileDescriptor_);
        }
    }

    bool Acquire(
        const std::filesystem::path& lockPath,
        int operation,
        std::string& errorMessage
    ) {
        fileDescriptor_ = ::open(
            lockPath.c_str(),
            O_CREAT | O_RDWR | O_CLOEXEC,
            static_cast<mode_t>(0666)
        );
        if (fileDescriptor_ < 0) {
            errorMessage = ErrnoMessage(
                "Could not open JSON lock file '" + lockPath.string() + "'"
            );
            return false;
        }

        while (::flock(fileDescriptor_, operation) == -1) {
            if (errno == EINTR) continue;
            errorMessage = ErrnoMessage(
                "Could not lock JSON file using '" + lockPath.string() + "'"
            );
            ::close(fileDescriptor_);
            fileDescriptor_ = -1;
            return false;
        }
        return true;
    }

private:
    int fileDescriptor_{-1};
};

bool ReadUnlocked(
    const std::filesystem::path& path,
    nlohmann::json& root,
    bool& exists,
    std::string& errorMessage
) {
    std::error_code filesystemError;
    exists = std::filesystem::exists(path, filesystemError);
    if (filesystemError) {
        errorMessage = "Could not inspect JSON file '" + path.string() +
            "': " + filesystemError.message();
        return false;
    }

    if (!exists) {
        root = nlohmann::json::object();
        errorMessage.clear();
        return true;
    }

    std::ifstream input(path);
    if (!input) {
        errorMessage = "Could not open JSON file for reading: " + path.string();
        return false;
    }

    try {
        input >> root;
    } catch (const std::exception& exception) {
        errorMessage = "Could not parse JSON file '" + path.string() +
            "': " + exception.what();
        return false;
    }

    if (!root.is_object()) {
        errorMessage = "JSON root must be an object: " + path.string();
        return false;
    }

    errorMessage.clear();
    return true;
}

bool WriteAll(int fileDescriptor, const std::string& data, std::string& errorMessage) {
    std::size_t offset = 0;
    while (offset < data.size()) {
        const ssize_t written = ::write(
            fileDescriptor,
            data.data() + offset,
            data.size() - offset
        );
        if (written < 0) {
            if (errno == EINTR) continue;
            errorMessage = ErrnoMessage("Could not write temporary JSON file");
            return false;
        }
        if (written == 0) {
            errorMessage = "Could not write temporary JSON file: zero-byte write";
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

bool SyncParentDirectory(
    const std::filesystem::path& path,
    std::string& errorMessage
) {
    const auto parent = path.has_parent_path()
        ? path.parent_path()
        : std::filesystem::path{"."};

    const int directoryFd = ::open(
        parent.c_str(),
        O_RDONLY | O_DIRECTORY | O_CLOEXEC
    );
    if (directoryFd < 0) {
        errorMessage = ErrnoMessage(
            "Could not open JSON parent directory for fsync '" + parent.string() + "'"
        );
        return false;
    }

    const int syncResult = ::fsync(directoryFd);
    const int savedErrno = errno;
    ::close(directoryFd);
    if (syncResult != 0) {
        errno = savedErrno;
        errorMessage = ErrnoMessage(
            "Could not fsync JSON parent directory '" + parent.string() + "'"
        );
        return false;
    }
    return true;
}

bool AtomicWrite(
    const std::filesystem::path& path,
    const nlohmann::json& root,
    std::string& errorMessage
) {
    const std::filesystem::path temporary =
        path.string() + ".tmp." + std::to_string(static_cast<long long>(::getpid()));

    const int fileDescriptor = ::open(
        temporary.c_str(),
        O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
        static_cast<mode_t>(0666)
    );
    if (fileDescriptor < 0) {
        errorMessage = ErrnoMessage(
            "Could not open temporary JSON file '" + temporary.string() + "'"
        );
        return false;
    }

    const std::string serialized = root.dump(4) + '\n';
    bool success = WriteAll(fileDescriptor, serialized, errorMessage);

    if (success && ::fsync(fileDescriptor) != 0) {
        errorMessage = ErrnoMessage(
            "Could not fsync temporary JSON file '" + temporary.string() + "'"
        );
        success = false;
    }

    const int closeResult = ::close(fileDescriptor);
    if (success && closeResult != 0) {
        errorMessage = ErrnoMessage(
            "Could not close temporary JSON file '" + temporary.string() + "'"
        );
        success = false;
    }

    if (!success) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }

    if (::rename(temporary.c_str(), path.c_str()) != 0) {
        errorMessage = ErrnoMessage(
            "Could not atomically replace JSON file '" + path.string() + "'"
        );
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }

    if (!SyncParentDirectory(path, errorMessage)) return false;

    errorMessage.clear();
    return true;
}

} // namespace

std::filesystem::path JsonDataStore::LockPath(const std::filesystem::path& path) {
    return std::filesystem::path(path.string() + ".lock");
}

bool JsonDataStore::Read(
    const std::filesystem::path& path,
    Json& root,
    bool& exists,
    std::string& errorMessage
) {
    if (!EnsureParentDirectory(path, errorMessage)) return false;

    FileLock lock;
    if (!lock.Acquire(LockPath(path), LOCK_SH, errorMessage)) return false;

    return ReadUnlocked(path, root, exists, errorMessage);
}

bool JsonDataStore::Update(
    const std::filesystem::path& path,
    const Mutator& mutator,
    std::string& errorMessage
) {
    if (!EnsureParentDirectory(path, errorMessage)) return false;

    FileLock lock;
    if (!lock.Acquire(LockPath(path), LOCK_EX, errorMessage)) return false;

    Json root;
    bool exists = false;
    if (!ReadUnlocked(path, root, exists, errorMessage)) return false;
    (void)exists;

    std::string mutationError;
    if (!mutator(root, mutationError)) {
        errorMessage = mutationError.empty()
            ? "JSON update was rejected by the caller."
            : mutationError;
        return false;
    }

    if (!root.is_object()) {
        errorMessage = "JSON mutator produced a non-object root.";
        return false;
    }

    return AtomicWrite(path, root, errorMessage);
}

} // namespace SentinelShared
