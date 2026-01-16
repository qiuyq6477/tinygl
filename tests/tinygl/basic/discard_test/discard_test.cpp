#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>
#include <framework/camera.h>
#include <framework/geometry.h>
#include <texture_manager.h>
#include <vector>
#include <random>

using namespace tinygl;
using namespace framework;
using namespace tests;

struct DiscardShader : ShaderBuiltins {
    SimdMat4 mvp;
    TextureObject* texture = nullptr;
    float alphaThreshold = 0.1f;

    // Vertex Shader
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // Pos
        float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, attribs[0].w};
        Simd4f pos = Simd4f::load(posArr);
        Simd4f res = mvp.transformPoint(pos);
        float outArr[4];
        res.store(outArr);
        
        // Pass UV
        ctx.varyings[0] = attribs[1];

        gl_Position = Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    }

    // Fragment Shader
    void fragment(ShaderContext& ctx) {
        Vec4 uv = ctx.varyings[0];
        Vec4 color = texture ? texture->sample(uv.x, uv.y) : Vec4(1,0,1,1);
        
        // Discard logic
        if (color.w < alphaThreshold) {
            discard();
            return;
        }

        gl_FragColor = color;
    }
};

class DiscardTest : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        camera = Camera({.position = Vec4(0.0f, 1.0f, 3.0f, 1.0f)});

        // Create a simple Quad for the grass billboard
        float vertices[] = {
            // Pos              // UV
            -0.5f, 1.0f, 0.0f,  0.0f, 1.0f, // Top Left
            -0.5f, 0.0f, 0.0f,  0.0f, 0.0f, // Bottom Left
             0.5f, 0.0f, 0.0f,  1.0f, 0.0f, // Bottom Right
             0.5f, 1.0f, 0.0f,  1.0f, 1.0f, // Top Right
        };
        uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

        ctx.glGenVertexArrays(1, &vao);
        ctx.glBindVertexArray(vao);
        
        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 5 * sizeof(float), (void*)0);
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, 5 * sizeof(float), (void*)(3*sizeof(float)));
        ctx.glEnableVertexAttribArray(1);

        // Load Texture from assets
        m_texRef = TextureManager::Load(ctx, "assets/grass.png");
        tex = m_texRef ? m_texRef->id : 0;
        
        // Generate positions for multiple grass blades
        std::mt19937 rng(123);
        std::uniform_real_distribution<float> distPos(-2.0f, 2.0f);
        std::uniform_real_distribution<float> distRot(0.0f, 360.0f);

        for(int i=0; i<50; ++i) {
            grassInstances.push_back({
                Vec4(distPos(rng), 0.0f, distPos(rng), 1.0f),
                distRot(rng)
            });
        }
        
        ctx.glEnable(GL_DEPTH_TEST);
        ctx.glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteVertexArrays(1, &vao);
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteBuffers(1, &ebo);
        m_texRef.reset();
    }

    void onEvent(const SDL_Event& e) override {
        camera.ProcessEvent(e);
    }

    void onUpdate(float dt) override {
        camera.Update(dt);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]){ -1 }, 0);
        mu_label(ctx, "Discard Test (Grass)");
        mu_slider(ctx, &shader.alphaThreshold, 0.0f, 1.0f);
        mu_text(ctx, "Adjust Threshold to see clipping.");
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        Mat4 view = camera.GetViewMatrix();
        Mat4 proj = camera.GetProjectionMatrix();

        shader.texture = ctx.getTextureObject(tex);
        
        ctx.glBindVertexArray(vao);
        ctx.glActiveTexture(GL_TEXTURE0);
        ctx.glBindTexture(GL_TEXTURE_2D, tex);

        // Render Grass Instances
        // Optimization: Sort by distance? For Alpha Test it's not strictly required for correctness 
        // (unlike Alpha Blending), but good for Early-Z perf.
        // But since we just implemented Discard, let's verify it works irrespective of order 
        // (though Early-Z only helps if front-to-back).
        
        for (const auto& inst : grassInstances) {
            Mat4 model = Mat4::Translate(inst.pos.x, inst.pos.y, inst.pos.z) * Mat4::RotateY(inst.rot);
            shader.mvp.load(proj * view * model);
            ctx.glDrawElements(shader, GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        }
    }

private:
    GLuint vao, vbo, ebo, tex;
    std::shared_ptr<Texture> m_texRef;
    Camera camera;
    DiscardShader shader;
    Geometry planeGeo; // Unused for now

    struct GrassInstance {
        Vec4 pos;
        float rot;
    };
    std::vector<GrassInstance> grassInstances;
};

static TestRegistrar registrar("Basic", "DiscardTest", []() -> ITinyGLTestCase* { return new DiscardTest(); });
