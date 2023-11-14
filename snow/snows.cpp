#include "stdafx.h"
#include "snows.h"


void Snow::setPPFbyPPS(float pixel_per_sec /*AUTOPPS*/, float fps /*DEFTFPS*/, float dpi /*USER_DEFAULT_SCREEN_DPI*/) {
    float def_pps = CLAMP(BASEPPS * size * USER_DEFAULT_SCREEN_DPI / dpi, SLOWPPS, FASTPPS);
    pixel_per_frame = (pixel_per_sec) ? pixel_per_sec / fps : def_pps / fps;
}


SnowList::SnowList(UINT xres, UINT yres, UINT ground, UINT dpi, UINT seed /*std::mt19937::default_seed*/)
    :xres(xres), yres(yres), ground(ground), dpi(dpi), list(), urand(seed)
{
    refreshList();
}

void SnowList::refreshSnowState(Snow& snow) {
    snow.x = float(urand() % UINT(xres - snow.size));
    snow.y = -snow.size;
    snow.ground = (float)ground;
    snow.sleep_frames = urand() % (MAXSLEEPFRAMES * yres / DEFYRESOLU);
}

void SnowList::refreshList() {
    float scale = (float)dpi / USER_DEFAULT_SCREEN_DPI;
    size_t cnt = size_t(COUNTPERSIZE / scale * xres / DEFXRESOLU * yres / DEFYRESOLU);
    list.resize(cnt * SnowSizes.size());
    for (size_t i = 0; i < cnt * SnowSizes.size(); i++) {
        auto& item = list[i];
        bool empty = !item.size;
        item.size = SnowSizes[i % SnowSizes.size()] * scale;
        if (empty || item.sleep_frames) {   //init for a new Snow item or a waiting item
            item.setPPFbyPPS(Snow::AUTOPPS, (float)FPS, (float)dpi);
            refreshSnowState(item);
        }
    }
}

void SnowList::nextFrame() {
    for (auto& item : list) {
        if (item.sleep_frames) {    //skip some frames at the begining of each turn
            item.sleep_frames--;
        }
        else if (item.y >= yres) { //refreash when out of screen
            refreshSnowState(item);
        }
        else {  //fall when inside screen
            if (item.ground != ground && item.y + item.size < min(item.ground, ground)) //update ground and keep the animation smooth
                item.ground = (float)ground;
            item.y += (item.y < item.ground) ? item.pixel_per_frame : Snow::SLOWPPS / FPS / 2;
        }
    }
}
