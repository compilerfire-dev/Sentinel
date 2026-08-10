#include "FuzzySearch.hpp"

#include <iostream>
#include <string>
#include <vector>

int main() {
    const std::vector<std::string> commands{
        "File: Save As Save to a different path",
        "Settings: Open Settings Configure SentinelEditor",
        "View: Toggle Line Numbers Show or hide the line-number gutter",
        "File: Open Open a file from disk"
    };

    const auto save = FuzzySearch::Rank("sa", commands);
    if (save.empty() || save.front().index != 0) {
        std::cerr << "save-as fuzzy query did not rank Save As first\n";
        return 1;
    }

    const auto settings = FuzzySearch::Rank("stng", commands);
    if (settings.empty() || settings.front().index != 1) {
        std::cerr << "settings subsequence query did not find Settings\n";
        return 1;
    }

    const auto missing = FuzzySearch::Rank("zzzz", commands);
    if (!missing.empty()) {
        std::cerr << "non-matching query unexpectedly returned results\n";
        return 1;
    }

    return 0;
}
