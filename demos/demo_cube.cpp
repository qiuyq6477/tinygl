#include "../vc/tinygl.h" 
#include "../vc/vc.cpp"

#define DEMO_WIDTH 800
#define DEMO_HEIGHT 600

std::unique_ptr<SoftRenderContext> g_ctx;
GLuint g_vao, g_vbo, g_ebo;
GLuint g_tex;
float g_rotationAngle = 0.0f;
static uint32_t g_cubeIndices[36]; 
// ==========================================
// 定义 Shader 结构体 (Template Architecture)
// ==========================================
struct CubeShader {
    // Uniforms
    TextureObject* texture = nullptr;
    SimdMat4 mvp; // 使用 SIMD 矩阵加速

    // Vertex Shader (返回 Clip Space Pos)
    // 编译器会自动内联
    // inline Vec4 vertex(const std::vector<Vec4>& attribs, ShaderContext& outCtx) {
    //     // attribs[0]: Pos, attribs[1]: Color, attribs[2]: UV
    //     outCtx.varyings[0] = attribs[2]; // UV
    //     outCtx.varyings[1] = attribs[1]; // Color

    //     // SIMD 矩阵乘法
    //     // 假设 attribs[0] 是 {x, y, z, 1}
    //     float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, 1.0f};
    //     Simd4f pos = Simd4f::load(posArr);
        
    //     Simd4f res = mvp.transformPoint(pos);
        
    //     float outArr[4];
    //     res.store(outArr);
    //     return Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    // }
    // [修改] 接收数组指针，而不是 vector
    // 为了安全，可以是 const Vec4*，我们在调用时传数组首地址
    inline Vec4 vertex(const Vec4* attribs, ShaderContext& outCtx) {
        // 直接通过数组索引访问，无边界检查开销
        outCtx.varyings[0] = attribs[2]; // UV
        outCtx.varyings[1] = attribs[1]; // Color

        // SIMD 矩阵变换
        // 假设 attribs[0] 是 {x, y, z, 1}
        float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, 1.0f};
        Simd4f pos = Simd4f::load(posArr);
        Simd4f res = mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        return Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    }

    // Fragment Shader (返回 0xAABBGGRR)
    inline uint32_t fragment(const ShaderContext& inCtx) {
        // 1. 极速纹理采样 (Nearest)
        uint32_t texColor = 0xFFFFFFFF;
        if (texture) {
            texColor = texture->sampleNearestFast(inCtx.varyings[0].x, inCtx.varyings[0].y);
        }

        // 2. 混合顶点颜色 (简单乘法)
        // 解析 Texture Color
        uint32_t r = texColor & 0xFF;
        uint32_t g = (texColor >> 8) & 0xFF;
        uint32_t b = (texColor >> 16) & 0xFF;

        // 混合 Varying Color (假设 varyings[1] 是 normalized 0.0-1.0)
        // 避免 float->vec4->float 的来回转换，直接算
        float vr = inCtx.varyings[1].x;
        float vg = inCtx.varyings[1].y;
        float vb = inCtx.varyings[1].z;

        uint32_t fr = (uint32_t)(r * vr);
        uint32_t fg = (uint32_t)(g * vg);
        uint32_t fb = (uint32_t)(b * vb);

        return (255 << 24) | (fb << 16) | (fg << 8) | fr;
    }
};

struct Vertex {
    float pos[3];
    uint8_t color[3];
    uint8_t padding; 
    float uv[2];
};

void vc_init(void) {
    g_ctx = std::unique_ptr<SoftRenderContext>(new SoftRenderContext(DEMO_WIDTH, DEMO_HEIGHT));
    
    // 1. 创建纹理
    g_ctx->glGenTextures(1, &g_tex); 
    g_ctx->glActiveTexture(GL_TEXTURE0); 
    g_ctx->glBindTexture(GL_TEXTURE_2D, g_tex);
    std::vector<uint32_t> pixels(256 * 256);
    for(int i=0; i<256*256; i++) {
        int x = i % 256;
        int y = i / 256;
        // 生成棋盘格：白色(0xFFFFFFFF) 和 黑色(0xFF000000，注意 Alpha 必须是 FF)
        bool isWhite = ((x / 32) + (y / 32)) % 2 != 0;
        pixels[i] = isWhite ? 0xFFFFFFFF : 0xFF000000; // Alpha=255
    }
    g_ctx->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // 2. 上传顶点数据 (保持不变)
    Vertex vertices[] = {
        // Front
        {{-1,-1, 1}, {255,0,0}, 0, {0,0}}, {{ 1,-1, 1}, {255,0,0}, 0, {1,0}},
        {{ 1, 1, 1}, {255,0,0}, 0, {1,1}}, {{-1, 1, 1}, {255,0,0}, 0, {0,1}},
        // Back
        {{ 1,-1,-1}, {0,255,0}, 0, {0,0}}, {{-1,-1,-1}, {0,255,0}, 0, {1,0}},
        {{-1, 1,-1}, {0,255,0}, 0, {1,1}}, {{ 1, 1,-1}, {0,255,0}, 0, {0,1}},
        // Left
        {{-1,-1,-1}, {0,0,255}, 0, {0,0}}, {{-1,-1, 1}, {0,0,255}, 0, {1,0}},
        {{-1, 1, 1}, {0,0,255}, 0, {1,1}}, {{-1, 1,-1}, {0,0,255}, 0, {0,1}},
        // Right
        {{ 1,-1, 1}, {255,255,0}, 0, {0,0}}, {{ 1,-1,-1}, {255,255,0}, 0, {1,0}},
        {{ 1, 1,-1}, {255,255,0}, 0, {1,1}}, {{ 1, 1, 1}, {255,255,0}, 0, {0,1}},
        // Top
        {{-1, 1, 1}, {0,255,255}, 0, {0,0}}, {{ 1, 1, 1}, {0,255,255}, 0, {1,0}},
        {{ 1, 1,-1}, {0,255,255}, 0, {1,1}}, {{-1, 1,-1}, {0,255,255}, 0, {0,1}},
        // Bottom
        {{-1,-1,-1}, {255,0,255}, 0, {0,0}}, {{ 1,-1,-1}, {255,0,255}, 0, {1,0}},
        {{ 1,-1, 1}, {255,0,255}, 0, {1,1}}, {{-1,-1, 1}, {255,0,255}, 0, {0,1}}
    };

    uint32_t *indices = g_cubeIndices;
    for(int i=0; i<6; i++) {
        uint32_t base = i*4;
        indices[i*6+0]=base; indices[i*6+1]=base+1; indices[i*6+2]=base+2;
        indices[i*6+3]=base+2; indices[i*6+4]=base+3; indices[i*6+5]=base;
    }

    g_ctx->glGenVertexArrays(1, &g_vao); 
    g_ctx->glBindVertexArray(g_vao);
    g_ctx->glGenBuffers(1, &g_vbo); 
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    g_ctx->glGenBuffers(1, &g_ebo); 
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_cubeIndices), g_cubeIndices, GL_STATIC_DRAW);

    GLsizei stride = sizeof(Vertex);
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)offsetof(Vertex, pos));
    g_ctx->glEnableVertexAttribArray(0);
    g_ctx->glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, true, stride, (void*)offsetof(Vertex, color));
    g_ctx->glEnableVertexAttribArray(1);
    g_ctx->glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)offsetof(Vertex, uv));
    g_ctx->glEnableVertexAttribArray(2);
}

void vc_input(SDL_Event *event) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_SPACE) g_rotationAngle = 0.0f;
}

// Olivec_Canvas vc_render(float dt, void* pixels) {
//     if (!g_ctx) return olivec_canvas(nullptr, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
//     g_ctx->setExternalBuffer((uint32_t*)pixels);
//     g_ctx->glClear(BufferType::COLOR | BufferType::DEPTH);

//     g_rotationAngle += 45.0f * dt;

//     // 1. 实例化 Shader (在栈上，极快)
//     CubeShader shader;
    
//     // 2. 设置 Uniforms
//     // 直接获取纹理对象指针
//     shader.texture = g_ctx->getTexture(g_tex); 

//     // 计算 MVP 并加载到 SIMD 寄存器
//     Mat4 model = Mat4::Translate(0, 0, -3.0f) * Mat4::RotateY(g_rotationAngle) * Mat4::RotateX(g_rotationAngle * 0.5f);
//     Mat4 proj = Mat4::Perspective(90.0f, (float)DEMO_WIDTH/DEMO_HEIGHT, 0.1f, 100.0f);
//     Mat4 mvp = proj * Mat4::Identity() * model;
    
//     shader.mvp.load(mvp); // Load once per frame

//     // 3. 调用模板化的绘制函数
//     // 手动构建索引 vector (为了适配新的接口，虽然有点多余但为了复用逻辑)
//     // 注意：这里为了性能，实际 tinygl 应该提供直接传指针的重载，但我们先凑合用 vector
//     // 更好的做法是让 drawElementsTemplate 接收 raw pointer
//     std::vector<uint32_t> idxCache(36);
//     // 这里其实很慢，生产环境应该把 indices 缓存在 context 里
//     // 暂时先重新生成一下作为演示，或者我们在 vc_init 里把 indices 存个全局
//     // 为了简单，我们这里重新填一遍，实际项目请优化
//     for(int i=0; i<6; i++) {
//          uint32_t base = i*4;
//          idxCache[i*6+0]=base; idxCache[i*6+1]=base+1; idxCache[i*6+2]=base+2;
//          idxCache[i*6+3]=base+2; idxCache[i*6+4]=base+3; idxCache[i*6+5]=base;
//     }
    
//     g_ctx->drawElementsTemplate(shader, 36, GL_UNSIGNED_INT, g_cubeIndices);

//     return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
// }
Olivec_Canvas vc_render(float dt, void* pixels) {
    if (!g_ctx) return olivec_canvas(nullptr, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
    g_ctx->setExternalBuffer((uint32_t*)pixels);
    g_ctx->glClear(BufferType::COLOR | BufferType::DEPTH);

    g_rotationAngle += 45.0f * dt;

    // 1. 实例化 Shader
    CubeShader shader;
    shader.texture = g_ctx->getTextureObject(g_tex); 

    // 2. 计算 MVP
    Mat4 model = Mat4::Translate(0, 0, -3.0f) * Mat4::RotateY(g_rotationAngle) * Mat4::RotateX(g_rotationAngle * 0.5f);
    Mat4 proj = Mat4::Perspective(90.0f, (float)DEMO_WIDTH/DEMO_HEIGHT, 0.1f, 100.0f);
    Mat4 mvp = proj * Mat4::Identity() * model;
    shader.mvp.load(mvp);

    // 3. [修改] 使用统一的 drawElements 接口
    // 参数：Shader对象, 模式, 数量, 类型, 索引指针
    // 此时 g_ctx 会根据 mode=GL_TRIANGLES 自动路由逻辑
    g_ctx->glDrawElements(shader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0);

    return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
}