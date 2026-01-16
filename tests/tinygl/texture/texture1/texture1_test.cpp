#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>
#include <framework/texture_manager.h>
#include <vector>
#include <cstdio>

using namespace tinygl;
using namespace framework;

struct Vertex {
    float pos[3];
    uint8_t color[3];
    float uv[2];
};

struct TextureShader : public ShaderBuiltins {
    TextureObject* texture1 = nullptr;
    TextureObject* texture2 = nullptr;
    Vec2 uvScale = {1.0f, 1.0f};
    Vec2 uvOffset = {0.0f, 0.0f};
    float opacity = 0.5f;

    // Vertex Shader
    // attribs[0]: Pos
    // attribs[1]: Color
    // attribs[2]: UV
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        ctx.varyings[0] = attribs[1]; // Color
        
        // Transform UV
        Vec4 originalUV = attribs[2];
        Vec4 transformedUV;
        transformedUV.x = originalUV.x * uvScale.x + uvOffset.x;
        transformedUV.y = originalUV.y * uvScale.y + uvOffset.y;
        
        ctx.varyings[1] = transformedUV;
        
        gl_Position = attribs[0];
    }

    // Fragment Shader
    void fragment(const ShaderContext& ctx) {
        Vec4 color = ctx.varyings[0];
        Vec4 uv = ctx.varyings[1];

        Vec4 texColor1 = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
        if (texture1) {
            texColor1 = texture1->sample(uv.x, uv.y);
        }

        Vec4 texColor2 = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
        if (texture2) {
            texColor2 = texture2->sample(uv.x, uv.y);
        }
        // 鍚堝苟棰滆壊
        gl_FragColor = mix(texColor1, texColor2, opacity);
    }
};

class Texture1Test : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // ... (Mesh Setup code remains same, skipping for brevity in replacement if possible, 
        // but since I'm replacing the class, I must include it)
        
        float vertices[] = {
            // positions          // colors           // texture coords
            -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,   0.0f, 1.0f,  // top left 
            0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,   1.0f, 1.0f, // top right
            0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,   1.0f, 0.0f, // bottom right
            -0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,   0.0f, 0.0f, // bottom left
        };
        uint32_t indices[] = {
            0, 2, 1,
            2, 0, 3
        };
        ctx.glGenVertexArrays(1, &vao);
        ctx.glBindVertexArray(vao);

        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        GLsizei stride = 8 * sizeof(float);
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
        ctx.glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));
        ctx.glEnableVertexAttribArray(2);

        // Load textures using Manager
        m_texRef1 = TextureManager::Load(ctx, "assets/container.jpg");
        tex1 = m_texRef1 ? m_texRef1->id : 0;

        m_texRef2 = TextureManager::Load(ctx, "assets/awesomeface.png");
        tex2 = m_texRef2 ? m_texRef2->id : 0;
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteBuffers(1, &ebo);
        ctx.glDeleteVertexArrays(1, &vao);
        // Textures managed by TextureManager/shared_ptr
        m_texRef1.reset();
        m_texRef2.reset();
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_label(ctx, "Texture Parameters");
        
        mu_label(ctx, "Wrap Mode:");
        if (mu_button(ctx, m_wrapMode == GL_REPEAT ? "[Repeat]" : "Repeat")) m_wrapMode = GL_REPEAT;
        if (mu_button(ctx, m_wrapMode == GL_MIRRORED_REPEAT ? "[Mirrored]" : "Mirrored")) m_wrapMode = GL_MIRRORED_REPEAT;
        if (mu_button(ctx, m_wrapMode == GL_CLAMP_TO_EDGE ? "[Clamp]" : "Clamp")) m_wrapMode = GL_CLAMP_TO_EDGE;

        mu_label(ctx, "Filter Mode:");
        if (mu_button(ctx, m_filterMode == GL_NEAREST ? "[Nearest]" : "Nearest")) m_filterMode = GL_NEAREST;
        if (mu_button(ctx, m_filterMode == GL_LINEAR ? "[Linear]" : "Linear")) m_filterMode = GL_LINEAR;

        mu_label(ctx, "UV Scale:");
        mu_slider(ctx, &m_uvScaleX, 0.1f, 5.0f);
        mu_slider(ctx, &m_uvScaleY, 0.1f, 5.0f);

        mu_label(ctx, "UV Offset:");
        mu_slider(ctx, &m_uvOffsetX, -2.0f, 2.0f);
        mu_slider(ctx, &m_uvOffsetY, -2.0f, 2.0f);

        mu_label(ctx, "Mix Opacity:");
        mu_slider(ctx, &m_opacity, 0.0f, 1.0f);
    }

    void onRender(SoftRenderContext& ctx) override {
        // Apply Texture Parameters
        // Set params for Texture Unit 0 (tex1)
        if (tex1) {
            ctx.glActiveTexture(GL_TEXTURE0);
            ctx.glBindTexture(GL_TEXTURE_2D, tex1);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapMode);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapMode);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filterMode);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filterMode);
        }

        // Set params for Texture Unit 1 (tex2)
        if (tex2) {
            ctx.glActiveTexture(GL_TEXTURE1);
            ctx.glBindTexture(GL_TEXTURE_2D, tex2);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapMode);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapMode);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filterMode);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filterMode);
        }

        shader.texture1 = ctx.getTextureObject(tex1);
        shader.texture2 = ctx.getTextureObject(tex2);
        shader.uvScale = {m_uvScaleX, m_uvScaleY};
        shader.uvOffset = {m_uvOffsetX, m_uvOffsetY};
        shader.opacity = m_opacity;

        ctx.glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
    }

private:
    GLuint vao = 0, vbo = 0, ebo = 0, tex1 = 0, tex2 = 0;
    std::shared_ptr<Texture> m_texRef1;
    std::shared_ptr<Texture> m_texRef2;
    
    // UI State
    GLint m_wrapMode = GL_REPEAT;
    GLint m_filterMode = GL_LINEAR;
    float m_uvScaleX = 1.0f;
    float m_uvScaleY = 1.0f;
    float m_uvOffsetX = 0.0f;
    float m_uvOffsetY = 0.0f;
    float m_opacity = 0.5f;

    TextureShader shader;
};

static TestRegistrar registrar("Texture", "Texture1", []() { return new Texture1Test(); });
