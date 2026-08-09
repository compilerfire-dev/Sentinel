#include <algorithm>
#include <ncurses.h>
#include <string>

namespace {

void DrawAt(WINDOW* window, int column, int width, const std::string& text) {
    if (!window || width <= 0 || column < 0) return;
    mvwaddnstr(window, 0, column, text.c_str(), width);
}

int InitializeLegend(WINDOW* window, int columns) {
    if (!window || columns <= 0) return ERR;

    werase(window);
    wattron(window, A_BOLD);

    const int nameColumn = std::clamp(columns / 4, 18, std::max(18, columns - 1));
    const std::string timerLegend = "ELAPSED / COMPLETED AT";
    const int timerColumn = std::max(nameColumn + 8, columns - static_cast<int>(timerLegend.size()) - 1);

    DrawAt(window, 0, std::max(0, nameColumn - 1), "ID / STATE  [ ] idle  [x] done  white row = running");
    DrawAt(window, nameColumn, std::max(0, timerColumn - nameColumn - 1), "TASK");
    DrawAt(window, timerColumn, std::max(0, columns - timerColumn - 1), timerLegend);

    wattroff(window, A_BOLD);
    wnoutrefresh(window);
    doupdate();
    return OK;
}

struct LegendRegistration {
    LegendRegistration() {
        // Positive one reserves a single physical line above stdscr.
        // It must be registered before initscr(), which this static object guarantees.
        ripoffline(1, InitializeLegend);
    }
};

LegendRegistration legendRegistration;

} // namespace
