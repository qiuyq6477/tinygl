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

struct TextureShader : ShaderBase {
    TextureObject* texture1 = nullptr;
    TextureObject* texture2 = nullptr;
    // Vertex Shader
    // attribs[0]: Pos
    // attribs[1]: UV
    Vec4 vertex(const Vec4* attribs, ShaderContext& ctx) {
        // 传递 UV 给 Fragment Shader
        ctx.varyings[0] = attribs[1];
        ctx.varyings[1] = attribs[2];
        // 直接返回 Clip Space 坐标
        return attribs[0];
    }

    // Fragment Shader
    Vec4 fragment(const ShaderContext& ctx) {
        Vec4 color = ctx.varyings[0];
        Vec4 uv = ctx.varyings[1];

        Vec4 texColor1 = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
        if (texture1) {
            texColor1 = texture1->sampleNearestFast(uv.x, uv.y);
        }

        Vec4 texColor2 = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
        if (texture2) {
            texColor2 = texture2->sampleNearestFast(uv.x, uv.y);
        }
        // 合并颜色
        return mix(texColor1, texColor2, 0.5);
    }
};

class Texture1Test : public ITestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // 一个简单的 Quad (4个顶点)
        // 格式: [位置 X, Y, Z] [UV U, V]
        // 注意：为了演示 GL_REPEAT，我将 UV 设置为了 2.0，这意味着图片会重复两次
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

        // 1. Mesh Setup
        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // 2. Attributes
        GLsizei stride = 8 * sizeof(float);
        // Pos
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
        ctx.glEnableVertexAttribArray(0);
        // Color
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
        // UV
        ctx.glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));
        ctx.glEnableVertexAttribArray(2);

        // 3. Load Texture
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
       
    }

    void onRender(SoftRenderContext& ctx) override {
        shader.texture1 = ctx.getTextureObject(tex1);
        shader.texture2 = ctx.getTextureObject(tex2);

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

    TextureShader shader;
};

static TestRegistrar registrar("Texture", "Texture1", []() { return new Texture1Test(); });
