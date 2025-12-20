#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>

using namespace tinygl;

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

class Quad2Test : public ITestCase {
public:
    void init(SoftRenderContext& ctx) override {
        float vertices[] = {
            // 0: 左上角 (红色)
            -0.5f,  0.5f, 0.0f,   0.0f, 0.0f,
            // 1: 右上角 (绿色)
            0.5f,  0.5f, 0.0f,   1.0f, 0.0f,
            // 2: 右下角 (蓝色)
            0.5f, -0.5f, 0.0f,   1.0f, 1.0f,
            // 3: 左下角 (黄色)
            -0.5f, -0.5f, 0.0f,   0.0f, 1.0f
        };

        // 索引数据：定义两个三角形组成一个矩形
        // 顺序是逆时针 (CCW)
        uint32_t indices[] = {
            0, 2, 1,
            2, 0, 3
        };
        // --- 1. 创建并上传 VBO (顶点数据) ---
        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // --- 2. 创建并上传 EBO (索引数据) ---
        ctx.glGenBuffers(1, &ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // --- 3. 配置顶点属性 (关键步骤) ---
        // 现在的 stride 是 5 个 float (3 pos + 2 uv)
        GLsizei stride = 5 * sizeof(float);

        // 属性 0: Pos
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
        ctx.glEnableVertexAttribArray(0);

        // 属性 1: UV (注意这里 size 是 2)
        ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteVertexArrays(1, &ebo);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        // Add specific controls for this test
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);    
    }

private:
    GLuint vbo = 0;
    GLuint ebo = 0;
    BilinearShader shader;
};

// Register the test
static TestRegistrar registrar("Basic", "Quad2", []() { return new Quad2Test(); });
