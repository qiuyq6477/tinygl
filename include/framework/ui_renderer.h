#pragma once
#include <SDL2/SDL.h>
#include <microui.h>
#include <rhi/device.h>
#include <rhi/encoder.h>

namespace framework {

class UIRenderer {
public:
    static void init(mu_Context* ctx, rhi::IGraphicsDevice* device);
    static void shutdown();
    static void processInput(mu_Context* ctx, const SDL_Event& event);
    static void render(mu_Context* ctx, rhi::CommandEncoder& encoder, int width, int height);
private:
    static int textWidth(mu_Font font, const char* text, int len);
    static int textHeight(mu_Font font);
};

}