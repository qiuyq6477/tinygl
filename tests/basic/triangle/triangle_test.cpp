#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>

using namespace tinygl;

struct TriangleShader : public ShaderBase {
    float scale = 1.0f;
    
    Vec4 vertex(const Vec4* attribs, ShaderContext& outCtx) {
        // Pass color to fragment shader
        outCtx.varyings[0] = attribs[1];
        
        // Position
        Vec4 pos = attribs[0];
        pos.x *= scale;
        pos.y *= scale;
        pos.w = 1.0f;
        return pos;
    }

    Vec4 fragment(const ShaderContext& inCtx) {
        return inCtx.varyings[0]; // Interpolated color
    }
};

class TriangleTest : public ITestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // Define vertices for a simple triangle
        // Position (3) + Color (3)
        float vertices[] = {
             0.0f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f, // Top Red
            -0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f, // Bottom Left Green
             0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f  // Bottom Right Blue
        };

        // Create VAO
        ctx.glGenVertexArrays(1, &m_vao);
        ctx.glBindVertexArray(m_vao);

        // Create VBO
        ctx.glGenBuffers(1, &m_vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Setup Attributes
        // Pos
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 6 * sizeof(float), (void*)0);
        ctx.glEnableVertexAttribArray(0);
        // Color
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, 6 * sizeof(float), (void*)(3 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteVertexArrays(1, &m_vao);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        // Add specific controls for this test
        mu_label(ctx, "Triangle Properties");
        mu_layout_row(ctx, 2, (int[]) { 54, -1 }, 0);
        mu_label(ctx, "Scale");
        mu_slider(ctx, &m_scale, 0.1f, 2.0f);
    }

    void onRender(SoftRenderContext& ctx) override {
        shader.scale = m_scale;
        ctx.glDrawArrays(shader, GL_TRIANGLES, 0, 3);
    }

private:
    GLuint m_vbo = 0;
    GLuint m_vao = 0;
    float m_scale = 1.0f;
    TriangleShader shader;
};

// Register the test
static TestRegistrar registrar("Basic", "Triangle", []() { return new TriangleTest(); });
