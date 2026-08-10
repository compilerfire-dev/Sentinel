#include "FuzzySearch.hpp"

#include <algorithm>
#include <cctype>
#include <limits>

namespace {
char Lower(char c) {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}
}

int FuzzySearch::Score(const std::string& query, const std::string& candidate) {
    if (query.empty()) return 0;

    std::size_t qi = 0;
    int score = 0;
    int consecutive = 0;

    for (std::size_t ci = 0; ci < candidate.size() && qi < query.size(); ++ci) {
        if (Lower(candidate[ci]) != Lower(query[qi])) {
            consecutive = 0;
            continue;
        }

        score += 10;
        if (ci == 0 || candidate[ci - 1] == ' ' || candidate[ci - 1] == ':' ||
            candidate[ci - 1] == '/' || candidate[ci - 1] == '-') {
            score += 12;
        }
        if (consecutive > 0) score += 7 + consecutive;
        if (candidate[ci] == query[qi]) score += 1;

        ++consecutive;
        ++qi;
    }

    if (qi != query.size()) return std::numeric_limits<int>::min();
    score -= static_cast<int>(candidate.size() - query.size());
    return score;
}

std::vector<FuzzySearchResult> FuzzySearch::Rank(
    const std::string& query,
    const std::vector<std::string>& candidates
) {
    std::vector<FuzzySearchResult> results;
    results.reserve(candidates.size());

    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const int score = Score(query, candidates[index]);
        if (score != std::numeric_limits<int>::min()) {
            results.push_back({index, score});
        }
    }

    std::stable_sort(results.begin(), results.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.score > rhs.score;
    });
    return results;
}
