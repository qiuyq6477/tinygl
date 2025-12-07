#include "../vc/tinygl.h" // Includes all tinygl declarations and implementations
#include "../vc/vc.cpp"     // Includes the vc framework and main loop

// Define the dimensions for this demo
#define DEMO_WIDTH 800
#define DEMO_HEIGHT 600

// Global variables for the tinygl context and scene state
std::unique_ptr<SoftRenderContext> g_ctx;
GLuint g_progID;
GLint g_uMVP;
GLint g_uTex;
GLuint g_vao, g_vbo, g_ebo, g_tex;
GLuint g_ebo_short, g_ebo_byte; // Added declarations

float g_rotationAngle = 0.0f; // For animation

void vc_init(void) {
    // 1. Initialize SoftRenderContext
    g_ctx = std::unique_ptr<SoftRenderContext>(new SoftRenderContext(DEMO_WIDTH, DEMO_HEIGHT));
    g_ctx->glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Set clear color to a dark grey

    // 2. Create and configure Shader Program
    g_progID = g_ctx->glCreateProgram();
    g_uMVP = g_ctx->glGetUniformLocation(g_progID, "uMVP");
    g_uTex = g_ctx->glGetUniformLocation(g_progID, "uTex");

    // Vertex Shader: Use optimized currMVP access
    SoftRenderContext* ctx = g_ctx.get();
    g_ctx->setVertexShader(g_progID, [ctx](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        outCtx.varyings[0] = attribs[2]; // UV
        outCtx.varyings[1] = attribs[1]; // Color
        
        Vec4 pos = attribs[0]; 
        pos.w = 1.0f;
        return ctx->currMVP * pos; // Direct, zero-overhead access
    });

    // Fragment Shader: 将插值后的颜色与纹理相乘
    g_ctx->setFragmentShader(g_progID, [&](const ShaderContext& inCtx) -> Vec4 {
        Vec4 uv    = inCtx.varyings[0];
        Vec4 color = inCtx.varyings[1]; // 获取插值后的顶点颜色

        int unit = 0;
        if (auto* p = g_ctx->getCurrentProgram()) if (p->uniforms.count(g_uTex)) unit = p->uniforms[g_uTex].data.i;
        
        Vec4 texColor = {1, 1, 1, 1};
        if (auto* tex = g_ctx->getTexture(unit)) texColor = tex->sample(uv.x, uv.y);

        // 核心修改: 顶点颜色 * 纹理颜色
        return color * texColor; 
    });

    // 3. Generate Texture
    g_ctx->glGenTextures(1, &g_tex);
    g_ctx->glActiveTexture(GL_TEXTURE0);
    g_ctx->glBindTexture(GL_TEXTURE_2D, g_tex);
    std::vector<uint32_t> pixels(256*256);
    for(int y=0; y<256; ++y) for(int x=0; x<256; ++x) {
        pixels[y*256+x] = (((x/32)+(y/32))%2) ? COLOR_WHITE : COLOR_GREY;
    }
    g_ctx->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // 4. Vertex and Index Data
    float vertices[] = {
        // Position (XYZ)     // Color (RGB)       // UV (UV)
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,    0.0f, 0.0f, // 左下 (红)
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,    1.0f, 0.0f, // 右下 (绿)
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,    1.0f, 1.0f, // 右上 (蓝)
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,    0.0f, 1.0f  // 左上 (黄)
    };
    uint32_t indices_uint[] = { 0, 1, 2 }; // Unsigned Int Indices
    uint16_t indices_ushort[] = { 2, 3, 0 }; // Unsigned Short Indices
    uint8_t indices_ubyte[] = { 0, 1, 3 };   // Unsigned Byte Indices (Example)

    // 5. Setup Buffers
    g_ctx->glGenVertexArrays(1, &g_vao);
    g_ctx->glBindVertexArray(g_vao);
    g_ctx->glGenBuffers(1, &g_vbo);
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    // EBO for Unsigned Int
    g_ctx->glGenBuffers(1, &g_ebo);
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices_uint), indices_uint, GL_STATIC_DRAW);

    // EBO for Unsigned Short
    g_ctx->glGenBuffers(1, &g_ebo_short);
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo_short);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices_ushort), indices_ushort, GL_STATIC_DRAW);

    // EBO for Unsigned Byte
    g_ctx->glGenBuffers(1, &g_ebo_byte);
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo_byte);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices_ubyte), indices_ubyte, GL_STATIC_DRAW);

    // 6. Configure Attributes
    GLsizei stride = 8 * sizeof(float);
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
    g_ctx->glEnableVertexAttribArray(0);
    g_ctx->glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
    g_ctx->glEnableVertexAttribArray(1);
    g_ctx->glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));
    g_ctx->glEnableVertexAttribArray(2);

    // 7. Use Program and set uniforms
    g_ctx->glUseProgram(g_progID);
    g_ctx->glUniform1i(g_uTex, 0);
    // Initial MVP is set in render loop for animation
}

void vc_input(SDL_Event *event) {
    (void)event; // No specific input handling for this demo
}

Olivec_Canvas vc_render(float dt, void* pixels) {
    (void)dt; // Suppress unused warning
    if (!g_ctx) return olivec_canvas(nullptr, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);

    g_ctx->setExternalBuffer((uint32_t*)pixels);
    g_ctx->glClear(BufferType::COLOR | BufferType::DEPTH);

    // MVP
    Mat4 model = Mat4::Translate(0, 0, -3.0f);
    Mat4 view = Mat4::Identity();
    float aspect = (float)DEMO_WIDTH / (float)DEMO_HEIGHT;
    Mat4 proj = Mat4::Perspective(90.0f, aspect, 0.1f, 100.0f);
    g_ctx->currMVP = proj * view * model;

    // 1. Draw using Unsigned Int Indices
    // First triangle (indices 0, 1, 2)
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    g_ctx->glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_INT, (void*)0);

    // 2. Draw using Unsigned Short Indices
    // Second triangle (indices 2, 3, 0)
    // We need to re-bind the Short EBO
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo_short);
    // Shift slightly to see difference
    g_ctx->currMVP = proj * view * Mat4::Translate(1.5f, 0, -3.0f);
    g_ctx->glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_SHORT, (void*)0);

    // 3. Draw using Unsigned Byte Indices
    // Example triangle (indices 0, 1, 3)
    // Re-bind Byte EBO
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo_byte);
    g_ctx->currMVP = proj * view * Mat4::Translate(-1.5f, 0, -3.0f);
    g_ctx->glDrawElements(GL_TRIANGLES, 3, GL_UNSIGNED_BYTE, (void*)0);

    return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
}