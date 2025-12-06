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

    g_ctx->setVertexShader(g_progID, [&](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        outCtx.varyings[0] = attribs[1]; // Color
        Mat4 mvp = Mat4::Identity();
        if (auto* p = g_ctx->getCurrentProgram()) {
             if (p->uniforms.count(g_uMVP)) std::memcpy(mvp.m, p->uniforms[g_uMVP].data.mat, 16*sizeof(float));
        }
        Vec4 pos = attribs[0];
        pos.w = 1.0f;
        return mvp * pos;
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
}

void vc_input(SDL_Event *event) {
    (void)event;
}

Olivec_Canvas vc_render(float dt) {
    (void)dt;
    g_ctx->glClear(BufferType::COLOR | BufferType::DEPTH);
    
    // Draw Lines
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo_lines);
    g_ctx->glDrawElements(GL_LINES, 8, GL_UNSIGNED_SHORT, (void*)0);

    // Draw Points
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo_points);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * 4, (uint16_t[]){0,1,2,3}, GL_STATIC_DRAW); // Re-buffer data for clarity
    g_ctx->glDrawElements(GL_POINTS, 4, GL_UNSIGNED_SHORT, (void*)0);

    return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
}