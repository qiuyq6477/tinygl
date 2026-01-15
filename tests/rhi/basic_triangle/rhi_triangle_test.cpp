#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <framework/material.h>
#include <cmath>

using namespace tinygl;
using namespace framework;
using namespace rhi;

// 1. Define a Shader compatible with RHI
// Must accept 'materialData' for uniforms to receive data from Slot 0
struct RhiTriangleShader : public ShaderBuiltins {
    // Uniforms injected by RHI from Slot 0
    MaterialData materialData; 
    
    // Vertex Shader
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[0] is Position (based on hardcoded layout in SoftDevice)
        gl_Position = attribs[0];
    }

    // Fragment Shader
    void fragment(const ShaderContext& ctx) {
        // Use color from Uniform Buffer (MaterialData)
        gl_FragColor = materialData.diffuse;
    }
};

class RhiTriangleTest : public IRHITestCase {
public:
    CommandEncoder encoder;
    
    BufferHandle vbo;
    PipelineHandle pipeline;
    ShaderHandle shaderHandle;

    void init(rhi::IGraphicsDevice* device) override {
        // 1. Create Resources
        // Triangle Vertices
        // Hack: Matching the stride expected by SoftDevice Phase 2 implementation
        // It expects stride of 3*Vec4 (Pos, Normal, UV)
        struct VertexPacked {
            Vec4 pos;
            Vec4 normal; // Unused
            Vec4 uv;     // Unused
        };
        
        std::vector<VertexPacked> vertices = {
            { { 0.0f,  0.5f, 0.0f, 1.0f}, {}, {} },
            { {-0.5f, -0.5f, 0.0f, 1.0f}, {}, {} },
            { { 0.5f, -0.5f, 0.0f, 1.0f}, {}, {} }
        };

        BufferDesc bufDesc;
        bufDesc.type = BufferType::VertexBuffer;
        bufDesc.size = vertices.size() * sizeof(VertexPacked);
        bufDesc.initialData = vertices.data();
        
        vbo = device->CreateBuffer(bufDesc);

        // 2. Register Shader & Create Pipeline
        // This binds the runtime handle to the RhiTriangleShader template AND provides GLSL for GLDevice
        ShaderDesc desc;
        desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<RhiTriangleShader>>(ctx, pDesc);
        };
        desc.glsl.vertex = R"(#version 330 core
layout(location = 0) in vec4 aPos;
layout(std140) uniform MaterialData {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    float opacity;
    float alphaCutoff;
    int alphaTest;
    int doubleSided;
};
void main() {
    gl_Position = aPos;
}
)";
        desc.glsl.fragment = R"(#version 330 core
out vec4 FragColor;
layout(std140) uniform MaterialData {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    float opacity;
    float alphaCutoff;
    int alphaTest;
    int doubleSided;
};
void main() {
    FragColor = diffuse;
}
)";
        shaderHandle = ShaderRegistry::GetInstance().Register("RhiTriangleShader", desc);
        
        PipelineDesc pipeDesc;
        pipeDesc.shader = shaderHandle;
        pipeDesc.cullMode = CullMode::None; // Ensure visibility regardless of winding
        
        // Define Vertex Input Layout (CRITICAL for RHI Phase 3)
        // Stride is 48 bytes (3 * Vec4)
        pipeDesc.inputLayout.stride = sizeof(VertexPacked);
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float4, offsetof(VertexPacked, pos), 0 } // Loc 0: Position
            // Loc 1 (Normal) and Loc 2 (UV) are present in data but unused by shader, 
            // so we don't strictly need to define them here if the shader doesn't read them.
        };

        pipeline = device->CreatePipeline(pipeDesc);
    }

    void destroy(rhi::IGraphicsDevice* device) override {
        // Cleanup RHI resources
        if (device) {
            device->DestroyPipeline(pipeline);
            device->DestroyBuffer(vbo);
        }
    }

    void onRender(rhi::IGraphicsDevice* device, int width, int height) override {
        encoder.Reset();
       
        RenderPassDesc passDesc;
        passDesc.colorLoadOp = LoadAction::Clear;
        passDesc.clearColor[0] = 0.1f;
        passDesc.clearColor[1] = 0.1f;
        passDesc.clearColor[2] = 0.15f;
        passDesc.clearColor[3] = 1.0f;
        passDesc.depthLoadOp = LoadAction::Clear;
        passDesc.clearDepth = 1.0f;
        
        // Use Render Area to restrict clear to viewport (Test Area)
        passDesc.renderArea = {0, 0, width, height};
        
        passDesc.initialViewport = {0, 0, width, height};

        encoder.BeginRenderPass(passDesc);

        encoder.SetPipeline(pipeline);
        encoder.SetVertexBuffer(vbo);
        
        static float time = 0.0f;
        time += 0.02f;
        
        MaterialData matData;
        // Pulse red channel
        matData.diffuse = Vec4(std::abs(std::sin(time)), 0.6f, 0.2f, 1.0f);
        
        // Slot 0 is mapped to materialData in SoftPipeline
        encoder.UpdateUniform(0, matData);
        
        encoder.Draw(3); 
        encoder.EndRenderPass();

        encoder.SubmitTo(*device);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_text(ctx, "RHI Triangle Test");
        mu_text(ctx, "Rendering via CommandBuffer -> SoftDevice");
        mu_text(ctx, "Color is updated via UniformBuffer payload");
    }
    
    void onEvent(const SDL_Event& e) override {}
    void onUpdate(float dt) override {}
};

static TestRegistrar registrar("RHI", "BasicTriangle", []() { return new RhiTriangleTest(); });
