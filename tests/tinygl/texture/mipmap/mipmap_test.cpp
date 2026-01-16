#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>
#include <vector>
#include <string>

using namespace tinygl;
using namespace framework;

struct MipmapShader : public ShaderBuiltins {
    TextureObject* texture = nullptr;
    SimdMat4 mvp;

    void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        // attribs[0]: Pos, attribs[1]: UV
        outCtx.varyings[0] = attribs[1]; // Pass UV to fragment shader
        
        // Transform position using SIMD
        float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, 1.0f};
        Simd4f pos = Simd4f::load(posArr);
        Simd4f res = mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        gl_Position = Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    }

    void fragment(const ShaderContext& inCtx) {
        if (texture) {
            gl_FragColor = texture->sample(inCtx.varyings[0].x, inCtx.varyings[0].y, inCtx.rho);
        }
        else {
            gl_FragColor = {1.0f, 0.0f, 1.0f, 1.0f}; // Error magenta
        }
    }
};

class MipmapTest : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        m_softCtx = &ctx;

        // 1. Generate high-frequency checkerboard texture
        generateCheckerboard(ctx);

        // 2. Setup ground plane geometry
        float size = 100.0f;
        float uvScale = 50.0f;
        float vertices[] = {
            // Pos (x, y, z)      // UV (u, v)
            -size, 0.0f, -size,   0.0f, 0.0f,
             size, 0.0f, -size,   uvScale, 0.0f,
             size, 0.0f,  size,   uvScale, uvScale,
            -size, 0.0f,  size,   0.0f, uvScale
        };
        uint32_t indices[] = { 0, 2, 1, 0, 3, 2 };

        ctx.glGenVertexArrays(1, &m_vao);
        ctx.glBindVertexArray(m_vao);
        ctx.glGenBuffers(1, &m_vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        ctx.glGenBuffers(1, &m_ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        GLsizei stride = 5 * sizeof(float);
        // Attribute 0: Position
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
        ctx.glEnableVertexAttribArray(0);
        // Attribute 1: UV
        ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteBuffers(1, &m_ebo);
        ctx.glDeleteVertexArrays(1, &m_vao);
        ctx.glDeleteTextures(1, &m_tex);
        m_softCtx = nullptr;
    }

    void onGui(mu_Context* ui, const Rect& rect) override {
        mu_label(ui, "Texture Filter (Minification)");
        
        if (mu_header_ex(ui, "Min Filter", MU_OPT_EXPANDED)) {
            if (mu_button(ui, m_minFilter == GL_NEAREST ? "[Nearest]" : "Nearest")) 
                setFilter(*m_softCtx, GL_NEAREST, m_magFilter);
            if (mu_button(ui, m_minFilter == GL_LINEAR ? "[Linear]" : "Linear")) 
                setFilter(*m_softCtx, GL_LINEAR, m_magFilter);
            if (mu_button(ui, m_minFilter == GL_NEAREST_MIPMAP_NEAREST ? "[Nearest-Mip-Nearest]" : "Nearest-Mip-Nearest")) 
                setFilter(*m_softCtx, GL_NEAREST_MIPMAP_NEAREST, m_magFilter);
            if (mu_button(ui, m_minFilter == GL_LINEAR_MIPMAP_NEAREST ? "[Linear-Mip-Nearest]" : "Linear-Mip-Nearest")) 
                setFilter(*m_softCtx, GL_LINEAR_MIPMAP_NEAREST, m_magFilter);
            if (mu_button(ui, m_minFilter == GL_NEAREST_MIPMAP_LINEAR ? "[Nearest-Mip-Linear]" : "Nearest-Mip-Linear")) 
                setFilter(*m_softCtx, GL_NEAREST_MIPMAP_LINEAR, m_magFilter);
            if (mu_button(ui, m_minFilter == GL_LINEAR_MIPMAP_LINEAR ? "[Linear-Mip-Linear]" : "Linear-Mip-Linear")) 
                setFilter(*m_softCtx, GL_LINEAR_MIPMAP_LINEAR, m_magFilter);
        }

        mu_label(ui, "Camera Controls");
        mu_label(ui, "Height");
        mu_slider(ui, &m_camHeight, 1.0f, 50.0f);
        mu_label(ui, "Distance");
        mu_slider(ui, &m_camDist, 1.0f, 100.0f);
    }

    void onRender(SoftRenderContext& ctx) override {
        const auto& vp = ctx.glGetViewport();
        float aspect = (float)vp.w / (float)vp.h;

        Mat4 proj = Mat4::Perspective(60.0f, aspect, 0.1f, 500.0f);
        
        Vec4 camPos(0.0f, m_camHeight, m_camDist, 1.0f);
        Vec4 target(0.0f, 0.0f, -20.0f, 1.0f);
        Vec4 up(0.0f, 1.0f, 0.0f, 0.0f);
        Mat4 view = Mat4::LookAt(camPos, target, up);
        
        m_shader.mvp.load(proj * view);
        m_shader.texture = ctx.getTextureObject(m_tex);

        ctx.glDrawElements(m_shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, (void*)0);
    }

private:
    void generateCheckerboard(SoftRenderContext& ctx) {
        ctx.glGenTextures(1, &m_tex);
        ctx.glBindTexture(GL_TEXTURE_2D, m_tex);
        
        int w = 256, h = 256;
        std::vector<uint32_t> pixels(w * h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                bool white = ((x / 32) + (y / 32)) % 2 == 0;
                pixels[y * w + x] = white ? 0xFFFFFFFF : 0xFF404040; 
            }
        }

        ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        ctx.glGenerateMipmap(GL_TEXTURE_2D);
        
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        setFilter(ctx, GL_LINEAR, GL_LINEAR);
    }

    void setFilter(SoftRenderContext& ctx, GLint min, GLint mag) {
        m_minFilter = min;
        m_magFilter = mag;
        ctx.glActiveTexture(GL_TEXTURE0);
        ctx.glBindTexture(GL_TEXTURE_2D, m_tex);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag);
    }

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0, m_tex = 0;
    MipmapShader m_shader;
    SoftRenderContext* m_softCtx = nullptr;
    
    // UI State
    GLint m_minFilter = GL_LINEAR;
    GLint m_magFilter = GL_LINEAR;
    float m_camHeight = 10.0f;
    float m_camDist = 10.0f;
};

static TestRegistrar registrar("Texture", "Mipmap Filtering", []() { return new MipmapTest(); });
