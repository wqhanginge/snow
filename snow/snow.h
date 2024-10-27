#pragma once

#include "stdafx.h"


struct Snow {
    static constexpr float DEFTFPS = 60;
    static constexpr float BASEPPS = 12;
    static constexpr float FASTPPS = 240;
    static constexpr float SLOWPPS = 60;
    static constexpr float FADELEN = 20;

    float size;
    float pixel_per_frame;
    float x;
    float y;
    float ground;
    UINT sleep_frames;

    constexpr float alpha() const { //fade out if touch the ground
        return 1 - clamp<float>(y - ground, 0, FADELEN) / FADELEN;
    }
    constexpr float pps(float dpi = USER_DEFAULT_SCREEN_DPI) const {    //pps by snow size
        return clamp<float>(BASEPPS * size * USER_DEFAULT_SCREEN_DPI / dpi, SLOWPPS, FASTPPS);
    }
    inline void step(float fps = DEFTFPS) { //move to next frame
        y += (y - ground >= 0 && y - ground < FADELEN) ? SLOWPPS / fps / 2 : pixel_per_frame;
    }
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
    void updateSnowGround(Snow& snow);
    void refreshList();
    void updateGround();
    void nextFrame();
};
