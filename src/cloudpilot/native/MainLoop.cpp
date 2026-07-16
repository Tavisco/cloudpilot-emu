#include "MainLoop.h"

#include <fcntl.h>
#include <linux/input.h>
#include <unistd.h>

#include <cstring>
#include <vector>

#include "Debugger.h"
#include "EmHAL.h"
#include "EmSession.h"
#include "EmSystemState.h"
#include "Nibbler.h"
#include "PenEvent.h"  // Included for direct Session event queueing
#include "SuspendManager.h"

constexpr uint32 BACKGROUND_HUE = 0xd2;
constexpr uint32 FOREGROUND_COLOR = 0xff000000;
constexpr uint32 BACKGROUND_COLOR =
    0xff000000 | BACKGROUND_HUE | (BACKGROUND_HUE << 8) | (BACKGROUND_HUE << 16);

constexpr uint32 PALETTE_GRAYSCALE_16[] = {
    0xffd2d2d2, 0xffc4c4c4, 0xffb6b6b6, 0xffa8a8a8, 0xff9a9a9a, 0xff8c8c8c, 0xff7e7e7e, 0xff707070,
    0xff626262, 0xff545454, 0xff464646, 0xff383838, 0xff2a2a2a, 0xff1c1c1c, 0xff0e0e0e, 0xff000000};

constexpr long SCREEN_REFRESH_GRACE_TIME = 10;
constexpr int TOUCH_MOVE_THROTTLE = 25;

inline uint16_t RGB888toRGB565(uint32_t color) {
    uint32_t r = (color >> 16) & 0xFF;
    uint32_t g = (color >> 8) & 0xFF;
    uint32_t b = color & 0xFF;
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
}

static std::vector<uint8_t> g_backbuffer;
static bool g_backbuffer_initialized = false;

static int touch_fd = -1;
static int current_phys_x = 0;
static int current_phys_y = 0;
static bool is_pen_down = false;

static bool was_pen_down = false;
static int last_palm_x = -1;
static int last_palm_y = -1;
static long lastTouchMove = 0;

template <typename T>
inline void DrawScaledBlock(std::vector<uint8_t>& bg, uint32_t x, uint32_t y, int destScaleX,
                            int destScaleY, uint32_t line_length, uint32_t totalOffsetX,
                            uint32_t totalOffsetY, T color) {
    uint32_t startX = (x * destScaleX) + totalOffsetX;
    uint32_t startY = (y * destScaleY) + totalOffsetY;

    for (int dy = 0; dy < destScaleY; dy++) {
        // Calculate absolute memory location in the backbuffer
        T* row_ptr =
            reinterpret_cast<T*>(bg.data() + ((startY + dy) * line_length) + (startX * sizeof(T)));
        for (int dx = 0; dx < destScaleX; dx++) {
            row_ptr[dx] = color;
        }
    }
}

MainLoop::MainLoop(uint8_t* fbp, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo,
                   int scale)
    : fbp(fbp),
      vinfo(vinfo),
      finfo(finfo),
      scale(scale),
      screenDimensions(gSession->GetDevice().GetScreenDimensions()) {
    // Initialize the backbuffer ONCE
    if (!g_backbuffer_initialized) {
        size_t bufferSize = finfo.line_length * vinfo.yres;
        g_backbuffer.resize(bufferSize);

        // Clear the entire physical screen to the Palm background color
        if (vinfo.bits_per_pixel == 32) {
            uint32_t* ptr = (uint32_t*)g_backbuffer.data();
            for (size_t i = 0; i < bufferSize / 4; ++i) ptr[i] = BACKGROUND_COLOR;
        } else if (vinfo.bits_per_pixel == 16) {
            uint16_t* ptr = (uint16_t*)g_backbuffer.data();
            uint16_t bg16 = RGB888toRGB565(BACKGROUND_COLOR);
            for (size_t i = 0; i < bufferSize / 2; ++i) ptr[i] = bg16;
        }

        // Push clear state to the hardware framebuffer
        memcpy(fbp, g_backbuffer.data(), bufferSize);
        g_backbuffer_initialized = true;
    }

    InitTouch();
}

MainLoop::~MainLoop() {
    // Cleanup touch file descriptor
    if (touch_fd >= 0) {
        close(touch_fd);
        touch_fd = -1;
    }
}

void MainLoop::InitTouch() {
    const char* devices[] = {"/dev/input/event1", "/dev/input/event0"};

    for (const char* path : devices) {
        touch_fd = open(path, O_RDONLY | O_NONBLOCK);
        if (touch_fd >= 0) {
            printf("DEBUG: Successfully opened touch device: %s (fd: %d)\n", path, touch_fd);
            return;
        } else {
            printf("DEBUG: Failed to open %s: %s\n", path, strerror(errno));
        }
    }

    printf("DEBUG: CRITICAL - No input devices could be opened.\n");
}

void MainLoop::PollTouch() {
    if (touch_fd < 0) return;

    struct input_event ev;
    bool sync_needed = false;

    // Read all pending events
    while (read(touch_fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_ABS) {
            if (ev.code == 53) {
                current_phys_x = ev.value;
                sync_needed = true;
            } else if (ev.code == 54) {
                current_phys_y = ev.value;
                sync_needed = true;
            }
        } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
            is_pen_down = (ev.value != 0);
            sync_needed = true;
        } else if (ev.type == EV_SYN) {
            if (sync_needed) {
                ProcessPalmTouch(current_phys_x, current_phys_y, is_pen_down);
                sync_needed = false;
            }
        }
    }
}

void MainLoop::ProcessPalmTouch(int phys_x, int phys_y, bool pen_down) {
    uint32_t emuPixelWidth = 160 * scale;
    uint32_t emuPixelHeight = 160 * scale;

    uint32_t drawOffsetX =
        vinfo.xoffset + ((vinfo.xres > emuPixelWidth) ? (vinfo.xres - emuPixelWidth) / 2 : 0);
    uint32_t drawOffsetY =
        vinfo.yoffset + ((vinfo.yres > emuPixelHeight) ? (vinfo.yres - emuPixelHeight) / 2 : 0);

    int relative_x = phys_x - drawOffsetX;
    int relative_y = phys_y - drawOffsetY;

    if (relative_x < 0 || relative_x >= (int)emuPixelWidth || relative_y < 0 ||
        relative_y >= (int)emuPixelHeight) {
        if (was_pen_down && !pen_down) {
            gSession->QueuePenEvent(PenEvent::up());
            was_pen_down = false;
        }
        return;
    }

    int palm_x = relative_x / scale;
    int palm_y = relative_y / scale;

    long currentMillis = Platform::GetMilliseconds();

    printf("DEBUG: PALM (%d, %d)\n", palm_x, palm_y);

    if (pen_down) {
        if (!was_pen_down) {
            // HandlePenDown
            gSession->QueuePenEvent(PenEvent::down(palm_x, palm_y));
            was_pen_down = true;
            last_palm_x = palm_x;
            last_palm_y = palm_y;
            lastTouchMove = currentMillis;
        } else {
            // HandlePenMove
            if ((currentMillis - lastTouchMove > TOUCH_MOVE_THROTTLE) &&
                (palm_x != last_palm_x || palm_y != last_palm_y)) {
                gSession->QueuePenEvent(PenEvent::down(palm_x, palm_y));
                last_palm_x = palm_x;
                last_palm_y = palm_y;
                lastTouchMove = currentMillis;
            }
        }
    } else if (was_pen_down) {
        // HandlePenUp
        gSession->QueuePenEvent(PenEvent::up());
        was_pen_down = false;
    }
}

bool MainLoop::IsRunning() const { return true; }

void MainLoop::Cycle() {
    const long millis = Platform::GetMilliseconds();
    const uint32 clocksPerSecond = gSession->GetClocksPerSecond();

    PollTouch();

    if (!gDebugger.IsStopped()) {
        if (millis - millisOffset - static_cast<long>(clockEmu) > 500)
            clockEmu = millis - millisOffset - 10;

        const long cycles = static_cast<long>(
            (static_cast<double>(millis - millisOffset) - clockEmu) * clocksPerSecond / 1000.);

        if (cycles > 0) {
            long cyclesPassed = 0;

            while (cyclesPassed < cycles && !gDebugger.IsStopped())
                cyclesPassed += gSession->RunEmulation(cycles);
            clockEmu +=
                static_cast<double>(cyclesPassed) / (static_cast<double>(clocksPerSecond) / 1000.);
        }
    } else {
        clockEmu = millis - millisOffset;
    }

    if (gSystemState.IsScreenDirty()) {
        UpdateScreen(false);
        gSystemState.MarkScreenClean();
    } else if (!SuspendManager::IsSuspended() && !gDebugger.IsStopped() &&
               !gDebugger.IsStepping()) {
        usleep(16000);
    }
}

void MainLoop::UpdateScreen(bool fullRedraw) {
    if (gSession->IsPowerOn() && EmHAL::CopyLCDFrame(frame, fullRedraw)) {
        uint8* buffer = frame.GetBuffer();

        int destScaleX = frame.scaleX * scale;
        int destScaleY = frame.scaleY * scale;

        uint32_t emuPixelWidth = frame.lineWidth * destScaleX;
        uint32_t emuPixelHeight = (frame.lastDirtyLine - frame.firstDirtyLine + 1) * destScaleY;

        uint32_t drawOffsetX =
            vinfo.xoffset + ((vinfo.xres > emuPixelWidth) ? (vinfo.xres - emuPixelWidth) / 2 : 0);
        uint32_t drawOffsetY =
            vinfo.yoffset + ((vinfo.yres > emuPixelHeight) ? (vinfo.yres - emuPixelHeight) / 2 : 0);

        switch (frame.bpp) {
            case 1: {
                Nibbler<1> nibbler;
                for (uint32 y = frame.firstDirtyLine; y <= frame.lastDirtyLine; y++) {
                    nibbler.reset(buffer + y * frame.bytesPerLine, frame.margin);
                    for (uint32 x = 0; x < frame.lineWidth; x++) {
                        uint32 color32 =
                            nibbler.nibble() == 0 ? BACKGROUND_COLOR : FOREGROUND_COLOR;

                        if (vinfo.bits_per_pixel == 32) {
                            DrawScaledBlock<uint32_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      color32);
                        } else {
                            DrawScaledBlock<uint16_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      RGB888toRGB565(color32));
                        }
                    }
                }
            } break;

            case 2: {
                uint16 mapping = EmHAL::GetLCD2bitMapping();
                uint32 palette[4] = {PALETTE_GRAYSCALE_16[mapping & 0x000f],
                                     PALETTE_GRAYSCALE_16[(mapping >> 4) & 0x000f],
                                     PALETTE_GRAYSCALE_16[(mapping >> 8) & 0x000f],
                                     PALETTE_GRAYSCALE_16[(mapping >> 12) & 0x000f]};

                Nibbler<2> nibbler;
                for (uint32 y = frame.firstDirtyLine; y <= frame.lastDirtyLine; y++) {
                    nibbler.reset(buffer + y * frame.bytesPerLine, frame.margin);
                    for (uint32 x = 0; x < frame.lineWidth; x++) {
                        uint32 color32 = palette[nibbler.nibble()];

                        if (vinfo.bits_per_pixel == 32) {
                            DrawScaledBlock<uint32_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      color32);
                        } else {
                            DrawScaledBlock<uint16_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      RGB888toRGB565(color32));
                        }
                    }
                }
            } break;

            case 4: {
                Nibbler<4> nibbler;
                for (uint32 y = frame.firstDirtyLine; y <= frame.lastDirtyLine; y++) {
                    nibbler.reset(buffer + y * frame.bytesPerLine, frame.margin);
                    for (uint32 x = 0; x < frame.lineWidth; x++) {
                        uint32 color32 = PALETTE_GRAYSCALE_16[nibbler.nibble()];

                        if (vinfo.bits_per_pixel == 32) {
                            DrawScaledBlock<uint32_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      color32);
                        } else {
                            DrawScaledBlock<uint16_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      RGB888toRGB565(color32));
                        }
                    }
                }
            } break;

            case 24: {
                for (uint32 y = frame.firstDirtyLine; y <= frame.lastDirtyLine; y++) {
                    uint32_t* lineStart =
                        (uint32_t*)(buffer + y * frame.bytesPerLine + 4 * frame.margin);
                    for (uint32 x = 0; x < frame.lineWidth; x++) {
                        uint32 color32 = lineStart[x];

                        if (vinfo.bits_per_pixel == 32) {
                            DrawScaledBlock<uint32_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      color32);
                        } else {
                            DrawScaledBlock<uint16_t>(g_backbuffer, x, y, destScaleX, destScaleY,
                                                      finfo.line_length, drawOffsetX, drawOffsetY,
                                                      RGB888toRGB565(color32));
                        }
                    }
                }
            } break;
        }

        memcpy(fbp, g_backbuffer.data(), g_backbuffer.size());
    }

    const long timestamp = Platform::GetMilliseconds();
    long elapsed = timestamp - lastScreenRefreshAt;

    if (elapsed < SCREEN_REFRESH_GRACE_TIME && elapsed >= 0) {
        long sleepTimeMs = SCREEN_REFRESH_GRACE_TIME - elapsed;
        usleep(sleepTimeMs * 1000);
    }

    lastScreenRefreshAt = Platform::GetMilliseconds();
}
