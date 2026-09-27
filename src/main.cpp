
#include <iostream>
#include <ncurses.h>

int main(int argc, char* argv[]) {
    initscr(); 
    printw("Sentinel"); 
    refresh(); 
    getch(); 
    endwin(); 

    return 0;
}