#include "EditorBuffer.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <sstream>

EditorBuffer::EditorBuffer() {
    EnsureNonEmpty();
}

bool EditorBuffer::Load(const std::filesystem::path& path, std::string& errorMessage) {
    std::error_code filesystemError;
    const bool exists = std::filesystem::exists(path, filesystemError);
    if (filesystemError) {
        errorMessage = "Could not inspect file '" + path.string() + "': " + filesystemError.message();
        return false;
    }

    if (!exists) {
        lines_.assign(1, std::string{});
        path_ = path;
        modified_ = false;
        errorMessage.clear();
        return true;
    }

    if (std::filesystem::is_directory(path, filesystemError)) {
        errorMessage = "Cannot edit a directory: " + path.string();
        return false;
    }
    if (filesystemError) {
        errorMessage = "Could not inspect file type '" + path.string() + "': " + filesystemError.message();
        return false;
    }

    std::ifstream input(path, std::ios::binary);
    if (!input) {
        errorMessage = "Could not open file: " + path.string();
        return false;
    }

    const std::string contents(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()
    );

    std::vector<std::string> loaded;
    std::size_t start = 0;
    while (start <= contents.size()) {
        const auto newline = contents.find('\n', start);
        if (newline == std::string::npos) {
            std::string line = contents.substr(start);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            loaded.push_back(std::move(line));
            break;
        }

        std::string line = contents.substr(start, newline - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        loaded.push_back(std::move(line));
        start = newline + 1;
        if (start == contents.size()) {
            loaded.emplace_back();
            break;
        }
    }

    if (loaded.empty()) loaded.emplace_back();
    lines_ = std::move(loaded);
    path_ = path;
    modified_ = false;
    errorMessage.clear();
    return true;
}

bool EditorBuffer::Save(std::string& errorMessage) {
    if (path_.empty()) {
        errorMessage = "No file name. Use :w <path>.";
        return false;
    }
    return SaveAs(path_, errorMessage);
}

bool EditorBuffer::SaveAs(const std::filesystem::path& path, std::string& errorMessage) {
    if (path.empty()) {
        errorMessage = "File path cannot be empty.";
        return false;
    }
    if (!WriteTo(path, errorMessage)) return false;
    path_ = path;
    modified_ = false;
    errorMessage.clear();
    return true;
}

void EditorBuffer::NewEmpty() {
    lines_.assign(1, std::string{});
    path_.clear();
    modified_ = false;
}

const std::filesystem::path& EditorBuffer::Path() const noexcept {
    return path_;
}

bool EditorBuffer::HasPath() const noexcept {
    return !path_.empty();
}

bool EditorBuffer::Modified() const noexcept {
    return modified_;
}

std::size_t EditorBuffer::LineCount() const noexcept {
    return lines_.size();
}

const std::string& EditorBuffer::Line(std::size_t row) const {
    return lines_.at(row);
}

std::string& EditorBuffer::Line(std::size_t row) {
    return lines_.at(row);
}

bool EditorBuffer::InsertCharacter(std::size_t row, std::size_t column, char value) {
    if (row >= lines_.size()) return false;
    auto& line = lines_[row];
    column = std::min(column, line.size());
    line.insert(line.begin() + static_cast<std::ptrdiff_t>(column), value);
    modified_ = true;
    return true;
}

bool EditorBuffer::EraseCharacter(std::size_t row, std::size_t column) {
    if (row >= lines_.size()) return false;
    auto& line = lines_[row];
    if (column >= line.size()) return false;
    line.erase(line.begin() + static_cast<std::ptrdiff_t>(column));
    modified_ = true;
    return true;
}

bool EditorBuffer::DeleteForward(std::size_t& row, std::size_t& column) {
    if (row >= lines_.size()) return false;
    auto& line = lines_[row];
    if (column < line.size()) {
        line.erase(line.begin() + static_cast<std::ptrdiff_t>(column));
        modified_ = true;
        return true;
    }

    if (row + 1 >= lines_.size()) return false;
    line += lines_[row + 1];
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(row + 1));
    modified_ = true;
    return true;
}

bool EditorBuffer::Backspace(std::size_t& row, std::size_t& column) {
    if (row >= lines_.size()) return false;
    auto& line = lines_[row];
    column = std::min(column, line.size());

    if (column > 0) {
        line.erase(line.begin() + static_cast<std::ptrdiff_t>(column - 1));
        --column;
        modified_ = true;
        return true;
    }

    if (row == 0) return false;

    const std::size_t previousLength = lines_[row - 1].size();
    lines_[row - 1] += line;
    lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(row));
    --row;
    column = previousLength;
    modified_ = true;
    return true;
}

bool EditorBuffer::SplitLine(std::size_t row, std::size_t column) {
    if (row >= lines_.size()) return false;
    auto& line = lines_[row];
    column = std::min(column, line.size());
    std::string tail = line.substr(column);
    line.resize(column);
    lines_.insert(
        lines_.begin() + static_cast<std::ptrdiff_t>(row + 1),
        std::move(tail)
    );
    modified_ = true;
    return true;
}

bool EditorBuffer::InsertBlankLineBefore(std::size_t row) {
    if (row > lines_.size()) return false;
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(row), std::string{});
    modified_ = true;
    return true;
}

bool EditorBuffer::InsertBlankLineAfter(std::size_t row) {
    if (row >= lines_.size()) return false;
    lines_.insert(lines_.begin() + static_cast<std::ptrdiff_t>(row + 1), std::string{});
    modified_ = true;
    return true;
}

bool EditorBuffer::DeleteLine(std::size_t row) {
    if (row >= lines_.size()) return false;
    if (lines_.size() == 1) {
        if (lines_.front().empty()) return false;
        lines_.front().clear();
    } else {
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(row));
    }
    EnsureNonEmpty();
    modified_ = true;
    return true;
}

bool EditorBuffer::WriteTo(const std::filesystem::path& path, std::string& errorMessage) const {
    const auto parent = path.has_parent_path() ? path.parent_path() : std::filesystem::path{"."};
    std::error_code filesystemError;
    if (!std::filesystem::exists(parent, filesystemError)) {
        errorMessage = "Parent directory does not exist: " + parent.string();
        return false;
    }
    if (filesystemError) {
        errorMessage = "Could not inspect parent directory: " + filesystemError.message();
        return false;
    }

    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        errorMessage = "Could not write file: " + path.string();
        return false;
    }

    for (std::size_t index = 0; index < lines_.size(); ++index) {
        output << lines_[index];
        if (index + 1 < lines_.size()) output.put('\n');
    }
    output.flush();
    if (!output) {
        errorMessage = "Failed while writing file: " + path.string();
        return false;
    }

    errorMessage.clear();
    return true;
}

void EditorBuffer::EnsureNonEmpty() {
    if (lines_.empty()) lines_.emplace_back();
}
