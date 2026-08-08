#pragma once

#include <optional>
#include <string_view>

class FuzzySearch {
public:
    // Returns no score when query characters cannot be matched in order.
    // Larger scores represent stronger matches.
    static std::optional<int> Score(
        std::string_view query,
        std::string_view candidate
    );
};
