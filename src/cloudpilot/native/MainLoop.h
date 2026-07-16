#ifndef _MAIN_LOOP_H_
#define _MAIN_LOOP_H_

#include <linux/fb.h>

#include <cstdint>

#include "ButtonEvent.h"
#include "Frame.h"
#include "Platform.h"
#include "ScreenDimensions.h"

class MainLoop {
   public:
    MainLoop(uint8_t* fbp, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo,
             int scale);
    ~MainLoop();

    bool IsRunning() const;

    void Cycle();

   private:
    void UpdateScreen(bool fullRedraw);
    void InitTouch();
    void PollTouch();
    void ProcessPalmTouch(int phys_x, int phys_y, bool pen_down);

   private:
    uint8_t* fbp;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;

    int scale{1};
    ScreenDimensions screenDimensions;
    Frame frame{320 * 480 * 4};

    const long millisOffset{Platform::GetMilliseconds()};
    double clockEmu{0};

    long lastScreenRefreshAt = 0;
};

#endif  // _MAIN_LOOP_H_
