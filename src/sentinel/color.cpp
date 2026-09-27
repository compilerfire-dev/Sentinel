
#include <sentinel/color.h>
#include <ncurses.h>

int sentinel_init_color(short color, short r, short g, short b) {
    if (!can_change_color()) {
        return ERR;
    }
    if (color < 0 || color >= COLORS) {
        return ERR;
    }
    r = (r < 0) ? 0 : (r > 1000 ? 1000 : r);
    g = (g < 0) ? 0 : (g > 1000 ? 1000 : g);
    b = (b < 0) ? 0 : (b > 1000 ? 1000 : b);

    return init_color(color, r, g, b);
}