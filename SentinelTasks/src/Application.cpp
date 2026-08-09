#include "Application.hpp"

#include <ncurses.h>

int Application::Run() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    bool running = true;
    while (running) {
        erase();

        int height = 0;
        int width = 0;
        getmaxyx(stdscr, height, width);
        (void)height;

        const char* title = "SentinelTasks";
        const char* message = "Independent task terminal application. Press q to quit.";

        mvaddnstr(0, 0, title, width > 0 ? width - 1 : 0);
        if (width > 1) {
            mvhline(1, 0, ACS_HLINE, width - 1);
        }
        mvaddnstr(3, 0, message, width > 0 ? width - 1 : 0);

        refresh();

        const int key = getch();
        if (key == 'q' || key == 'Q' || key == 27) {
            running = false;
        }
    }

    endwin();
    return 0;
}
