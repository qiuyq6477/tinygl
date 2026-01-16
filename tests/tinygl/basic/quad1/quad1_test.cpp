#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>

using namespace tinygl;
using namespace framework;

struct GradientShader : public ShaderBuiltins {
    // 椤剁偣鐫€鑹插櫒
    // attribs[0]: 浣嶇疆 (x, y, z)
    // attribs[1]: 棰滆壊 (r, g, b)
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // 1. 灏嗛鑹蹭紶閫掔粰 Varying 0锛屼互渚垮湪鍍忕礌闂存彃鍊?
        ctx.varyings[0] = attribs[1];
        // 2. 鐩存帴杩斿洖 Clip Space 鍧愭爣 (鏃犻渶鐭╅樀锛屽洜涓哄潗鏍囧凡鍦?-1~1 鑼冨洿鍐?
        gl_Position = attribs[0];
    }

    // 鐗囧厓鐫€鑹插櫒
    void fragment(const ShaderContext& ctx) {
        // 鑾峰彇鎻掑€煎悗鐨勯鑹?
        gl_FragColor = ctx.varyings[0];
    }
};

class Quad1Test : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // 鐭╁舰鐨?4 涓《鐐?
        // 鏍煎紡: [浣嶇疆 X, Y, Z] [棰滆壊 R, G, B]
        float vertices[] = {
            // 0: 宸︿笂瑙?(绾㈣壊)
            -0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f,
            // 1: 鍙充笂瑙?(缁胯壊)
            0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f,
            // 2: 鍙充笅瑙?(钃濊壊)
            0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f,
            // 3: 宸︿笅瑙?(榛勮壊)
            -0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 0.0f
        };

        // 绱㈠紩鏁版嵁锛氬畾涔変袱涓笁瑙掑舰缁勬垚涓€涓煩褰?
        // 椤哄簭鏄€嗘椂閽?(CCW)
        uint32_t indices[] = {
            0, 2, 1,
            2, 0, 3
        };
        // Create VAO
        ctx.glGenVertexArrays(1, &vao);
        ctx.glBindVertexArray(vao);

        // --- 1. 鍒涘缓骞朵笂浼?VBO (椤剁偣鏁版嵁) ---
        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // --- 2. 鍒涘缓骞朵笂浼?EBO (绱㈠紩鏁版嵁) ---
        ctx.glGenBuffers(1, &ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        // --- 3. 閰嶇疆椤剁偣灞炴€?(鍏抽敭姝ラ) ---
        // 璁＄畻姝ラ暱 (Stride): 涓€涓畬鏁撮《鐐瑰寘鍚?6 涓?float (3 pos + 3 color)
        GLsizei stride = 6 * sizeof(float);

        // 灞炴€?0: 浣嶇疆 (Offset = 0)
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
        ctx.glEnableVertexAttribArray(0);

        // 灞炴€?1: 棰滆壊 (Offset = 3 * sizeof(float), 鍗宠烦杩囧墠 3 涓綅缃暟鎹?
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteBuffers(1, &ebo);
        ctx.glDeleteVertexArrays(1, &vao);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        // Add specific controls for this test
    }

    void onRender(SoftRenderContext& ctx) override {
        // 缁樺埗璋冪敤
        // Mode: GL_TRIANGLES
        // Count: 6 (鍥犱负鏈変袱涓笁瑙掑舰锛屽叡6涓储寮?
        // Type: GL_UNSIGNED_INT (indices 鏁扮粍鐨勭被鍨?
        // Indices: (void*)0 (鍥犱负宸茬粡缁戝畾浜?EBO锛屾墍浠ヨ繖閲岀殑鎸囬拡鏄?Buffer 鍐呯殑鍋忕Щ閲忥紝閫氬父涓?0)
        // 绗竴绉嶆柟寮?
        ctx.glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
        
        // 绗簩绉嶆柟寮?
        // 璺宠繃绗竴涓笁瑙掑舰锛屽彧缁樺埗绗簩涓笁瑙掑舰
        // 闇€瑕佽烦杩囧墠 3 涓储寮曘€傚洜涓虹被鍨嬫槸 GL_UNSIGNED_INT (4瀛楄妭)锛屾墍浠ュ瓧鑺傚亸绉婚噺 = 3 * 4 = 12 瀛楄妭
        // 鍋忕Щ閲忥紙Offset锛夐€氬父鐢ㄤ簬 Batching锛堟壒澶勭悊锛夛細
        // 鍙互鍦ㄤ竴涓法澶х殑 EBO 涓瓨鍌ㄥ涓ā鍨嬬殑绱㈠紩銆傛ā鍨?A 鐨勭储寮曟斁鍦?0~100 瀛楄妭銆傛ā鍨?B 鐨勭储寮曟斁鍦?100~200 瀛楄妭銆?
        // 鐢绘ā鍨?A锛歡lDrawElements(..., count=25, ..., offset=0)
        // 鐢绘ā鍨?B锛歡lDrawElements(..., count=25, ..., offset=100)
        // ctx->glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)12);
        
        // 绗笁绉嶆柟寮?
        // 鐩存帴浼犻€?CPU 鏁扮粍鎸囬拡
        // ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        // ctx->glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, indices);

    }

private:
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint vao = 0;
    GradientShader shader;
};

// Register the test
static TestRegistrar registrar("Basic", "Quad1", []() { return new Quad1Test(); });
