#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

class EditorBuffer {
public:
    EditorBuffer();

    bool Load(const std::filesystem::path& path, std::string& errorMessage);
    bool Save(std::string& errorMessage);
    bool SaveAs(const std::filesystem::path& path, std::string& errorMessage);
    void NewEmpty();

    const std::filesystem::path& Path() const noexcept;
    bool HasPath() const noexcept;
    bool Modified() const noexcept;

    std::size_t LineCount() const noexcept;
    const std::string& Line(std::size_t row) const;
    std::string& Line(std::size_t row);

    bool InsertCharacter(std::size_t row, std::size_t column, char value);
    bool EraseCharacter(std::size_t row, std::size_t column);
    bool DeleteForward(std::size_t& row, std::size_t& column);
    bool Backspace(std::size_t& row, std::size_t& column);
    bool SplitLine(std::size_t row, std::size_t column);
    bool InsertBlankLineBefore(std::size_t row);
    bool InsertBlankLineAfter(std::size_t row);
    bool DeleteLine(std::size_t row);

private:
    bool WriteTo(const std::filesystem::path& path, std::string& errorMessage) const;
    void EnsureNonEmpty();

    std::vector<std::string> lines_;
    std::filesystem::path path_;
    bool modified_{false};
};
