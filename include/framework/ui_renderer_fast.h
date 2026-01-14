#pragma once
#include <SDL2/SDL.h>
#include <unordered_map>
#include <tinygl/tinygl.h>
#include <microui.h>
#include <tinygl/core/gl_defs.h>

namespace framework {

class TINYGL_API UIRendererFast {
public:
    static void init(mu_Context* ctx);
    static void processInput(mu_Context* ctx, const SDL_Event& event);
    static void render(mu_Context* ctx, SoftRenderContext& renderCtx);
private:
    static int textWidth(mu_Font font, const char* text, int len);
    static int textHeight(mu_Font font);
};

}
