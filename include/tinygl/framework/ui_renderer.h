#pragma once
#include <SDL2/SDL.h>
#include <unordered_map>
#include <tinygl/core/tinygl.h>
#include <tinygl/third_party/microui.h>
namespace tinygl {

class UIRenderer {
public:
    static void init(mu_Context* ctx);
    static void processInput(mu_Context* ctx, const SDL_Event& event);
    static void render(mu_Context* ctx, SoftRenderContext& renderCtx);
private:
    static int textWidth(mu_Font font, const char* text, int len);
    static int textHeight(mu_Font font);
};

}
