#include "stdafx.h"
#include "snows.h"


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

void SnowList::updateSnowGround(Snow& snow) {
    if (snow.y < snow.ground) { //not fading yet
        snow.ground = std::max<float>((float)ground, snow.y);
    }
}

void SnowList::refreshList() {
    float scale = (float)dpi / USER_DEFAULT_SCREEN_DPI;
    size_t cnt = size_t(COUNTPERSIZE / scale * xres / DEFXRESOLU * yres / DEFYRESOLU);
    list.resize(cnt * SnowSizes.size());
    for (size_t i = 0; i < list.size(); i++) {
        auto& snow = list[i];
        snow.size = SnowSizes[i % SnowSizes.size()] * scale;
        if (!snow.pixel_per_frame || snow.sleep_frames) {   //init for a new Snow item or a waiting item
            snow.pixel_per_frame = snow.pps((float)dpi) / FPS;
            refreshSnowState(snow);
        }
    }
}

void SnowList::updateGround() {
    for (auto& snow : list) updateSnowGround(snow);
}

void SnowList::nextFrame() {
    for (auto& snow : list) {
        if (snow.sleep_frames) {    //skip some frames at the begining of each turn
            snow.sleep_frames--;
        }
        else if (snow.y >= yres) { //refreash when out of screen
            refreshSnowState(snow);
        }
        else {  //fall when inside screen
            snow.step((float)FPS);
        }
    }
}
