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

    const std::string treeLegend = "TREE: [F] folder  [T] idle  [>] running  [x] completed";
    const std::string timerLegend = "Timer = elapsed HH:MM:SS | Completed = completion timestamp";

    const int split = std::clamp(columns * 64 / 100, 40, std::max(40, columns - 30));
    DrawAt(window, 0, std::max(0, split - 2), treeLegend);
    DrawAt(window, split + 1, std::max(0, columns - split - 2), timerLegend);

    wattroff(window, A_BOLD);
    wnoutrefresh(window);
    doupdate();
    return OK;
}

struct LegendRegistration {
    LegendRegistration() {
        // Reserve a physical legend line above stdscr without changing any of
        // SentinelTasks' existing tree, description-pane, mouse, or dialog rows.
        ripoffline(1, InitializeLegend);
    }
};

LegendRegistration legendRegistration;

} // namespace
