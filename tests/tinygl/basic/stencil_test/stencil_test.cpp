#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>
#include <framework/geometry.h>
#include <framework/camera.h>
#include <cstdio>
#include <vector>

using namespace tinygl;
using namespace framework;

struct StencilShader : ShaderBuiltins {
    SimdMat4 mvp;
    Vec4 color;
    bool useTexture = false;
    TextureObject* texture = nullptr;

    // Vertex Shader
    inline void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        // attribs[0] is Position (x, y, z, w)
        float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, attribs[0].w};
        Simd4f pos = Simd4f::load(posArr);
        Simd4f res = mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        
        // attribs[1] is UV. Store in varying 0.
        outCtx.varyings[0] = Vec4(attribs[1].x, attribs[1].y, 0.0f, 0.0f);

        gl_Position = Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    }

    // Fragment Shader
    void fragment(const ShaderContext& inCtx) {
        if (useTexture && texture) {
            Vec4 uv = inCtx.varyings[0];
            gl_FragColor = texture->sample(uv.x, uv.y);
        } else {
            gl_FragColor = color;
        }
    }
};

class StencilTest : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // Setup Camera
        camera = Camera({.position = Vec4(0.0f, 2.0f, 5.0f, 1.0f)});

        // Create Cube Geometry (Half extend 0.5 -> Size 1.0)
        cubeGeo = geometry::createCube(0.5f);

        // VAO/VBO/EBO
        ctx.glGenVertexArrays(1, &m_vao);
        ctx.glBindVertexArray(m_vao);
        
        ctx.glGenBuffers(1, &m_vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, cubeGeo.allAttributes.size() * sizeof(float), cubeGeo.allAttributes.data(), GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &m_ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, cubeGeo.indices.size() * sizeof(uint32_t), cubeGeo.indices.data(), GL_STATIC_DRAW);

        // Attributes: Pos(4), Norm(3), Tan(3), Bitan(3), UV(2)
        // Stride = 15 * sizeof(float)
        GLsizei stride = 15 * sizeof(float);
        
        // Pos (loc 0) - 4 components
        ctx.glVertexAttribPointer(0, 4, GL_FLOAT, false, stride, (void*)0);
        ctx.glEnableVertexAttribArray(0);
        
        // UV (loc 1) - Offset is 13 floats (4+3+3+3)
        ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (void*)(13 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);

        // Create Texture (Checkerboard)
        createCheckerboardTexture(ctx);
        
        ctx.glEnable(GL_DEPTH_TEST);
    }

    void createCheckerboardTexture(SoftRenderContext& ctx) {
        int w = 64, h = 64;
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // 8x8 squares
                bool white = ((x / 8) + (y / 8)) % 2 == 0;
                pixels[y * w + x] = white ? 0xFFFFFFFF : 0xFF000000; // White or Black
            }
        }

        ctx.glGenTextures(1, &m_texture);
        ctx.glBindTexture(GL_TEXTURE_2D, m_texture);
        ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteVertexArrays(1, &m_vao);
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteBuffers(1, &m_ebo);
        ctx.glDeleteTextures(1, &m_texture);
        ctx.glDisable(GL_STENCIL_TEST);
    }

    void onEvent(const SDL_Event& e) override {
        camera.ProcessEvent(e);
    }

    void onUpdate(float dt) override {
        camera.Update(dt);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
        mu_label(ctx, "Stencil Test (Outline)");
        
        int check = m_stencilEnabled ? 1 : 0;
        if (mu_checkbox(ctx, "Enable Stencil Outline", &check)) {
            m_stencilEnabled = check != 0;
        }
        
        mu_slider(ctx, &m_outlineScale, 1.01f, 1.2f);
        mu_label(ctx, "WASD + Mouse(RMB) to fly.");
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        ctx.glClearStencil(0);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        Mat4 view = camera.GetViewMatrix();
        Mat4 proj = camera.GetProjectionMatrix();

        // Define objects positions (Using Vec4 or individual floats as Vec3 is missing)
        Vec4 pos1(-1.0f, 0.0f, -1.0f, 1.0f);
        Vec4 pos2( 1.0f, 0.0f, -2.0f, 1.0f);
        
        // 1st Pass: Draw Objects normally, write to Stencil
        if (m_stencilEnabled) {
            ctx.glEnable(GL_STENCIL_TEST);
            ctx.glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
            ctx.glStencilFunc(GL_ALWAYS, 1, 0xFF);
            ctx.glStencilMask(0xFF);
        } else {
            ctx.glDisable(GL_STENCIL_TEST);
        }

        shader.useTexture = true;
        shader.texture = ctx.getTextureObject(m_texture);
        
        ctx.glBindVertexArray(m_vao);
        ctx.glBindTexture(GL_TEXTURE_2D, m_texture);

        // Draw Cube 1
        Mat4 model1 = Mat4::Translate(pos1.x, pos1.y, pos1.z);
        shader.mvp.load(proj * view * model1);
        ctx.glDrawElements(shader, GL_TRIANGLES, cubeGeo.indices.size(), GL_UNSIGNED_INT, 0);

        // Draw Cube 2
        Mat4 model2 = Mat4::Translate(pos2.x, pos2.y, pos2.z);
        // Rotate cube 2 for variety
        model2 = model2 * Mat4::RotateY(45.0f);
        shader.mvp.load(proj * view * model2);
        ctx.glDrawElements(shader, GL_TRIANGLES, cubeGeo.indices.size(), GL_UNSIGNED_INT, 0);

        // 2nd Pass: Draw Outlines
        if (m_stencilEnabled) {
            ctx.glStencilFunc(GL_NOTEQUAL, 1, 0xFF);
            ctx.glStencilMask(0x00); // Do not write to stencil
            ctx.glDisable(GL_DEPTH_TEST); 
            
            shader.useTexture = false;
            shader.color = Vec4(1.0f, 0.5f, 0.0f, 1.0f); // Orange Outline

            float s = m_outlineScale;
            Mat4 scale = Mat4::Scale(s, s, s);

            // Draw Cube 1 Outline
            shader.mvp.load(proj * view * model1 * scale);
            ctx.glDrawElements(shader, GL_TRIANGLES, cubeGeo.indices.size(), GL_UNSIGNED_INT, 0);

            // Draw Cube 2 Outline
            shader.mvp.load(proj * view * model2 * scale);
            ctx.glDrawElements(shader, GL_TRIANGLES, cubeGeo.indices.size(), GL_UNSIGNED_INT, 0);
            
            ctx.glStencilMask(0xFF);
            ctx.glEnable(GL_DEPTH_TEST); 
        }
    }

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLuint m_texture = 0;
    Geometry cubeGeo;
    Camera camera;
    StencilShader shader;
    
    // UI state
    bool m_stencilEnabled = true;
    float m_outlineScale = 1.1f;
};

static TestRegistrar registrar("Basic", "StencilTest", []() -> ITinyGLTestCase* { return new StencilTest(); });


