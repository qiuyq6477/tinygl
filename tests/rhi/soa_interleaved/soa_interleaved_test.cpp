#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <framework/geometry.h>
#include <vector>

using namespace tinygl;
using namespace framework;
using namespace rhi;

// Shader that accepts Position from Binding 0 and Normal from Binding 1
struct SoAPlaneShader : public ShaderBuiltins {
    Mat4 mvp;
    void BindUniforms(const std::vector<uint8_t>& data) {
        if (data.size() >= sizeof(SimdMat4)) {
            memcpy(&mvp, data.data(), sizeof(SimdMat4));
        }
    }
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[0] is Position
        // attribs[1] is Normal
        gl_Position = mvp * attribs[0];
        
        // Pass Normal as Color to fragment shader
        // Map Normal (-1..1) to Color (0..1)
        Vec4 normal = attribs[1];
        Vec4 color = normal * 0.5f + Vec4(0.5f, 0.5f, 0.5f, 0.0f);
        color.w = 1.0f;
        ctx.varyings[0] = color;
    }

    void fragment(const ShaderContext& ctx) {
        gl_FragColor = ctx.varyings[0];
    }
};

class SoAInterleavedTest : public ITinyGLTestCase {
public:
    std::unique_ptr<SoftDevice> device;
    CommandEncoder encoder;
    
    BufferHandle combinedBuffer;
    BufferHandle ibo;
    PipelineHandle pipeline;
    ShaderHandle shaderHandle;
    
    uint32_t indexCount = 0;
    
    // Offsets in the combined buffer
    uint32_t posOffset = 0;
    uint32_t nrmOffset = 0;

    void init(SoftRenderContext& ctx) override {
        device = std::make_unique<SoftDevice>(ctx);

        // 1. Generate Geometry (Cube)
        Geometry geo = geometry::createCube(0.5f);
        indexCount = geo.indices.size();

        // 2. Prepare Planar Data in Single Buffer
        // Layout: [Positions... | Normals...]
        std::vector<float> bufferData;
        bufferData.reserve(geo.vertices.size() + geo.normals.size());
        
        posOffset = 0;
        bufferData.insert(bufferData.end(), geo.vertices.begin(), geo.vertices.end());
        
        nrmOffset = bufferData.size() * sizeof(float);
        bufferData.insert(bufferData.end(), geo.normals.begin(), geo.normals.end());

        // 3. Create Buffers
        BufferDesc bufDesc;
        bufDesc.type = BufferType::VertexBuffer;
        bufDesc.size = bufferData.size() * sizeof(float);
        bufDesc.initialData = bufferData.data();
        combinedBuffer = device->CreateBuffer(bufDesc);
        
        BufferDesc idxDesc;
        idxDesc.type = BufferType::IndexBuffer;
        idxDesc.size = geo.indices.size() * sizeof(uint32_t);
        idxDesc.initialData = geo.indices.data();
        ibo = device->CreateBuffer(idxDesc);

        // 4. Create Pipeline
        shaderHandle = ShaderRegistry::RegisterShader<SoAPlaneShader>("SoAPlaneShader");
        
        PipelineDesc pipeDesc;
        pipeDesc.shader = shaderHandle;
        pipeDesc.useInterleavedAttributes = false; // ENABLE SOA MODE
        pipeDesc.cullMode = CullMode::Back;
        pipeDesc.depthTestEnabled = true; // Cube needs depth test
        
        // Define Attributes
        // Position: Float4 (x,y,z,w from geometry.h)
        // Normal: Float3 (geometry.h normals are 3 floats, wait... let's check)
        // geometry.h: std::vector<float> normals; // 3 components
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float4, 0, 0 }, // Loc 0 (Pos) -> Binding 0. Stride=16
            { VertexFormat::Float3, 0, 1 }  // Loc 1 (Nrm) -> Binding 1. Stride=12
        };

        pipeline = device->CreatePipeline(pipeDesc);
    }

    void destroy(SoftRenderContext& ctx) override {
        if (device) {
            device->DestroyPipeline(pipeline);
            device->DestroyBuffer(combinedBuffer);
            device->DestroyBuffer(ibo);
            device.reset();
        }
    }

    void onRender(SoftRenderContext& ctx) override {
        // Rotating Cube
        static float angle = 0.0f;
        angle += 1.0f;

        // Calculate MVP
        Mat4 model = Mat4::RotateY(angle) * Mat4::RotateX(angle * 0.5f);
        Mat4 view = Mat4::Translate(0, 0, -2.0f);
        Mat4 proj = Mat4::Perspective(45.0f, 1.33, 0.1f, 10.0f);
        Mat4 mvp = proj * view * model;

        encoder.Reset();

        RenderPassDesc passDesc;
        passDesc.colorLoadOp = LoadAction::Clear;
        passDesc.clearColor[0] = 0.2f;
        passDesc.clearColor[1] = 0.2f;
        passDesc.clearColor[2] = 0.2f;
        passDesc.clearColor[3] = 1.0f;
        passDesc.depthLoadOp = LoadAction::Clear;
        passDesc.clearDepth = 1.0f;
        passDesc.initialViewport = {0, 0, ctx.glGetViewport().w, ctx.glGetViewport().h};

        encoder.BeginRenderPass(passDesc);

        encoder.SetPipeline(pipeline);

        // Pass Uniform (Slot 0 for mvp by default if shader has it as first member or handled by SoftPipeline)
        // In SoftPipeline, it copies uniformData to shader memory.
        encoder.UpdateUniform(0, mvp);
        
        // Bind Stream 0: Positions
        // Offset = 0. Stride = 16 (Float4)
        encoder.SetVertexStream(0, combinedBuffer, 0, 16);
        
        // Bind Stream 1: Normals
        // Offset = nrmOffset. Stride = 12 (Float3)
        encoder.SetVertexStream(1, combinedBuffer, nrmOffset, 12);
        
        encoder.SetIndexBuffer(ibo);
        
        encoder.DrawIndexed(indexCount); 

        encoder.EndRenderPass();

        if (device) {
            encoder.SubmitTo(*device);
        }
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_text(ctx, "SoA Single Buffer Test");
        mu_text(ctx, "Cube should be visible with Normal-based colors.");
        mu_text(ctx, "Pos and Normals are in one buffer but separate regions.");
    }
    
    void onEvent(const SDL_Event& e) override {}
    void onUpdate(float dt) override {}
};

static TestRegistrar registrar("RHI", "SoAInterleaved", []() { return new SoAInterleavedTest(); });
