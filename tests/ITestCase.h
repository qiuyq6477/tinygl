#pragma once
#include <tinygl/tinygl.h>
#include <tinygl/framework/application.h>
#include <tinygl/third_party/microui.h>
#include <tinygl/framework/camera.h>
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
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

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
};
