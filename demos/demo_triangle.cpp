#include <tinygl/tinygl.h> 
#include "../src/framework/vc.cpp"
#include <tinygl/color.h>
// ==========================================
// 1. 数据定义
// ==========================================

// 定义三角形的三个顶点 (x, y, z)
// 范围在 [-1, 1] 之间，直接对应屏幕坐标
float vertices[] = {
    0.0f,  0.5f, 0.0f, // 顶部
   -0.5f, -0.5f, 0.0f, // 左下
    0.5f, -0.5f, 0.0f  // 右下
};

// ==========================================
// 2. Shader 定义
// ==========================================

struct ColorShader : ShaderBase {
    // 顶点着色器
    // 输入：attribs[0] 是位置
    Vec4 vertex(const Vec4* attribs, ShaderContext& ctx) {
        // 直接返回输入的位置
        // 因为我们的数据已经是 Clip Space (-1.0 ~ 1.0) 的，
        // 且 w 默认为 1.0，所以不需要矩阵变换即可显示。
        return attribs[0];
    }

    // 片元着色器
    // 输出：颜色 (RGBA)
    Vec4 fragment(const ShaderContext& ctx) {
        return Vec4(1.0f, 0.5f, 0.2f, 1.0f);
    }
};

// ==========================================
// 3. 全局变量与初始化
// ==========================================

SoftRenderContext* ctx = nullptr;
ColorShader shader;

void vc_init(void) {
    // 1. 初始化上下文 (800x600)
    ctx = new SoftRenderContext(800, 600);

    // 2. 创建 VBO (Vertex Buffer Object)
    GLuint vbo;
    ctx->glGenBuffers(1, &vbo);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    // 3. 上传顶点数据
    ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 4. 配置顶点属性 (Attribute 0)
    ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(float) * 3, 0);
    ctx->glEnableVertexAttribArray(0);
}

void vc_input(SDL_Event *event) {
    // 不需要处理输入
}

Olivec_Canvas vc_render(float dt, void* pixels) {
    // 1. 将 SDL 的像素内存设置给渲染器
    ctx->setExternalBuffer((uint32_t*)pixels);

    // 2. 清屏 (黑色背景)
    ctx->glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    ctx->glClear(COLOR | DEPTH);

    // 3. 绘制调用
    // Mode = GL_TRIANGLES
    // First = 0 (从第0个顶点开始)
    // Count = 3 (绘制3个顶点)
    ctx->glDrawArrays(shader, GL_TRIANGLES, 0, 3);

    // 返回 canvas 用于可能的后期处理或调试显示
    return olivec_canvas((uint32_t*)pixels, 800, 600, 800);
}