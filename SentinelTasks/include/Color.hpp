#pragma once

struct RgbColor {
    int red{255};
    int green{255};
    int blue{255};
};

struct TreeDisplaySettings {
    RgbColor foreground{255, 255, 255};
    RgbColor background{0, 0, 0};
};
