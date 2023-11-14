#pragma once

#include "stdafx.h"


struct Snow {
    static constexpr float DEFTFPS = 60;
    static constexpr float AUTOPPS = 0;
    static constexpr float BASEPPS = 12;
    static constexpr float FASTPPS = 240;
    static constexpr float SLOWPPS = 60;

    float size;
    float pixel_per_frame;
    float x;
    float y;
    float ground;
    UINT sleep_frames;

    constexpr float alpha(float height) const { //fade out when touch the ground before reaching the bottom of the window
        return (height > ground) ? max(1 - (y - ground) / (height - ground) * 2, 0) : 1;
    }
    void setPPFbyPPS(float pixel_per_sec = AUTOPPS, float fps = DEFTFPS, float dpi = USER_DEFAULT_SCREEN_DPI);
};

struct SnowList {
    static constexpr std::array<float, 5> SnowSizes = { 3, 5, 8, 12, 20 };
    static constexpr UINT FPS = 60;
    static constexpr UINT MAXSLEEPFRAMES = (FPS * 30);
    static constexpr UINT COUNTPERSIZE = 20;
    static constexpr UINT DEFXRESOLU = 1920;
    static constexpr UINT DEFYRESOLU = 1080;

    UINT xres;
    UINT yres;
    UINT ground;
    UINT dpi;
    std::vector<Snow> list;
    std::mt19937 urand;

    SnowList(UINT xres, UINT yres, UINT ground, UINT dpi, UINT seed = std::mt19937::default_seed);
    void refreshSnowState(Snow& snow);
    void refreshList();
    void nextFrame();
};
