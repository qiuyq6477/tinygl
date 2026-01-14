#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <framework/geometry.h>

namespace {

struct ColorShader : public tinygl::ShaderBuiltins {
    tinygl::Vec4 uColor;

    void vertex(const tinygl::Vec4* attribs, tinygl::ShaderContext& ctx) {
        gl_Position = attribs[0];
    }

    void fragment(const tinygl::ShaderContext& ctx) {
        gl_FragColor = uColor;
    }
};

class BlendScissorTest : public ITinyGLTestCase {
    GLuint vbo = 0;
    GLuint vao = 0;
    ColorShader shader;
    
    bool enableScissor = true;
    bool enableBlend = true;
    
    struct {
        float x = 100, y = 100, w = 600, h = 400;
    } scissor;

public:
    void init(tinygl::SoftRenderContext& ctx) override {
        // Two overlapping quads
        // Quad 1: Red, [-0.5, 0.5]
        // Quad 2: Green, [0.0, 1.0]
        float vertices[] = {
            // Quad 1
            -0.5f, -0.5f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f, 1.0f,
             0.5f,  0.5f, 0.0f, 1.0f,
            -0.5f,  0.5f, 0.0f, 1.0f,

            // Quad 2
             0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f, 0.0f, 1.0f,
             0.0f,  0.0f, 0.0f, 1.0f,
             1.0f,  1.0f, 0.0f, 1.0f,
             0.0f,  1.0f, 0.0f, 1.0f,
        };
        
        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        ctx.glGenVertexArrays(1, &vao);
        ctx.glBindVertexArray(vao);
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
        
        ctx.glDisable(GL_DEPTH_TEST);
    }

    void destroy(tinygl::SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteVertexArrays(1, &vao);
        ctx.glDisable(GL_SCISSOR_TEST);
        ctx.glDisable(GL_BLEND);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_checkbox(ctx, "Enable Scissor", (int*)&enableScissor);
        mu_label(ctx, "Scissor X"); mu_slider(ctx, &scissor.x, 0, 800);
        mu_label(ctx, "Scissor Y"); mu_slider(ctx, &scissor.y, 0, 600);
        mu_label(ctx, "Scissor W"); mu_slider(ctx, &scissor.w, 0, 800);
        mu_label(ctx, "Scissor H"); mu_slider(ctx, &scissor.h, 0, 600);
        
        mu_checkbox(ctx, "Enable Blend", (int*)&enableBlend);
    }

    void onRender(tinygl::SoftRenderContext& ctx) override {
        ctx.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        
        // glClear should respect scissor if enabled
        if (enableScissor) {
            ctx.glEnable(GL_SCISSOR_TEST);
            ctx.glScissor((int)scissor.x, (int)scissor.y, (int)scissor.w, (int)scissor.h);
        } else {
            ctx.glDisable(GL_SCISSOR_TEST);
        }
        
        ctx.glClear(GL_COLOR_BUFFER_BIT);

        if (enableBlend) {
            ctx.glEnable(GL_BLEND);
            ctx.glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        } else {
            ctx.glDisable(GL_BLEND);
        }

        ctx.glBindVertexArray(vao);

        // Draw background quad (opaque red)
        shader.uColor = {1.0f, 0.0f, 0.0f, 1.0f};
        ctx.glDrawArrays(shader, GL_TRIANGLES, 0, 6);

        // Draw overlapping quad (semi-transparent green)
        shader.uColor = {0.0f, 1.0f, 0.0f, 0.5f};
        ctx.glDrawArrays(shader, GL_TRIANGLES, 6, 6);
    }
};

static TestRegistrar registry("Basic", "Blend & Scissor", []() { return new BlendScissorTest(); });

}
