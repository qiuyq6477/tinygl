#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <framework/material.h>
#include <vector>

using namespace tinygl;
using namespace framework;
using namespace rhi;

// Shader that accepts Position from Binding 0 and Color from Binding 1
struct SoATriangleShader : public ShaderBuiltins {
    
    // Attributes
    // In SoA mode, we map these manually to bindings in PipelineDesc
    
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[0] is Position (from Binding 0)
        // attribs[1] is Color (from Binding 1)
        gl_Position = attribs[0];
        
        // Pass color to fragment shader (using varying 0)
        ctx.varyings[0] = attribs[1];
    }

    void fragment(const ShaderContext& ctx) {
        gl_FragColor = ctx.varyings[0];
    }
};

class SoATest : public IRHITestCase {
public:
    CommandEncoder encoder;
    
    BufferHandle posBuffer;
    BufferHandle colorBuffer;
    PipelineHandle pipeline;
    ShaderHandle shaderHandle;

    void init(rhi::IGraphicsDevice* device) override {
        // 1. Prepare SoA Data (Planar)
        // Stream 0: Positions (3 vertices * 3 floats)
        std::vector<float> positions = {
             0.0f,  0.5f, 0.0f, // Top
            -0.5f, -0.5f, 0.0f, // Left
             0.5f, -0.5f, 0.0f  // Right
        };
        
        // Stream 1: Colors (3 vertices * 4 floats)
        std::vector<float> colors = {
            1.0f, 0.0f, 0.0f, 1.0f, // Red
            0.0f, 1.0f, 0.0f, 1.0f, // Green
            0.0f, 0.0f, 1.0f, 1.0f  // Blue
        };

        // 2. Create Buffers
        BufferDesc posDesc;
        posDesc.type = BufferType::VertexBuffer;
        posDesc.size = positions.size() * sizeof(float);
        posDesc.initialData = positions.data();
        posBuffer = device->CreateBuffer(posDesc);
        
        BufferDesc colDesc;
        colDesc.type = BufferType::VertexBuffer;
        colDesc.size = colors.size() * sizeof(float);
        colDesc.initialData = colors.data();
        colorBuffer = device->CreateBuffer(colDesc);

        // 3. Register Shader
        ShaderDesc desc;
        desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<SoATriangleShader>>(ctx, pDesc);
        };
        desc.glsl.vertex = R"(#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec4 aColor;
out vec4 vertexColor;
void main() {
    gl_Position = vec4(aPos, 1.0);
    vertexColor = aColor;
}
)";
        desc.glsl.fragment = R"(#version 330 core
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
}
)";
        shaderHandle = ShaderRegistry::GetInstance().Register("SoATriangleShader", desc);

        // 4. Create Pipeline with Multi-Stream Layout
        PipelineDesc pipeDesc;
        pipeDesc.shader = shaderHandle;
        pipeDesc.useInterleavedAttributes = false; // ENABLE SOA MODE
        pipeDesc.cullMode = CullMode::None;
        
        // Define Attributes and their bindings
        // Note: For 'useInterleavedAttributes = false', the RHI maps:
        // Attribute at shaderLocation N -> Binding Slot N
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float3, 0, 0 }, // Loc 0 (Pos) -> Binding 0. Offset 0 within stream.
            { VertexFormat::Float4, 0, 1 }  // Loc 1 (Col) -> Binding 1. Offset 0 within stream.
        };
        // Note: 'inputLayout.stride' is ignored/unused in Planar mode usually, 
        // as we provide per-binding strides at Draw time.

        pipeline = device->CreatePipeline(pipeDesc);
    }

    void destroy(rhi::IGraphicsDevice* device) override {
        if (device) {
            device->DestroyPipeline(pipeline);
            device->DestroyBuffer(posBuffer);
            device->DestroyBuffer(colorBuffer);
        }
    }

    void onRender(rhi::IGraphicsDevice* device, int width, int height) override {
        encoder.Reset();

        RenderPassDesc passDesc;
        passDesc.colorLoadOp = LoadAction::Clear;
        passDesc.clearColor[0] = 0.1f;
        passDesc.clearColor[1] = 0.1f;
        passDesc.clearColor[2] = 0.1f;
        passDesc.clearColor[3] = 1.0f;
        passDesc.initialViewport = {0, 0, width, height};
        
        encoder.BeginRenderPass(passDesc);
        
        encoder.SetPipeline(pipeline);
        
        // Bind Stream 0: Positions
        encoder.SetVertexStream(0, posBuffer, 0, 3 * sizeof(float));
        
        // Bind Stream 1: Colors
        encoder.SetVertexStream(1, colorBuffer, 0, 4 * sizeof(float));
        
        encoder.Draw(3); 

        encoder.EndRenderPass();

        encoder.SubmitTo(*device);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_text(ctx, "SoA / Multi-Stream Test");
        mu_text(ctx, "Triangle should be Multi-Colored (Red/Green/Blue)");
        mu_text(ctx, "Data fetched from 2 separate buffers.");
    }
    
    void onEvent(const SDL_Event& e) override {}
    void onUpdate(float dt) override {}
};

static TestRegistrar registrar("RHI", "SoAVerify", []() { return new SoATest(); });
