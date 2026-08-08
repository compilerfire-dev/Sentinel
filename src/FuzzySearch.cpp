#include "FuzzySearch.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

std::string Lower(std::string_view value) {
    std::string result(value);
    std::transform(
        result.begin(),
        result.end(),
        result.begin(),
        [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        }
    );
    return result;
}

bool IsBoundary(std::string_view value, std::size_t index) {
    if (index == 0) {
        return true;
    }

    const unsigned char previous = static_cast<unsigned char>(value[index - 1]);
    return std::isspace(previous) || previous == '-' || previous == '_' || previous == '/';
}

} // namespace

std::optional<int> FuzzySearch::Score(
    std::string_view query,
    std::string_view candidate
) {
    if (query.empty() || candidate.empty()) {
        return std::nullopt;
    }

    const std::string normalizedQuery = Lower(query);
    const std::string normalizedCandidate = Lower(candidate);

    if (normalizedQuery == normalizedCandidate) {
        return 2000;
    }

    int score = 0;

    if (normalizedCandidate.starts_with(normalizedQuery)) {
        score += 500;
    }

    std::size_t queryIndex = 0;
    std::size_t previousMatch = 0;
    bool hasPreviousMatch = false;
    int consecutiveMatches = 0;

    for (std::size_t candidateIndex = 0;
         candidateIndex < normalizedCandidate.size() && queryIndex < normalizedQuery.size();
         ++candidateIndex) {
        if (normalizedCandidate[candidateIndex] != normalizedQuery[queryIndex]) {
            continue;
        }

        score += 10;

        if (IsBoundary(normalizedCandidate, candidateIndex)) {
            score += 25;
        }

        if (!hasPreviousMatch) {
            // Earlier first matches are generally more relevant.
            score += std::max(0, 40 - static_cast<int>(candidateIndex));
            consecutiveMatches = 1;
        } else if (candidateIndex == previousMatch + 1) {
            ++consecutiveMatches;
            score += 15 + consecutiveMatches * 4;
        } else {
            consecutiveMatches = 1;
            const auto gap = candidateIndex - previousMatch - 1;
            score -= std::min(20, static_cast<int>(gap));
        }

        previousMatch = candidateIndex;
        hasPreviousMatch = true;
        ++queryIndex;
    }

    if (queryIndex != normalizedQuery.size()) {
        return std::nullopt;
    }

    // Prefer compact candidates when otherwise similarly matched.
    score -= std::min(
        40,
        static_cast<int>(normalizedCandidate.size() - normalizedQuery.size())
    );

    return score;
}
