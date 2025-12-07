#include "../vc/tinygl.h" 
#include "../vc/vc.cpp"   

#define DEMO_WIDTH 800
#define DEMO_HEIGHT 600

std::unique_ptr<SoftRenderContext> g_ctx;
GLuint g_progID;
GLint g_uMVP;
GLuint g_vao, g_vbo, g_ebo_lines, g_ebo_points;

void vc_init(void) {
    g_ctx = std::unique_ptr<SoftRenderContext>(new SoftRenderContext(DEMO_WIDTH, DEMO_HEIGHT));
    g_ctx->glClearColor(0.1f, 0.1f, 0.1f, 1.0f); // Set clear color to a dark grey

    g_progID = g_ctx->glCreateProgram();
    g_uMVP = g_ctx->glGetUniformLocation(g_progID, "uMVP");

    // Vertex Shader: Use optimized currMVP access
    SoftRenderContext* ctx = g_ctx.get();
    g_ctx->setVertexShader(g_progID, [ctx](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        outCtx.varyings[0] = attribs[1]; // Color
        
        Vec4 pos = attribs[0]; 
        pos.w = 1.0f;
        return ctx->currMVP * pos; 
    });

    g_ctx->setFragmentShader(g_progID, [&](const ShaderContext& inCtx) -> Vec4 {
        return inCtx.varyings[0]; // Return interpolated color
    });

    float vertices[] = {
        // Positions         // Colors
        -0.5f,  0.5f, 0.0f,   1.0f, 0.0f, 0.0f, // Top-left (Red)
         0.5f,  0.5f, 0.0f,   0.0f, 1.0f, 0.0f, // Top-right (Green)
         0.5f, -0.5f, 0.0f,   0.0f, 0.0f, 1.0f, // Bottom-right (Blue)
        -0.5f, -0.5f, 0.0f,   1.0f, 1.0f, 0.0f  // Bottom-left (Yellow)
    };
    
    uint16_t line_indices[] = { 0, 1, 1, 2, 2, 3, 3, 0 };


    g_ctx->glGenVertexArrays(1, &g_vao);
    g_ctx->glBindVertexArray(g_vao);

    g_ctx->glGenBuffers(1, &g_vbo);
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO for lines
    g_ctx->glGenBuffers(1, &g_ebo_lines);
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo_lines);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(line_indices), line_indices, GL_STATIC_DRAW);

    // EBO for points (can be separate or part of the same VAO state)
    g_ctx->glGenBuffers(1, &g_ebo_points);
    // Note: The VAO stores the LAST element buffer that was bound. We will re-bind before drawing.

    GLsizei stride = 6 * sizeof(float);
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
    g_ctx->glEnableVertexAttribArray(0);
    g_ctx->glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
    g_ctx->glEnableVertexAttribArray(1);

    g_ctx->glUseProgram(g_progID);
    
    Mat4 proj = Mat4::Perspective(60.0f, (float)DEMO_WIDTH / DEMO_HEIGHT, 0.1f, 100.0f);
    Mat4 view = Mat4::Translate(0.0f, 0.0f, -3.0f);
    Mat4 mvp = proj * view;
    g_ctx->glUniformMatrix4fv(g_uMVP, 1, false, mvp.m);
    g_ctx->currMVP = mvp; // Update fast-path MVP
}

void vc_input(SDL_Event *event) {
    (void)event;
}

Olivec_Canvas vc_render(float dt, void* pixels) {
    if (!g_ctx) return olivec_canvas(nullptr, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);

    g_ctx->setExternalBuffer((uint32_t*)pixels);
    g_ctx->glClear(BufferType::COLOR | BufferType::DEPTH);

    Mat4 view = Mat4::Identity();
    float aspect = (float)DEMO_WIDTH / (float)DEMO_HEIGHT;
    Mat4 proj = Mat4::Perspective(90.0f, aspect, 0.1f, 100.0f);

    // 1. Draw Points (Left)
    Mat4 modelPoints = Mat4::Translate(-2.0f, 0, -3.0f);
    g_ctx->currMVP = proj * view * modelPoints;
    // Points logic: just vertices
    g_ctx->glDrawArrays(GL_POINTS, 0, 3);

    // 2. Draw Lines (Center)
    Mat4 modelLines = Mat4::Translate(0, 0, -3.0f);
    g_ctx->currMVP = proj * view * modelLines;
    g_ctx->glDrawArrays(GL_LINES, 0, 4); // 2 lines, 4 vertices

    // 3. Draw Triangles (Right)
    Mat4 modelTris = Mat4::Translate(2.0f, 0, -3.0f);
    g_ctx->currMVP = proj * view * modelTris;
    g_ctx->glDrawArrays(GL_TRIANGLES, 0, 3);

    return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
}