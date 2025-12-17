#include <algorithm>  // std::clamp
#include "../src/framework/vc.cpp"       // 平台入口 (SDL/Term)
#include <tinygl/tinygl.h> // 软渲染器实现
#include <tinygl/color.h>  // 颜色转换工具
// ==========================================
// 1. 数据定义
// ==========================================
// 简单的 2x2 网格生成
// 原始范围: -0.5 ~ 0.5
// 步长: 0.5
float vertices[] = {
    // Row 0
    -0.5f,  0.5f, 0.0f,   0.0f, 0.0f, // 0
     0.0f,  0.5f, 0.0f,   0.5f, 0.0f, // 1
     0.5f,  0.5f, 0.0f,   1.0f, 0.0f, // 2
    // Row 1
    -0.5f,  0.0f, 0.0f,   0.0f, 0.5f, // 3
     0.0f,  0.0f, 0.0f,   0.5f, 0.5f, // 4
     0.5f,  0.0f, 0.0f,   1.0f, 0.5f, // 5
    // Row 2
    -0.5f, -0.5f, 0.0f,   0.0f, 1.0f, // 6
     0.0f, -0.5f, 0.0f,   0.5f, 1.0f, // 7
     0.5f, -0.5f, 0.0f,   1.0f, 1.0f  // 8
};

// 2x2 = 4个矩形 = 8个三角形
uint32_t indices[] = {
    // Quad TL
    0, 3, 1,  1, 3, 4,
    // Quad TR
    1, 4, 2,  2, 4, 5,
    // Quad BL
    3, 6, 4,  4, 6, 7,
    // Quad BR
    4, 7, 5,  5, 7, 8
};

// ==========================================
// 2. Shader 定义
// ==========================================

struct BilinearShader : ShaderBase {
    // 定义 4 个角的颜色
    const Vec4 cTL = {1.0f, 0.0f, 0.0f, 1.0f}; // 红
    const Vec4 cTR = {0.0f, 1.0f, 0.0f, 1.0f}; // 绿
    const Vec4 cBR = {0.0f, 0.0f, 1.0f, 1.0f}; // 蓝
    const Vec4 cBL = {1.0f, 1.0f, 0.0f, 1.0f}; // 黄

    // 顶点着色器
    Vec4 vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[1] 是 UV，只有前两个分量有用 (u, v, 0, 1)
        ctx.varyings[0] = attribs[1]; 
        return attribs[0];
    }

    // 片元着色器
    Vec4 fragment(const ShaderContext& ctx) {
        // 获取插值后的 UV
        // 这里的 UV 是由三角形光栅化线性插值过来的，
        // 对于平面的矩形，三角形插值得到的 UV 和矩形 UV 是完全一致的，不会有折痕。
        float u = ctx.varyings[0].x;
        float v = ctx.varyings[0].y;

        // 【关键】手动进行双线性插值 (Bilinear Interpolation)
        // 1. 在顶部两个角之间插值
        Vec4 colorTop = mix(cTL, cTR, u);
        // 2. 在底部两个角之间插值
        Vec4 colorBot = mix(cBL, cBR, u);
        // 3. 在垂直方向插值
        Vec4 finalColor = mix(colorTop, colorBot, v);
        return finalColor;
    }
};
// ==========================================
// 3. 全局变量与初始化
// ==========================================

SoftRenderContext* ctx = nullptr;
GLuint vbo, ebo;
BilinearShader bilinearShader;
void vc_init(void) {
    // 初始化 800x600 的画布
    ctx = new SoftRenderContext(800, 600);

    // --- 1. 创建并上传 VBO (顶点数据) ---
    ctx->glGenBuffers(1, &vbo);
    ctx->glBindBuffer(GL_ARRAY_BUFFER, vbo);
    ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // --- 2. 创建并上传 EBO (索引数据) ---
    ctx->glGenBuffers(1, &ebo);
    ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // --- 3. 配置顶点属性 (关键步骤) ---
    // 现在的 stride 是 5 个 float (3 pos + 2 uv)
    GLsizei stride = 5 * sizeof(float);

    // 属性 0: Pos
    ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
    ctx->glEnableVertexAttribArray(0);

    // 属性 1: UV (注意这里 size 是 2)
    ctx->glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
    ctx->glEnableVertexAttribArray(1);
}

void vc_input(SDL_Event *event) {
    // 暂无需处理输入
}

Olivec_Canvas vc_render(float dt, void* pixels) {
    // 设置外部 Framebuffer
    ctx->setExternalBuffer((uint32_t*)pixels);

    // 清屏 (灰色背景)
    ctx->glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    ctx->glClear(COLOR | DEPTH);

    // 绘制调用
    // Mode: GL_TRIANGLES
    // Count: 6 (因为有两个三角形，共6个索引)
    // Type: GL_UNSIGNED_INT (indices 数组的类型)
    // Indices: (void*)0 (因为已经绑定了 EBO，所以这里的指针是 Buffer 内的偏移量，通常为 0)
    // 第一种方式
// 索引数量变成了 8个三角形 * 3 = 24
ctx->glDrawElements(bilinearShader, GL_TRIANGLES, 24, GL_UNSIGNED_INT, (void*)0);    
    // 第二种方式
    // 跳过第一个三角形，只绘制第二个三角形
    // 需要跳过前 3 个索引。因为类型是 GL_UNSIGNED_INT (4字节)，所以字节偏移量 = 3 * 4 = 12 字节
    // 偏移量（Offset）通常用于 Batching（批处理）：
    // 可以在一个巨大的 EBO 中存储多个模型的索引。模型 A 的索引放在 0~100 字节。模型 B 的索引放在 100~200 字节。
    // 画模型 A：glDrawElements(..., count=25, ..., offset=0)
    // 画模型 B：glDrawElements(..., count=25, ..., offset=100)
    // ctx->glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)12);
    
    // 第三种方式
    // 直接传递 CPU 数组指针
    // ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    // ctx->glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);

    return olivec_canvas((uint32_t*)pixels, 800, 600, 800);
}