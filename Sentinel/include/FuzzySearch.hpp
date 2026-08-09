#pragma once

#include <optional>
#include <string_view>

class FuzzySearch {
public:
    static std::optional<int> Score(
        std::string_view query,
        std::string_view candidate
    );
};
