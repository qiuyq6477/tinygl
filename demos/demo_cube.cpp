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
GLuint g_vao, g_vbo, g_ebo;
GLuint g_tex;

float g_rotationAngle = 0.0f; // For animating the cube


// Define a vertex structure for clarity
struct Vertex {
    float pos[3];
    uint8_t color[3];
    uint8_t padding; // Align to 4 bytes
    float uv[2];
};

void vc_init(void) {
    // 1. Initialize SoftRenderContext
    g_ctx = std::unique_ptr<SoftRenderContext>(new SoftRenderContext(DEMO_WIDTH, DEMO_HEIGHT));

    // 2. Create and configure Shader Program
    g_progID = g_ctx->glCreateProgram();
    g_uMVP = g_ctx->glGetUniformLocation(g_progID, "uMVP");
    g_uTex = g_ctx->glGetUniformLocation(g_progID, "uTex");

    g_ctx->setVertexShader(g_progID, [&](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        outCtx.varyings[0] = attribs[2]; // UV
        outCtx.varyings[1] = attribs[1]; // Color
        
        Mat4 mvp = Mat4::Identity();
        if (auto* p = g_ctx->getCurrentProgram()) {
             if (p->uniforms.count(g_uMVP)) std::memcpy(mvp.m, p->uniforms[g_uMVP].data.mat, 16*sizeof(float));
        }
        Vec4 pos = attribs[0]; 
        pos.w = 1.0f;
        return mvp * pos; // MVP 变换
    });

    g_ctx->setFragmentShader(g_progID, [&](const ShaderContext& inCtx) -> Vec4 {
        int unit = 0;
        if (auto* p = g_ctx->getCurrentProgram()) if (p->uniforms.count(g_uTex)) unit = p->uniforms[g_uTex].data.i;
        Vec4 texColor = {1,1,1,1};
        if (auto* tex = g_ctx->getTexture(unit)) texColor = tex->sample(inCtx.varyings[0].x, inCtx.varyings[0].y, inCtx.lod);
        // 混合顶点颜色和纹理
        return inCtx.varyings[1] * texColor;
    });

    // 3. Generate Texture (white/blue checkerboard)
    g_ctx->glGenTextures(1, &g_tex); 
    g_ctx->glActiveTexture(GL_TEXTURE0); 
    g_ctx->glBindTexture(GL_TEXTURE_2D, g_tex);
    std::vector<uint32_t> pixels(256 * 256);
    for(int i=0; i<256*256; i++) 
        pixels[i] = (((i%256/32)+(i/256/32))%2) ? COLOR_WHITE : 0xFF000000; // White / Black
    g_ctx->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    g_ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    g_ctx->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    
    // 4. Build Cube Data with uint8_t for colors
    Vertex vertices[] = {
        //   x,     y,    z,        r,  g,  b,      uv
        // 1. Front Face
        {{-1.0f, -1.0f,  1.0f}, {255, 0, 0}, 0, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {255, 0, 0}, 0, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {255, 0, 0}, 0, {1.0f, 1.0f}},
        {{-1.0f,  1.0f,  1.0f}, {255, 0, 0}, 0, {0.0f, 1.0f}},
        // 2. Back Face
        {{ 1.0f, -1.0f, -1.0f}, {0, 255, 0}, 0, {0.0f, 0.0f}},
        {{-1.0f, -1.0f, -1.0f}, {0, 255, 0}, 0, {1.0f, 0.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0, 255, 0}, 0, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0, 255, 0}, 0, {0.0f, 1.0f}},
        // 3. Left Face
        {{-1.0f, -1.0f, -1.0f}, {0, 0, 255}, 0, {0.0f, 0.0f}},
        {{-1.0f, -1.0f,  1.0f}, {0, 0, 255}, 0, {1.0f, 0.0f}},
        {{-1.0f,  1.0f,  1.0f}, {0, 0, 255}, 0, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0, 0, 255}, 0, {0.0f, 1.0f}},
        // 4. Right Face
        {{ 1.0f, -1.0f,  1.0f}, {255, 255, 0}, 0, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {255, 255, 0}, 0, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {255, 255, 0}, 0, {1.0f, 1.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {255, 255, 0}, 0, {0.0f, 1.0f}},
        // 5. Top Face
        {{-1.0f,  1.0f,  1.0f}, {0, 255, 255}, 0, {0.0f, 0.0f}},
        {{ 1.0f,  1.0f,  1.0f}, {0, 255, 255}, 0, {1.0f, 0.0f}},
        {{ 1.0f,  1.0f, -1.0f}, {0, 255, 255}, 0, {1.0f, 1.0f}},
        {{-1.0f,  1.0f, -1.0f}, {0, 255, 255}, 0, {0.0f, 1.0f}},
        // 6. Bottom Face
        {{-1.0f, -1.0f, -1.0f}, {255, 0, 255}, 0, {0.0f, 0.0f}},
        {{ 1.0f, -1.0f, -1.0f}, {255, 0, 255}, 0, {1.0f, 0.0f}},
        {{ 1.0f, -1.0f,  1.0f}, {255, 0, 255}, 0, {1.0f, 1.0f}},
        {{-1.0f, -1.0f,  1.0f}, {255, 0, 255}, 0, {0.0f, 1.0f}}
    };

    uint32_t cubeIndices[36];
    for(int i=0; i<6; i++) {
        uint32_t base = i * 4;
        cubeIndices[i*6+0] = base + 0;
        cubeIndices[i*6+1] = base + 1;
        cubeIndices[i*6+2] = base + 2;
        cubeIndices[i*6+3] = base + 2;
        cubeIndices[i*6+4] = base + 3;
        cubeIndices[i*6+5] = base + 0;
    }

    // Upload Data
    g_ctx->glGenVertexArrays(1, &g_vao); g_ctx->glBindVertexArray(g_vao);
    g_ctx->glGenBuffers(1, &g_vbo); g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    g_ctx->glGenBuffers(1, &g_ebo); g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // Attributes
    GLsizei stride = sizeof(Vertex);
    // Position attribute (3 floats)
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)offsetof(Vertex, pos));
    g_ctx->glEnableVertexAttribArray(0);
    // Color attribute (3 uint8_t), normalized
    g_ctx->glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, true, stride, (void*)offsetof(Vertex, color));
    g_ctx->glEnableVertexAttribArray(1);
    // UV attribute (2 floats)
    g_ctx->glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)offsetof(Vertex, uv));
    g_ctx->glEnableVertexAttribArray(2);

    g_ctx->glUseProgram(g_progID);
    g_ctx->glUniform1i(g_uTex, 0);

    // Initial MVP is set in render loop for animation
}

void vc_input(SDL_Event *event) {
    // Basic input: space to reset rotation (optional)
    if (event->type == SDL_KEYDOWN) {
        if (event->key.keysym.sym == SDLK_SPACE) {
            g_rotationAngle = 0.0f;
        }
    }
}

Olivec_Canvas vc_render(float dt) {
    if (!g_ctx) { // Should not happen if vc_init is called
        return olivec_canvas(nullptr, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
    }

    g_ctx->glClear(COLOR_BLACK | BufferType::DEPTH); // Clear color and depth buffer

    g_rotationAngle += 45.0f * dt; // Rotate 45 degrees per second

    // Calculate MVP matrix
    // Model: Apply rotation based on g_rotationAngle and translation
    Mat4 model = Mat4::Translate(0, 0, -3.0f); // Translate back to be visible
    model = model * Mat4::RotateY(g_rotationAngle); // Rotate around Y-axis
    model = model * Mat4::RotateX(g_rotationAngle * 0.5f); // Rotate around X-axis
    
    Mat4 view = Mat4::Identity(); // Simple Identity View
    float aspect = (float)DEMO_WIDTH / (float)DEMO_HEIGHT;
    Mat4 proj = Mat4::Perspective(90.0f, aspect, 0.1f, 100.0f); // 90 deg FOV

    Mat4 mvp = proj * view * model; // P * V * M

    // Update MVP uniform
    g_ctx->glUniformMatrix4fv(g_uMVP, 1, false, mvp.m);

    // Draw the cube
    g_ctx->glDrawElements(GL_TRIANGLES, 36, 0, (void*)0);

    // Return the rendered buffer
    return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
}
