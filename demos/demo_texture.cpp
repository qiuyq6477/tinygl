#define STB_IMAGE_IMPLEMENTATION
#include "../vc/stb_image.h" // 请确保此文件在目录下

#include "../vc/vc.cpp"        // 平台入口
#include "../vc/tinygl.h"  // 软渲染器实现
#include <algorithm>


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

SoftRenderContext* g_ctx = nullptr;
GLuint g_vbo, g_ebo, g_tex1, g_tex2;

struct TextureShader : ShaderBase {
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

        TextureObject* texture = g_ctx->getTextureObject(g_tex1);
        Vec4 texColor = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
        if (texture) {
            texColor = texture->sampleNearestFast(uv.x, uv.y);
        }

        TextureObject* texture2 = g_ctx->getTextureObject(g_tex2);
        Vec4 texColor2 = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
        if (texture2) {
            texColor2 = texture2->sampleNearestFast(uv.x, uv.y);
        }
        // 合并颜色
        return mix(texColor, texColor2, 0.5);
    }
} shader;

// 加载纹理的辅助函数
bool loadTexture(const char* filename, GLenum textureUnit, GLuint& tex) {
    int width, height, nrChannels;
    
    // 翻转 Y 轴 (因为 OpenGL 纹理坐标 Y 轴向上，而图片通常是从上往下)
    // stb_image 默认左上角为 (0,0)，我们需要让它符合 GL 习惯
    // 或者我们在 Shader 里用 1.0 - v 也可以。这里用 stbi 的功能。
    stbi_set_flip_vertically_on_load(true); // 取决于你的 UV 映射习惯，这里暂时注释，按默认走

    // 强制加载为 4 通道 (RGBA)，简化处理
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 4);
    
    if (data) {
        g_ctx->glGenTextures(1, &tex);
        g_ctx->glActiveTexture(textureUnit);
        g_ctx->glBindTexture(GL_TEXTURE_2D, tex);

        // 设置纹理参数
        // 1. 重复模式 (Repeat)
        g_ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        g_ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        // 2. 过滤模式 (Linear = 平滑, Nearest = 像素风)
        g_ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        g_ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        // 上传数据
        // InternalFormat = GL_RGBA
        // Type = GL_UNSIGNED_BYTE (stbi 读出来的是 char)
        g_ctx->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        stbi_image_free(data);
        printf("Texture loaded: %s (%dx%d)\n", filename, width, height);
        return true;
    } else {
        printf("Failed to load texture: %s\n", filename);
        // 生成一个默认的粉色纹理作为 Fallback
        uint32_t pinkPixel = 0xFFFF00FF; // ABGR
        g_ctx->glGenTextures(1, &tex);
        g_ctx->glBindTexture(GL_TEXTURE_2D, tex);
        g_ctx->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pinkPixel);
        return false;
    }
}

void vc_init(void) {
    g_ctx = new SoftRenderContext(800, 600);

    // 1. Mesh Setup
    g_ctx->glGenBuffers(1, &g_vbo);
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    g_ctx->glGenBuffers(1, &g_ebo);
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 2. Attributes
    GLsizei stride = 8 * sizeof(float);
    // Pos
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
    g_ctx->glEnableVertexAttribArray(0);
    // Color
    g_ctx->glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
    g_ctx->glEnableVertexAttribArray(1);
    // UV
    g_ctx->glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));
    g_ctx->glEnableVertexAttribArray(2);

    // 3. Load Texture
    loadTexture("assets/container.jpg" , GL_TEXTURE0, g_tex1);
    loadTexture("assets/awesomeface.png" , GL_TEXTURE1, g_tex2);
}

void vc_input(SDL_Event *event) {
    // 交互逻辑...
}

Olivec_Canvas vc_render(float dt, void* pixels) {
    g_ctx->setExternalBuffer((uint32_t*)pixels);

    // 清屏 (深灰色)
    g_ctx->glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    g_ctx->glClear(COLOR | DEPTH);

    // 绘制
    g_ctx->glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
    return olivec_canvas((uint32_t*)pixels, 800, 600, 800);
}