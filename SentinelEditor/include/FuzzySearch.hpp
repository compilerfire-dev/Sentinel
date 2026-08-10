#pragma once

#include <string>
#include <vector>

struct FuzzySearchResult {
    std::size_t index{0};
    int score{0};
};

class FuzzySearch {
public:
    static int Score(const std::string& query, const std::string& candidate);
    static std::vector<FuzzySearchResult> Rank(
        const std::string& query,
        const std::vector<std::string>& candidates
    );
};
