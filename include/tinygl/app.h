#pragma once

#include <SDL2/SDL.h>

// Define OLIVECDEF as extern so that:
// 1. In demos, we get external function declarations (linked from library).
// 2. In the library (vc.cpp), we get external function definitions (exported symbols).
#define OLIVECDEF extern
#include <tinygl/olive.h>

// Functions that the application (demo) must implement
extern "C" {
    void vc_init(void);
    void vc_input(SDL_Event *event);
    Olivec_Canvas vc_render(float dt, void* pixels);
}
