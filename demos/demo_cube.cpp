#include <tinygl/application.h>
#include <vector>
#include <iostream>
#include <cstdio> // for sprintf

using namespace tinygl;

// ==========================================
// Shader & Vertex Definitions
// ==========================================
struct Vertex {
    float pos[3];
    uint8_t color[3];
    uint8_t padding; 
    float uv[2];
};

struct CubeShader : public ShaderBase {
    TextureObject* texture = nullptr;
    SimdMat4 mvp; 
    Vec4 tintColor = {1.0f, 1.0f, 1.0f, 1.0f};

    // Vertex Shader
    inline Vec4 vertex(const Vec4* attribs, ShaderContext& outCtx) {
        outCtx.varyings[0] = attribs[2]; // UV
        outCtx.varyings[1] = attribs[1]; // Color

        float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, 1.0f};
        Simd4f pos = Simd4f::load(posArr);
        Simd4f res = mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        return Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    }

    // Fragment Shader
    inline Vec4 fragment(const ShaderContext& inCtx) {
        Vec4 texColor = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
        if (texture) {
            texColor = texture->sampleNearestFast(inCtx.varyings[0].x, inCtx.varyings[0].y);
        }
        // Combine interpolated vertex color, texture color, and global tint
        return mix(inCtx.varyings[0], texColor, 0.5f) * tintColor;
    }
};

// ==========================================
// Cube Application
// ==========================================
class CubeApp : public Application {
public:
    CubeApp() : Application({ "TinyGL Cube Demo", 800, 600, true }) {}

protected:
    bool onInit() override {
        auto& ctx = getContext();

        // 1. Generate Texture
        ctx.glGenTextures(1, &m_tex); 
        ctx.glActiveTexture(GL_TEXTURE0); 
        ctx.glBindTexture(GL_TEXTURE_2D, m_tex);
        
        // Generate Checkerboard Pattern
        std::vector<uint32_t> pixels(256 * 256);
        for(int i=0; i<256*256; i++) {
            int x = i % 256;
            int y = i / 256;
            bool isWhite = ((x / 32) + (y / 32)) % 2 != 0;
            pixels[i] = isWhite ? 0xFFFFFFFF : 0xFF000000;
        }
        ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // 2. Setup Cube Geometry
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

        // Indices
        for(int i=0; i<6; i++) {
            uint32_t base = i*4;
            m_cubeIndices[i*6+0]=base; m_cubeIndices[i*6+1]=base+1; m_cubeIndices[i*6+2]=base+2;
            m_cubeIndices[i*6+3]=base+2; m_cubeIndices[i*6+4]=base+3; m_cubeIndices[i*6+5]=base;
        }

        ctx.glGenVertexArrays(1, &m_vao); 
        ctx.glBindVertexArray(m_vao);
        ctx.glGenBuffers(1, &m_vbo); 
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        ctx.glGenBuffers(1, &m_ebo); 
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_cubeIndices), m_cubeIndices, GL_STATIC_DRAW);

        GLsizei stride = sizeof(Vertex);
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)offsetof(Vertex, pos));
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, true, stride, (void*)offsetof(Vertex, color));
        ctx.glEnableVertexAttribArray(1);
        ctx.glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)offsetof(Vertex, uv));
        ctx.glEnableVertexAttribArray(2);

        return true;
    }

    void onEvent(const SDL_Event& event) override {
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_SPACE) {
            m_rotationAngle = 0.0f;
        }
    }

    void onGUI() override {
        mu_Context* ctx = getUIContext();
        if (mu_begin_window(ctx, "Settings", mu_rect(10, 10, 200, 240))) {
            mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
            
            mu_label(ctx, "Rotation Speed (deg/s):");
            mu_slider_ex(ctx, &m_speed, 0, 360, 0, "%.0f", MU_OPT_ALIGNCENTER);
            
            char buf[64];
            snprintf(buf, sizeof(buf), "Current Angle: %.1f", m_rotationAngle);
            mu_label(ctx, buf);
            
            mu_label(ctx, "Tint Color (RGB):");
            mu_layout_row(ctx, 3, (int[]) { 60, 60, 60 }, 0);
            mu_slider(ctx, &m_colorR, 0, 1);
            mu_slider(ctx, &m_colorG, 0, 1);
            mu_slider(ctx, &m_colorB, 0, 1);
            
            // Revert layout
            mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
            
            // Preview color
            mu_Rect r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color(m_colorR*255, m_colorG*255, m_colorB*255, 255));

            mu_end_window(ctx);
        }
    }

    void onUpdate(float dt) override {
        m_rotationAngle += m_speed * dt;
        if (m_rotationAngle > 360.0f) m_rotationAngle -= 360.0f;
    }

    void onRender() override {
        auto& ctx = getContext();
        ctx.glClear(BufferType::COLOR | BufferType::DEPTH);

        CubeShader shader;
        shader.texture = ctx.getTextureObject(m_tex);
        shader.tintColor = {m_colorR, m_colorG, m_colorB, 1.0f};

        Mat4 model = Mat4::Translate(0, 0, -3.0f) * Mat4::RotateY(m_rotationAngle) * Mat4::RotateX(m_rotationAngle * 0.5f);
        Mat4 proj = Mat4::Perspective(90.0f, (float)getWidth()/getHeight(), 0.1f, 100.0f);
        Mat4 mvp = proj * Mat4::Identity() * model;
        shader.mvp.load(mvp);

        ctx.glDrawElements(shader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0);
    }

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0, m_tex = 0;
    uint32_t m_cubeIndices[36];
    float m_rotationAngle = 0.0f;
    float m_speed = 45.0f;
    float m_colorR = 1.0f;
    float m_colorG = 1.0f;
    float m_colorB = 1.0f;
};

// ==========================================
// Main Entry Point
// ==========================================
int main(int argc, char** argv) {
    CubeApp app;
    app.run();
    return 0;
}
