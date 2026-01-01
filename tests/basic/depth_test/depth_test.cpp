#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <cstdio>

using namespace tinygl;

struct DepthShader {
    SimdMat4 mvp;

    // Vertex Shader
    inline Vec4 vertex(const Vec4* attribs, ShaderContext& outCtx) {
        outCtx.varyings[0] = attribs[1]; // Color
        
        float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, 1.0f};
        Simd4f pos = Simd4f::load(posArr);
        Simd4f res = mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        return Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    }

    // Fragment Shader: Output Color
    Vec4 fragment(const ShaderContext& inCtx) {
        return inCtx.varyings[0];
    }
};

class DepthTest : public ITestCase {
public:
    void init(SoftRenderContext& ctx) override {
        float vertices[] = {
            // Front face (Red)
            -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f,
            
            // Back face (Green)
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 0.0f,

            // Top (Blue)
            -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f,

            // Bottom (Yellow)
            -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  1.0f, 1.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  1.0f, 1.0f, 0.0f,

            // Right (Cyan)
             0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f,

            // Left (Magenta)
            -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  1.0f, 0.0f, 1.0f,
            -0.5f,  0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f,
        };

        ctx.glGenVertexArrays(1, &m_vao);
        ctx.glBindVertexArray(m_vao);
        ctx.glGenBuffers(1, &m_vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Pos
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 6*sizeof(float), (void*)0);
        ctx.glEnableVertexAttribArray(0);
        // Color
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, 6*sizeof(float), (void*)(3*sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteVertexArrays(1, &m_vao);
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteBuffers(1, &m_ebo);
    }

    void onUpdate(float dt) override {
        m_rotationAngle += 45.0f * dt;
        if (m_rotationAngle > 720.0f) m_rotationAngle -= 720.0f;
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
        mu_label(ctx, "Depth Settings");
        
        if (mu_header_ex(ctx, "Depth Test Enable", MU_OPT_EXPANDED)) {
             int enable = m_depthTestEnabled ? 1 : 0;
             if (mu_checkbox(ctx, "Enabled", &enable)) {
                 m_depthTestEnabled = enable != 0;
             }
        }

        if (mu_header_ex(ctx, "Depth Function", MU_OPT_EXPANDED)) {
            // Radio buttons for functions
            if (mu_button(ctx, m_depthFunc == GL_NEVER ? "[NEVER]" : "NEVER")) m_depthFunc = GL_NEVER;
            if (mu_button(ctx, m_depthFunc == GL_LESS ? "[LESS]" : "LESS")) m_depthFunc = GL_LESS;
            if (mu_button(ctx, m_depthFunc == GL_EQUAL ? "[EQUAL]" : "EQUAL")) m_depthFunc = GL_EQUAL;
            if (mu_button(ctx, m_depthFunc == GL_LEQUAL ? "[LEQUAL]" : "LEQUAL")) m_depthFunc = GL_LEQUAL;
            if (mu_button(ctx, m_depthFunc == GL_GREATER ? "[GREATER]" : "GREATER")) m_depthFunc = GL_GREATER;
            if (mu_button(ctx, m_depthFunc == GL_NOTEQUAL ? "[NOTEQUAL]" : "NOTEQUAL")) m_depthFunc = GL_NOTEQUAL;
            if (mu_button(ctx, m_depthFunc == GL_GEQUAL ? "[GEQUAL]" : "GEQUAL")) m_depthFunc = GL_GEQUAL;
            if (mu_button(ctx, m_depthFunc == GL_ALWAYS ? "[ALWAYS]" : "ALWAYS")) m_depthFunc = GL_ALWAYS;
        }
        
        mu_label(ctx, "Cull Face Settings");
        if (mu_header_ex(ctx, "Cull Face Enable", MU_OPT_EXPANDED)) {
             int enable = m_cullFaceEnabled ? 1 : 0;
             if (mu_checkbox(ctx, "Enabled", &enable)) {
                 m_cullFaceEnabled = enable != 0;
             }
        }
        
        if (m_cullFaceEnabled) {
            if (mu_header_ex(ctx, "Cull Mode", MU_OPT_EXPANDED)) {
                if (mu_button(ctx, m_cullFaceMode == GL_BACK ? "[BACK]" : "BACK")) m_cullFaceMode = GL_BACK;
                if (mu_button(ctx, m_cullFaceMode == GL_FRONT ? "[FRONT]" : "FRONT")) m_cullFaceMode = GL_FRONT;
                if (mu_button(ctx, m_cullFaceMode == GL_FRONT_AND_BACK ? "[FRONT_AND_BACK]" : "FRONT_AND_BACK")) m_cullFaceMode = GL_FRONT_AND_BACK;
            }
            if (mu_header_ex(ctx, "Front Face", MU_OPT_EXPANDED)) {
                if (mu_button(ctx, m_frontFace == GL_CCW ? "[CCW]" : "CCW")) m_frontFace = GL_CCW;
                if (mu_button(ctx, m_frontFace == GL_CW ? "[CW]" : "CW")) m_frontFace = GL_CW;
            }
        }

        mu_label(ctx, "Description:");
        mu_label(ctx, "Rotating cube with per-face colors.");
        mu_label(ctx, "Toggle depth test to see the difference.");
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (m_depthTestEnabled) {
            ctx.glEnable(GL_DEPTH_TEST);
        } else {
            ctx.glDisable(GL_DEPTH_TEST);
        }
        ctx.glDepthFunc(m_depthFunc);

        if (m_cullFaceEnabled) {
            ctx.glEnable(GL_CULL_FACE);
        } else {
            ctx.glDisable(GL_CULL_FACE);
        }
        ctx.glCullFace(m_cullFaceMode);
        ctx.glFrontFace(m_frontFace);

        // Calculate aspect ratio from current viewport
        const auto& vp = ctx.glGetViewport();
        float aspect = (float)vp.w / (float)vp.h;
        
        Mat4 model = Mat4::Translate(0, 0, -5.0f) * Mat4::RotateY(m_rotationAngle) * Mat4::RotateX(m_rotationAngle * 0.5f);
        Mat4 proj = Mat4::Perspective(45.0f, aspect, 0.1f, 100.0f);
        Mat4 mvp = proj * Mat4::Identity() * model;
        shader.mvp.load(mvp);
        
        ctx.glDrawArrays(shader, GL_TRIANGLES, 0, 36);
    }

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    bool m_depthTestEnabled = true;
    GLenum m_depthFunc = GL_LESS;
    bool m_cullFaceEnabled = false;
    GLenum m_cullFaceMode = GL_BACK;
    GLenum m_frontFace = GL_CCW;
    float m_rotationAngle = 0.0f;
    DepthShader shader;
};

static TestRegistrar registrar("Basic", "DepthTest", []() { return new DepthTest(); });
