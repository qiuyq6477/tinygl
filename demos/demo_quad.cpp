#include <algorithm>  // std::clamp
#include "../src/framework/vc.cpp"       // 平台入口 (SDL/Term)
#include <tinygl/tinygl.h> // 软渲染器实现
#include <tinygl/color.h>  // 颜色转换工具
// ==========================================
// 1. 数据定义
// ==========================================

// 矩形的 4 个顶点
// 格式: [位置 X, Y, Z] [颜色 R, G, B]
float vertices[] = {
    // 0: 左上角 (红色)
    -0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,
    // 1: 右上角 (绿色)
     0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
    // 2: 右下角 (蓝色)
     0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,
    // 3: 左下角 (黄色)
    -0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 0.0f
};

// 索引数据：定义两个三角形组成一个矩形
// 顺序是逆时针 (CCW)
uint32_t indices[] = {
    0, 2, 1,
    2, 0, 3
};

// ==========================================
// 2. Shader 定义
// ==========================================

struct GradientShader : ShaderBase {
    // 顶点着色器
    // attribs[0]: 位置 (x, y, z)
    // attribs[1]: 颜色 (r, g, b)
    Vec4 vertex(const Vec4* attribs, ShaderContext& ctx) {
        // 1. 将颜色传递给 Varying 0，以便在像素间插值
        ctx.varyings[0] = attribs[1];
        // 2. 直接返回 Clip Space 坐标 (无需矩阵，因为坐标已在 -1~1 范围内)
        return attribs[0];
    }

    // 片元着色器
    Vec4 fragment(const ShaderContext& ctx) {
        // 获取插值后的颜色
        return ctx.varyings[0];
    }
};

// ==========================================
// 3. 全局变量与初始化
// ==========================================

SoftRenderContext* ctx = nullptr;
GLuint vbo, ebo;
GradientShader shader;

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
    // 计算步长 (Stride): 一个完整顶点包含 6 个 float (3 pos + 3 color)
    GLsizei stride = 6 * sizeof(float);

    // 属性 0: 位置 (Offset = 0)
    ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
    ctx->glEnableVertexAttribArray(0);

    // 属性 1: 颜色 (Offset = 3 * sizeof(float), 即跳过前 3 个位置数据)
    ctx->glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
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
    ctx->glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
    
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