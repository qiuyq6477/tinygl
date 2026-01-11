#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>

using namespace tinygl;
using namespace framework;

// Simple shader for texturing a quad
struct TextureFilterShader : public ShaderBuiltins {
    Mat4 mvp = Mat4::Identity();
    TextureObject* texture = nullptr;

    void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        // Pass UV to fragment shader
        outCtx.varyings[0] = attribs[1]; 
        // Position
        gl_Position = mvp * attribs[0];
    }

    void fragment(const ShaderContext& inCtx) {
        if (!texture) {
            gl_FragColor = {1, 0, 1, 1}; // Return magenta if texture is missing
        }
        Vec4 uv = inCtx.varyings[0];
        gl_FragColor = texture->sample(uv.x, uv.y, inCtx.rho);
    }
};

class TextureFilterTest : public ITestCase {
public:
    GLuint m_vbo = 0;
    GLuint m_vao = 0;
    GLuint m_textureId = 0;
    TextureFilterShader m_shader;
    
    // UI state
    int m_filterMode = 0; // 0: NEAREST, 1: LINEAR
    int m_wrapMode = 0;   // 0: REPEAT, 1: MIRRORED, 2: EDGE, 3: BORDER
    float m_borderColor[4] = {1.0f, 0.0f, 1.0f, 1.0f}; // Default Magenta

    const GLint filterOptions[2] = {GL_NEAREST, GL_LINEAR};
    const GLint wrapOptions[4] = {GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, GL_CLAMP_TO_BORDER};
    const char* wrapNames[4] = {"Repeat", "Mirrored", "Clamp Edge", "Clamp Border"};

    void init(SoftRenderContext& ctx) override {
        // A simple quad with extended UVs to demonstrate wrapping
        // UV range [-1.5, 2.5] (spanning 4x4 tiles approx)
        float vertices[] = {
            // positions      // texture coords
            -0.8f, -0.8f, 0.0f,  -1.5f, -1.5f,
             0.8f, -0.8f, 0.0f,   2.5f, -1.5f,
             0.8f,  0.8f, 0.0f,   2.5f,  2.5f,
            -0.8f,  0.8f, 0.0f,  -1.5f,  2.5f
        };

        ctx.glGenVertexArrays(1, &m_vao);
        ctx.glBindVertexArray(m_vao);

        ctx.glGenBuffers(1, &m_vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 5 * sizeof(float), (void*)0);
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);

        // --- Procedural 4x4 Texture (Checkboard with colors) ---
        const int W = 4, H = 4;
        unsigned char data[W * H * 4];
        for(int y=0; y<H; ++y) {
            for(int x=0; x<W; ++x) {
                int i = (y * W + x) * 4;
                bool isDark = (x + y) % 2 == 0;
                // Dark cells: Dark Grey, Light cells: Colored
                if (isDark) {
                    data[i] = 50; data[i+1] = 50; data[i+2] = 50; data[i+3] = 255;
                } else {
                    // Gradient colors
                    data[i]   = (unsigned char)(x * 255 / (W-1));
                    data[i+1] = (unsigned char)(y * 255 / (H-1));
                    data[i+2] = 128;
                    data[i+3] = 255;
                }
            }
        }

        ctx.glGenTextures(1, &m_textureId);
        ctx.glBindTexture(GL_TEXTURE_2D, m_textureId);
        ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteVertexArrays(1, &m_vao);
        ctx.glDeleteTextures(1, &m_textureId);
    }

    void onGui(mu_Context* mu_ctx, const Rect& rect) override {
        // --- Filter Mode ---
        mu_label(mu_ctx, "Filter Mode (Min/Mag):");
        mu_layout_row(mu_ctx, 2, (int[]){100, 100}, 0);
        if (mu_button(mu_ctx, m_filterMode == 0 ? "[Nearest]" : "Nearest")) m_filterMode = 0;
        if (mu_button(mu_ctx, m_filterMode == 1 ? "[Linear]" : "Linear")) m_filterMode = 1;

        // --- Wrap Mode ---
        mu_label(mu_ctx, "Wrap Mode (S/T):");
        mu_layout_row(mu_ctx, 2, (int[]){100, 100}, 0);
        for(int i=0; i<4; ++i) {
            char buf[32];
            if (m_wrapMode == i) snprintf(buf, sizeof(buf), "[%s]", wrapNames[i]);
            else snprintf(buf, sizeof(buf), "%s", wrapNames[i]);
            
            if (mu_button(mu_ctx, buf)) m_wrapMode = i;
        }
        
        // --- Border Color UI ---
        if (m_wrapMode == 3) { // Only show if Clamp To Border
            mu_layout_row(mu_ctx, 1, (int[]){-1}, 0);
            mu_label(mu_ctx, "Border Color (RGBA):");
            
            mu_layout_row(mu_ctx, 4, (int[]){50, 50, 50, 50}, 0);
            mu_slider(mu_ctx, &m_borderColor[0], 0.0f, 1.0f);
            mu_slider(mu_ctx, &m_borderColor[1], 0.0f, 1.0f);
            mu_slider(mu_ctx, &m_borderColor[2], 0.0f, 1.0f);
            mu_slider(mu_ctx, &m_borderColor[3], 0.0f, 1.0f);
            
            // Preview
            char buf[64];
            snprintf(buf, sizeof(buf), "R:%.1f G:%.1f B:%.1f A:%.1f", 
                m_borderColor[0], m_borderColor[1], m_borderColor[2], m_borderColor[3]);
            mu_layout_row(mu_ctx, 1, (int[]){-1}, 0);
            mu_label(mu_ctx, buf);
        }
        
        mu_layout_row(mu_ctx, 1, (int[]){-1}, 0);
        mu_label(mu_ctx, "Note: UV range is [-1.5, 2.5]");
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ctx.glActiveTexture(GL_TEXTURE0);
        ctx.glBindTexture(GL_TEXTURE_2D, m_textureId);
        
        // Apply Filter
        GLint filter = filterOptions[m_filterMode];
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

        // Apply Wrap
        GLint wrap = wrapOptions[m_wrapMode];
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
        
        // Apply Border Color
        if (m_wrapMode == 3) {
            ctx.glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, m_borderColor);
        }

        m_shader.texture = ctx.getTextureObject(m_textureId);

        // Simple scale to fit screen nicely
        m_shader.mvp = Mat4::Scale(0.8f, 0.8f, 1.0f);

        ctx.glDrawArrays(m_shader, GL_TRIANGLE_FAN, 0, 4);
    }
};

// Register the test
static TestRegistrar registrar("Texture", "Filter", []() { return new TextureFilterTest(); });