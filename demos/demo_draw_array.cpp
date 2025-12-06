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
GLuint g_vao, g_vbo, g_tex; // No EBO for glDrawArrays

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
    
    // 4. Vertex Data (VBO Only / Non-Indexed)
    float vertices[] = {
        // Pos(3)             Color(3)           UV(2)
        // Triangle 1
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  0.0f, 0.0f, // BL (Red)
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,  1.0f, 0.0f, // BR (Green)
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f, // TR (Blue)

        // Triangle 2
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f, // TR (Blue)
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,  0.0f, 1.0f, // TL (Yellow)
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  0.0f, 0.0f  // BL (Red)
    };

    // 5. Setup Buffers
    g_ctx->glGenVertexArrays(1, &g_vao); g_ctx->glBindVertexArray(g_vao);
    g_ctx->glGenBuffers(1, &g_vbo); g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 6. Configure Attributes
    GLsizei stride = 8 * sizeof(float);
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);                   g_ctx->glEnableVertexAttribArray(0);
    g_ctx->glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3*sizeof(float)));   g_ctx->glEnableVertexAttribArray(1);
    g_ctx->glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6*sizeof(float)));   g_ctx->glEnableVertexAttribArray(2);

    // 7. Use Program and set uniforms
    g_ctx->glUseProgram(g_progID);
    g_ctx->glUniform1i(g_uTex, 0);
    // Initial MVP is set in render loop for animation
}

void vc_input(SDL_Event *event) {
    (void)event; // No specific input handling for this demo
}

Olivec_Canvas vc_render(float dt) {
    if (!g_ctx) return olivec_canvas(nullptr, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);

    g_ctx->glClear(BufferType::COLOR | BufferType::DEPTH); // Clear color and depth buffer

    g_rotationAngle += 45.0f * dt; // Rotate 45 degrees per second

    // Calculate MVP matrix
    // Model: Apply rotation and translation to the quad
    Mat4 model = Mat4::Translate(0, 0, -2.0f); // Move quad back to be visible
    model = model * Mat4::RotateY(g_rotationAngle); // Rotate around Y-axis
    
    Mat4 view = Mat4::Identity(); // Simple Identity View
    float aspect = (float)DEMO_WIDTH / (float)DEMO_HEIGHT;
    Mat4 proj = Mat4::Perspective(90.0f, aspect, 0.1f, 100.0f); // 90 deg FOV

    Mat4 mvp = proj * view * model; // P * V * M

    // Update the fast-path MVP
    g_ctx->currMVP = mvp;

    // Update MVP uniform (Optional)
    g_ctx->glUniformMatrix4fv(g_uMVP, 1, false, mvp.m);

    // Draw triangle using glDrawArrays (6 vertices)
    g_ctx->glDrawArrays(GL_TRIANGLES, 0, 6); // 6 vertices total (2 triangles)

    // Return the rendered buffer
    return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
}