#pragma once

struct RgbColor {
    int red{255};
    int green{255};
    int blue{255};
};

struct DisplaySettings {
    RgbColor foreground{255, 255, 255};
    RgbColor background{0, 0, 0};
    bool dirty{true};
};
