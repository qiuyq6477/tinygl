#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <framework/geometry.h>
#include <framework/texture_manager.h>
#include <framework/camera.h>
#include <cmath>
#include <vector>

using namespace tinygl;
using namespace framework;
using namespace rhi;

// Define RHI-compatible Shader
struct RhiCubeShader : public ShaderBuiltins {
    // Uniforms
    SimdMat4 mvp;
    // Resources
    TextureObject* texture = nullptr;

    // Interface Block for Fragment Shader Reading
    struct FS_In {
        const Vec4& uv;
        FS_In(const Vec4* v) : uv(v[0]) {}
    };

    // Vertex Shader
    void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        // Input:
        // attribs[0] -> Position (4 floats)
        // attribs[1] -> Normal (3 floats, unused)
        // attribs[2] -> Tangent (3 floats, unused)
        // attribs[3] -> Bitangent (3 floats, unused)
        // attribs[4] -> UV (2 floats)
        
        // Transform Position
        // attribs[0] is automatically expanded to (x,y,z,1) by SoftRenderContext fetcher
        Simd4f pos = Simd4f::load(attribs[0]);
        Simd4f res = mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        gl_Position = Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);

        // Pass UV (Slot 0)
        outCtx.varyings[0] = attribs[4];
    }

    // Fragment Shader
    void fragment(const ShaderContext& inCtx) {
        FS_In in(inCtx.varyings);

        Vec4 texColor = Vec4(1.0f, 0.0f, 1.0f, 1.0f); // Default Pink
        if (texture) {
            // Use texture sampling
            texColor = texture->sample(in.uv.x, in.uv.y);
        }
        
        gl_FragColor = texColor;
    }

    // RHI Binding Hooks
    void BindUniforms(const std::vector<uint8_t>& data) {
        if (data.size() >= sizeof(SimdMat4)) {
            memcpy(&mvp, data.data(), sizeof(SimdMat4));
        }
    }

    void BindResources(SoftRenderContext& ctx) {
        // Assume texture is bound to Unit 0
        texture = ctx.getTexture(0);
    }
};

class RhiTexturedCubeTest : public ITestCase {
public:
    std::unique_ptr<SoftDevice> device;
    CommandEncoder encoder;
    
    BufferHandle vbo;
    BufferHandle ebo;
    TextureHandle textureHandle;
    std::shared_ptr<Texture> textureAsset;
    PipelineHandle pipeline;
    ShaderHandle shaderHandle;

    Camera camera;

    float m_rotationAngle = 0.0f;
    size_t m_indexCount = 0;

    void init(SoftRenderContext& ctx) override {
        // Initialize Device
        device = std::make_unique<SoftDevice>(ctx);

        // Setup Camera
        CameraCreateInfo camInfo;
        camInfo.position = Vec4(0.0f, 0.0f, 3.0f, 1.0f);
        camInfo.aspect = (float)ctx.getWidth() / (float)ctx.getHeight();
        camera = Camera(camInfo);

        // 1. Create Texture via Manager
        textureAsset = TextureManager::Load(ctx, "texture/cube/assets/container.jpg");
        if (textureAsset) {
            // Import into RHI
            textureHandle = device->CreateTextureFromNative(textureAsset->id);
            printf("RHI: Imported texture from TextureManager (ID: %d)\n", textureAsset->id);
        } else {
            printf("RHI: Failed to load texture asset.\n");
        }

        // 2. Create Geometry using Utils
        Geometry geo = geometry::createCube(1.0f);
        m_indexCount = geo.indices.size();

        BufferDesc vboDesc;
        vboDesc.type = BufferType::VertexBuffer;
        vboDesc.size = geo.allAttributes.size() * sizeof(float);
        vboDesc.initialData = geo.allAttributes.data();
        vbo = device->CreateBuffer(vboDesc);

        BufferDesc eboDesc;
        eboDesc.type = BufferType::IndexBuffer;
        eboDesc.size = geo.indices.size() * sizeof(uint32_t);
        eboDesc.initialData = geo.indices.data();
        ebo = device->CreateBuffer(eboDesc);

        // 3. Create Pipeline
        shaderHandle = RegisterShader<RhiCubeShader>(*device);
        
        PipelineDesc pipeDesc;
        pipeDesc.shader = shaderHandle;
        pipeDesc.depthTestEnabled = true;
        pipeDesc.cullMode = CullMode::Back;
        
        // Define Input Layout matching GeometryUtils::finalize()
        // Pos(4) + Norm(3) + Tan(3) + Bitan(3) + UV(2) = 15 floats
        size_t stride = 15 * sizeof(float);
        pipeDesc.inputLayout.stride = stride;
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float4, 0 * sizeof(float), 0 }, // Loc 0: Pos
            { VertexFormat::Float3, 4 * sizeof(float), 1 }, // Loc 1: Normal
            { VertexFormat::Float3, 7 * sizeof(float), 2 }, // Loc 2: Tangent
            { VertexFormat::Float3, 10 * sizeof(float), 3 }, // Loc 3: Bitangent
            { VertexFormat::Float2, 13 * sizeof(float), 4 }  // Loc 4: UV
        };

        pipeline = device->CreatePipeline(pipeDesc);
    }

    void destroy(SoftRenderContext& ctx) override {
        if (device) {
            device->DestroyPipeline(pipeline);
            // We destroy the handle, but RHI won't delete the GL ID because we used CreateTextureFromNative
            device->DestroyTexture(textureHandle);
            device->DestroyBuffer(vbo);
            device->DestroyBuffer(ebo);
            device.reset();
        }
        // TextureAsset (shared_ptr) releases resource automatically
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        encoder.Reset();

        // Viewport
        const auto& vp = ctx.glGetViewport();
        encoder.SetViewport(vp.x, vp.y, vp.w, vp.h);

        // Calculate MVP
        float aspect = (float)vp.w / (float)vp.h;
        // Camera Aspect might need update if window resized
        camera.aspect = aspect;

        Mat4 model = Mat4::Translate(0, 0, -5.0f) * 
                     Mat4::RotateY(m_rotationAngle) * 
                     Mat4::RotateX(m_rotationAngle * 0.5f);
        
        Mat4 view = camera.GetViewMatrix();
        Mat4 proj = camera.GetProjectionMatrix();
        
        Mat4 mvp = proj * view * model; 

        SimdMat4 u;
        u.load(mvp);

        // Encode Commands
        encoder.SetPipeline(pipeline);
        encoder.SetVertexBuffer(vbo);
        encoder.SetIndexBuffer(ebo);
        
        // Bind Texture to Slot 0
        if (textureAsset) {
             encoder.SetTexture(0, textureHandle);
        }

        // Update Uniforms (Slot 0)
        encoder.UpdateUniform(0, u);

        // Draw Indexed
        encoder.DrawIndexed(m_indexCount);

        // Submit
        if (device) {
            encoder.SubmitTo(*device);
        }
    }

    void onUpdate(float dt) override {
        m_rotationAngle += 50.0f * dt;
        if (m_rotationAngle > 720.0f) m_rotationAngle -= 720.0f;
        
        camera.Update(dt);
    }
    
    void onEvent(const SDL_Event& e) override {
        camera.ProcessEvent(e);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_text(ctx, "RHI Textured Cube");
        mu_text(ctx, "Use WASD + RMB to fly");
        mu_text(ctx, "Alt + LMB to Orbit");
    }
};

static TestRegistrar registrar("RHI", "TexturedCube", []() { return new RhiTexturedCubeTest(); });