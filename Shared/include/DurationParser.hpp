#pragma once

#include <chrono>
#include <cctype>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>

namespace SentinelShared {

inline std::optional<std::chrono::seconds> ParseDuration(std::string_view text) {
    using Rep = std::chrono::seconds::rep;

    Rep total = 0;
    std::size_t position = 0;
    bool foundAny = false;

    while (position < text.size()) {
        while (position < text.size() &&
               std::isspace(static_cast<unsigned char>(text[position]))) {
            ++position;
        }
        if (position >= text.size()) break;

        if (!std::isdigit(static_cast<unsigned char>(text[position]))) {
            return std::nullopt;
        }

        Rep value = 0;
        while (position < text.size() &&
               std::isdigit(static_cast<unsigned char>(text[position]))) {
            const int digit = text[position] - '0';
            if (value > (std::numeric_limits<Rep>::max() - digit) / 10) {
                return std::nullopt;
            }
            value = value * 10 + digit;
            ++position;
        }

        while (position < text.size() &&
               std::isspace(static_cast<unsigned char>(text[position]))) {
            ++position;
        }
        if (position >= text.size()) return std::nullopt;

        const char unit = static_cast<char>(
            std::tolower(static_cast<unsigned char>(text[position]))
        );
        ++position;

        Rep multiplier = 0;
        if (unit == 's') multiplier = 1;
        else if (unit == 'm') multiplier = 60;
        else if (unit == 'h') multiplier = 3600;
        else return std::nullopt;

        if (value > std::numeric_limits<Rep>::max() / multiplier) {
            return std::nullopt;
        }
        const Rep contribution = value * multiplier;
        if (total > std::numeric_limits<Rep>::max() - contribution) {
            return std::nullopt;
        }
        total += contribution;
        foundAny = true;

        if (position < text.size() &&
            !std::isspace(static_cast<unsigned char>(text[position])) &&
            !std::isdigit(static_cast<unsigned char>(text[position]))) {
            return std::nullopt;
        }
    }

    if (!foundAny || total <= 0) return std::nullopt;
    return std::chrono::seconds(total);
}

inline std::string FormatDuration(std::chrono::seconds duration) {
    auto total = duration.count();
    if (total <= 0) return "0s";

    const auto hours = total / 3600;
    total %= 3600;
    const auto minutes = total / 60;
    const auto seconds = total % 60;

    std::ostringstream output;
    bool first = true;
    const auto append = [&](auto value, char unit) mutable {
        if (value <= 0) return;
        if (!first) output << ' ';
        output << value << unit;
        first = false;
    };

    append(hours, 'h');
    append(minutes, 'm');
    append(seconds, 's');
    return output.str();
}

} // namespace SentinelShared
