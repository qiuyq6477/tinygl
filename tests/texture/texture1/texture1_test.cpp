#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <vector>
#include <cstdio>

using namespace tinygl;

struct Vertex {
    float pos[3];
    uint8_t color[3];
    float uv[2];
};

struct TextureShader {
    TextureObject* texture1 = nullptr;
    TextureObject* texture2 = nullptr;
    Vec2 uvScale = {1.0f, 1.0f};
    Vec2 uvOffset = {0.0f, 0.0f};
    float opacity = 0.5f;

    // Vertex Shader
    // attribs[0]: Pos
    // attribs[1]: Color
    // attribs[2]: UV
    Vec4 vertex(const Vec4* attribs, ShaderContext& ctx) {
        ctx.varyings[0] = attribs[1]; // Color
        
        // Transform UV
        Vec4 originalUV = attribs[2];
        Vec4 transformedUV;
        transformedUV.x = originalUV.x * uvScale.x + uvOffset.x;
        transformedUV.y = originalUV.y * uvScale.y + uvOffset.y;
        
        ctx.varyings[1] = transformedUV;
        
        return attribs[0];
    }

    // Fragment Shader
    Vec4 fragment(const ShaderContext& ctx) {
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
        // 合并颜色
        return mix(texColor1, texColor2, opacity);
    }
};

class Texture1Test : public ITestCase {
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

        loadTexture(ctx, "texture/texture1/assets/container.jpg" , GL_TEXTURE0, tex1);
        loadTexture(ctx, "texture/texture1/assets/awesomeface.png" , GL_TEXTURE1, tex2);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteBuffers(1, &ebo);
        ctx.glDeleteTextures(1, &tex1);
        ctx.glDeleteTextures(1, &tex2);
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
        for (GLuint t : {tex1, tex2}) {
            ctx.glActiveTexture(GL_TEXTURE0); // Temporarily assume texture unit doesn't matter for bind only, but param needs bind
            // Actually implementation of glActiveTexture sets unit, glBindTexture binds to that unit.
            // But glTexParameteri operates on ACTIVE unit.
            
            // To set params for tex1, we must bind it.
            // But tex1 might be intended for Unit 0, tex2 for Unit 1.
            // Let's preserve units.
        }
        
        // Set params for Texture Unit 0 (tex1)
        ctx.glActiveTexture(GL_TEXTURE0);
        ctx.glBindTexture(GL_TEXTURE_2D, tex1);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapMode);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapMode);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filterMode);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filterMode);

        // Set params for Texture Unit 1 (tex2)
        ctx.glActiveTexture(GL_TEXTURE1);
        ctx.glBindTexture(GL_TEXTURE_2D, tex2);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, m_wrapMode);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, m_wrapMode);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, m_filterMode);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, m_filterMode);

        shader.texture1 = ctx.getTextureObject(tex1);
        shader.texture2 = ctx.getTextureObject(tex2);
        shader.uvScale = {m_uvScaleX, m_uvScaleY};
        shader.uvOffset = {m_uvOffsetX, m_uvOffsetY};
        shader.opacity = m_opacity;

        ctx.glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
    }
    // 加载纹理的辅助函数
    bool loadTexture(SoftRenderContext& ctx, const char* filename, GLenum textureUnit, GLuint& tex) {
        int width, height, nrChannels;
        
        // 翻转 Y 轴 (因为 OpenGL 纹理坐标 Y 轴向上，而图片通常是从上往下)
        // stb_image 默认左上角为 (0,0)，我们需要让它符合 GL 习惯
        // 或者我们在 Shader 里用 1.0 - v 也可以。这里用 stbi 的功能。
        stbi_set_flip_vertically_on_load(true); // 取决于你的 UV 映射习惯，这里暂时注释，按默认走

        // 强制加载为 4 通道 (RGBA)，简化处理
        unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 4);
        
        if (data) {
            ctx.glGenTextures(1, &tex);
            ctx.glActiveTexture(textureUnit);
            ctx.glBindTexture(GL_TEXTURE_2D, tex);

            // 设置纹理参数
            // 1. 重复模式 (Repeat)
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            
            // 2. 过滤模式 (Linear = 平滑, Nearest = 像素风)
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

            // 上传数据
            // InternalFormat = GL_RGBA
            // Type = GL_UNSIGNED_BYTE (stbi 读出来的是 char)
            ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
            
            stbi_image_free(data);
            printf("Texture loaded: %s (%dx%d)\n", filename, width, height);
            return true;
        } else {
            printf("Failed to load texture: %s\n", filename);
            // 生成一个默认的粉色纹理作为 Fallback
            uint32_t pinkPixel = 0xFFFF00FF; // ABGR
            ctx.glGenTextures(1, &tex);
            ctx.glBindTexture(GL_TEXTURE_2D, tex);
            ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pinkPixel);
            return false;
        }
    }

private:
    GLuint vbo = 0, ebo = 0, tex1 = 0, tex2 = 0;
    
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
