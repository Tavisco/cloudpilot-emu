#ifndef _SDL_MAIN_LOOP_H_
#define _SDL_MAIN_LOOP_H_

#include <SDL2/SDL.h>

#include "ButtonEvent.h"
#include "EventHandler.h"
#include "Frame.h"
#include "Platform.h"

class MainLoop {
   public:
    MainLoop(SDL_Window* window, SDL_Renderer* renderer);

    bool IsRunning() const;

    void Cycle();

    static void CycleStatic(MainLoop* self);

   private:
    void LoadSilkscreen();

    void DrawSilkscreen(SDL_Renderer* renderer);

    void UpdateScreen();

   private:
    SDL_Renderer* renderer{nullptr};
    SDL_Texture* lcdTexture{nullptr};
    SDL_Texture* silkscreenTexture{nullptr};

    Frame frame{1024 * 128};

    const long millisOffset{Platform::GetMilliseconds()};
    double clockEmu{0};

    EventHandler eventHandler;
};

#endif  // _SDL_MAIN_LOOP_H_
