#pragma once
#include "TaskManager.hpp"
#include <optional>
#include <string>
#include <vector>
class CommandProcessor { public: explicit CommandProcessor(TaskManager& manager); void Execute(const std::string& line); bool ShouldQuit() const noexcept; const std::string& GetStatusMessage() const noexcept; const std::optional<std::vector<std::size_t>>& GetSearchResults() const noexcept; private: std::optional<std::size_t> ParseIndex(const std::string& value); TaskManager& manager_; bool quit_{false}; std::string status_; std::optional<std::vector<std::size_t>> searchResults_; };
