#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <vector>
#include <cstdio>

using namespace tinygl;

struct Vertex {
    float pos[3];
    uint8_t color[3];
    float uv[2];
};

struct CubeShader {
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
        Vec4 fragment(const ShaderContext& inCtx) {
            Vec4 texColor = Vec4(1.0f, 0.0f, 1.0f, 1.0f);
            if (texture) {
                texColor = texture->sample(inCtx.varyings[0].x, inCtx.varyings[0].y);
            }
            return mix(inCtx.varyings[1], texColor, 0.5f) * tintColor;
        }
    };
    
class CubeTest : public ITestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // Load Texture
        ctx.glGenTextures(1, &m_tex); 
        ctx.glActiveTexture(GL_TEXTURE0); 
        ctx.glBindTexture(GL_TEXTURE_2D, m_tex);
        
        int width, height, nrChannels;
        // Try to load texture relative to the executable path
        unsigned char *data = stbi_load("texture/cube/assets/container.jpg", &width, &height, &nrChannels, 0);
        
        if (data) {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            ctx.glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
            printf("Loaded texture: %dx%d channels: %d\n", width, height, nrChannels);
        } else {
            printf("Failed to load texture, falling back to checkerboard.\n");
            // Generate Checkerboard Pattern
            std::vector<uint32_t> pixels(256 * 256);
            for(int i=0; i<256*256; i++) {
                int x = i % 256;
                int y = i / 256;
                bool isWhite = ((x / 32) + (y / 32)) % 2 != 0;
                pixels[i] = isWhite ? 0xFFFFFFFF : 0xFF000000;
            }
            ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        }

        // Setup Geometry (Same as original demo)
        Vertex vertices[] = {
            // Front
            {{-1,-1, 1}, {255,0,0}, {0,0}}, {{ 1,-1, 1}, {255,0,0}, {1,0}},
            {{ 1, 1, 1}, {255,0,0}, {1,1}}, {{-1, 1, 1}, {255,0,0}, {0,1}},
            // Back
            {{ 1,-1,-1}, {0,255,0}, {0,0}}, {{-1,-1,-1}, {0,255,0}, {1,0}},
            {{-1, 1,-1}, {0,255,0}, {1,1}}, {{ 1, 1,-1}, {0,255,0}, {0,1}},
            // Left
            {{-1,-1,-1}, {0,0,255}, {0,0}}, {{-1,-1, 1}, {0,0,255}, {1,0}},
            {{-1, 1, 1}, {0,0,255}, {1,1}}, {{-1, 1,-1}, {0,0,255}, {0,1}},
            // Right
            {{ 1,-1, 1}, {255,255,0}, {0,0}}, {{ 1,-1,-1}, {255,255,0}, {1,0}},
            {{ 1, 1,-1}, {255,255,0}, {1,1}}, {{ 1, 1, 1}, {255,255,0}, {0,1}},
            // Top
            {{-1, 1, 1}, {0,255,255}, {0,0}}, {{ 1, 1, 1}, {0,255,255}, {1,0}},
            {{ 1, 1,-1}, {0,255,255}, {1,1}}, {{-1, 1,-1}, {0,255,255}, {0,1}},
            // Bottom
            {{-1,-1,-1}, {255,0,255}, {0,0}}, {{ 1,-1,-1}, {255,0,255}, {1,0}},
            {{ 1,-1, 1}, {255,0,255}, {1,1}}, {{-1,-1, 1}, {255,0,255}, {0,1}}
        };

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
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteBuffers(1, &m_ebo);
        ctx.glDeleteVertexArrays(1, &m_vao);
        ctx.glDeleteTextures(1, &m_tex);
    }

    void onUpdate(float dt) override {
        m_rotationAngle += m_speed * dt;
        // Wrap at 720 degrees to maintain continuity for the X-axis rotation 
        // which runs at half speed (360 * 0.5 = 180, causing a flip if wrapped early).
        if (m_rotationAngle > 720.0f) m_rotationAngle -= 720.0f;
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_label(ctx, "Cube Controls");
        mu_label(ctx, "Rotation Speed");
        mu_slider(ctx, &m_speed, 0.0f, 360.0f);
        mu_label(ctx, "Rotation Angle");
        mu_slider(ctx, &m_rotationAngle, 0.0f, 720.0f);
        char buf[64];
        sprintf(buf, "Angle: %.1f", m_rotationAngle);
        mu_label(ctx, buf);

        mu_label(ctx, "Tint Color");
        mu_slider(ctx, &m_colorR, 0, 1);
        mu_slider(ctx, &m_colorG, 0, 1);
        mu_slider(ctx, &m_colorB, 0, 1);
        
        mu_draw_rect(ctx, mu_layout_next(ctx), mu_color(m_colorR*255, m_colorG*255, m_colorB*255, 255));
    }

    void onRender(SoftRenderContext& ctx) override {
        shader.texture = ctx.getTextureObject(m_tex);
        shader.tintColor = {m_colorR, m_colorG, m_colorB, 1.0f};

        // Calculate aspect ratio from current viewport
        const auto& vp = ctx.glGetViewport();
        float aspect = (float)vp.w / (float)vp.h;
        
        Mat4 model = Mat4::Translate(0, 0, -7.0f) * Mat4::RotateY(m_rotationAngle) * Mat4::RotateX(m_rotationAngle * 0.5f);
        Mat4 proj = Mat4::Perspective(90.0f, aspect, 0.1f, 100.0f);
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

    CubeShader shader;
};

static TestRegistrar registrar("Texture", "Cube", []() { return new CubeTest(); });
