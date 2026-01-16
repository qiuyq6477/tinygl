#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>

using namespace tinygl;
using namespace framework;

struct BilinearShader : public ShaderBuiltins {
    // 瀹氫箟 4 涓鐨勯鑹?
    const Vec4 cTL = {1.0f, 0.0f, 0.0f, 1.0f}; // 绾?
    const Vec4 cTR = {0.0f, 1.0f, 0.0f, 1.0f}; // 缁?
    const Vec4 cBR = {0.0f, 0.0f, 1.0f, 1.0f}; // 钃?
    const Vec4 cBL = {1.0f, 1.0f, 0.0f, 1.0f}; // 榛?

    // 椤剁偣鐫€鑹插櫒
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[1] 鏄?UV锛屽彧鏈夊墠涓や釜鍒嗛噺鏈夌敤 (u, v, 0, 1)
        ctx.varyings[0] = attribs[1]; 
        gl_Position = attribs[0];
    }

    // 鐗囧厓鐫€鑹插櫒
    void fragment(const ShaderContext& ctx) {
        // 鑾峰彇鎻掑€煎悗鐨?UV
        // 杩欓噷鐨?UV 鏄敱涓夎褰㈠厜鏍呭寲绾挎€ф彃鍊艰繃鏉ョ殑锛?
        // 瀵逛簬骞抽潰鐨勭煩褰紝涓夎褰㈡彃鍊煎緱鍒扮殑 UV 鍜岀煩褰?UV 鏄畬鍏ㄤ竴鑷寸殑锛屼笉浼氭湁鎶樼棔銆?
        float u = ctx.varyings[0].x;
        float v = ctx.varyings[0].y;

        // 銆愬叧閿€戞墜鍔ㄨ繘琛屽弻绾挎€ф彃鍊?(Bilinear Interpolation)
        // 1. 鍦ㄩ《閮ㄤ袱涓涔嬮棿鎻掑€?
        Vec4 colorTop = mix(cTL, cTR, u);
        // 2. 鍦ㄥ簳閮ㄤ袱涓涔嬮棿鎻掑€?
        Vec4 colorBot = mix(cBL, cBR, u);
        // 3. 鍦ㄥ瀭鐩存柟鍚戞彃鍊?
        Vec4 finalColor = mix(colorTop, colorBot, v);
        gl_FragColor = finalColor;
    }
};

class Quad2Test : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        float vertices[] = {
            // 0: 宸︿笂瑙?(绾㈣壊)
            -0.5f,  0.5f, 0.0f,   0.0f, 0.0f,
            // 1: 鍙充笂瑙?(缁胯壊)
            0.5f,  0.5f, 0.0f,   1.0f, 0.0f,
            // 2: 鍙充笅瑙?(钃濊壊)
            0.5f, -0.5f, 0.0f,   1.0f, 1.0f,
            // 3: 宸︿笅瑙?(榛勮壊)
            -0.5f, -0.5f, 0.0f,   0.0f, 1.0f
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
        // 鐜板湪鐨?stride 鏄?5 涓?float (3 pos + 2 uv)
        GLsizei stride = 5 * sizeof(float);

        // 灞炴€?0: Pos
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
        ctx.glEnableVertexAttribArray(0);

        // 灞炴€?1: UV (娉ㄦ剰杩欓噷 size 鏄?2)
        ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
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
        ctx.glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);    
    }

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    BilinearShader shader;
};

// Register the test
static TestRegistrar registrar("Basic", "Quad2", []() { return new Quad2Test(); });
