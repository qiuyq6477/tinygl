#pragma once
#include <tinygl/core/tinygl.h>
#include <tinygl/framework/application.h>
#include <tinygl/framework/camera.h>
#include <tinygl/framework/geometry.h>
#include <stb_image.h>
#include <microui.h>
#include <SDL2/SDL.h>

struct Rect {
    int x, y, w, h;
};

class ITestCase {
public:
    virtual ~ITestCase() = default;

    // Called when the test case is selected
    // Use this to create resources (buffers, textures, shaders)
    virtual void init(tinygl::SoftRenderContext& ctx) = 0;

    // Called when the test case is deselected or app closes
    // Use this to free resources
    virtual void destroy(tinygl::SoftRenderContext& ctx) = 0;

    // Called every frame to render the test case specific UI
    // The UI should be constrained within the provided rect
    virtual void onGui(mu_Context* ctx, const Rect& rect) = 0;

    // Called every frame to render the 3D scene
    // The viewport is already set to the left side of the screen
    virtual void onRender(tinygl::SoftRenderContext& ctx) = 0;
    
    // Optional: Called every frame for logic updates
    virtual void onUpdate(float dt) {}
    
    // Optional: Called for SDL events
    virtual void onEvent(const SDL_Event& e) {}
};
