#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>
#include <framework/camera.h>

using namespace tinygl;
using namespace framework;

struct CameraShader : ShaderBuiltins{
    Mat4 model;
    Mat4 view;
    Mat4 projection;

    void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        Vec4 pos = attribs[0]; // Position
        Vec4 color = attribs[1]; // Color

        outCtx.varyings[0] = color;
        
        // MVP transform
        Vec4 worldPos = model * pos;
        Vec4 viewPos = view * worldPos;
        gl_Position = projection * viewPos;
    }

    void fragment(const ShaderContext& inCtx) {
        gl_FragColor = inCtx.varyings[0];
    }
};

class CameraTest : public ITinyGLTestCase {
public:
    Camera m_camera;
    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    CameraShader m_shader;

    void init(SoftRenderContext& ctx) override {
        // Init Camera
        m_camera = Camera({.position = Vec4(0, 0, 3, 1)});

        // Cube Vertices (Pos + Color)
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

        ctx.glEnable(GL_DEPTH_TEST);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteVertexArrays(1, &m_vao);
    }

    void onEvent(const SDL_Event& e) override {
        m_camera.ProcessEvent(e);
    }

    void onUpdate(float dt) override {
        m_camera.Update(dt);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_label(ctx, "Controls:");
        mu_label(ctx, "Pan: MMB Drag");
        mu_label(ctx, "Orbit: Alt + LMB Drag");
        mu_label(ctx, "Zoom: Wheel or Alt+RMB");
        mu_label(ctx, "Fly: RMB Hold + WASDQE");
        mu_label(ctx, "Speed: Hold Shift");
        
        char buf[64];
        snprintf(buf, sizeof(buf), "Pos: %.1f %.1f %.1f", m_camera.position.x, m_camera.position.y, m_camera.position.z);
        mu_label(ctx, buf);
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_shader.model = Mat4::Identity();
        m_shader.view = m_camera.GetViewMatrix();
        m_shader.projection = m_camera.GetProjectionMatrix();

        ctx.glDrawArrays(m_shader, GL_TRIANGLES, 0, 36);
    }
};

static TestRegistrar registrar("Basic", "Camera", []() { return new CameraTest(); });
