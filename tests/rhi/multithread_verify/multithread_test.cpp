#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <framework/material.h>
#include <vector>
#include <thread>
#include <mutex>

using namespace tinygl;
using namespace framework;
using namespace rhi;

// Simple shader that draws a solid color provided by uniforms
struct SolidColorShader : public ShaderBuiltins {
    MaterialData materialData; 

    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        gl_Position = attribs[0];
    }

    void fragment(const ShaderContext& ctx) {
        gl_FragColor = materialData.diffuse;
    }
};

class MultiThreadRecordTest : public IRHITestCase {
public:
    // Resources shared across threads
    BufferHandle vbo;
    PipelineHandle pipeline;
    ShaderHandle shaderHandle;

    struct VertexPacked {
        Vec4 pos;
    };

    void init(rhi::IGraphicsDevice* device) override {
        // Define a full-screen quad (2 triangles)
        std::vector<VertexPacked> vertices = {
            // Triangle 1 (Top-Left)
            { { -0.5f,  0.5f, 0.0f, 1.0f} },
            { { -0.5f, -0.5f, 0.0f, 1.0f} },
            { {  0.0f, -0.5f, 0.0f, 1.0f} },
            
            // Triangle 2 (Bottom-Right)
            { {  0.0f,  0.5f, 0.0f, 1.0f} },
            { {  0.0f, -0.5f, 0.0f, 1.0f} },
            { {  0.5f, -0.5f, 0.0f, 1.0f} }
        };

        BufferDesc bufDesc;
        bufDesc.type = BufferType::VertexBuffer;
        bufDesc.size = vertices.size() * sizeof(VertexPacked);
        bufDesc.initialData = vertices.data();
        vbo = device->CreateBuffer(bufDesc);

        ShaderDesc desc;
        desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<SolidColorShader>>(ctx, pDesc);
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
        shaderHandle = ShaderRegistry::GetInstance().Register("SolidColorShader", desc);

        PipelineDesc pipeDesc;
        pipeDesc.shader = shaderHandle;
        pipeDesc.cullMode = CullMode::None;
        pipeDesc.inputLayout.stride = sizeof(VertexPacked);
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float4, 0, 0 }
        };

        pipeline = device->CreatePipeline(pipeDesc);
    }

    void destroy(rhi::IGraphicsDevice* device) override {
        if (device) {
            device->DestroyPipeline(pipeline);
            device->DestroyBuffer(vbo);
        }
    }

    void onRender(rhi::IGraphicsDevice* device, int width, int height) override {
        // Simulate multi-threaded recording
        // We will have 2 encoders:
        // Encoder A draws the Left Triangle (Red)
        // Encoder B draws the Right Triangle (Blue)
        
        CommandEncoder encoderA;
        CommandEncoder encoderB;

        // Job A: Record commands for Red Triangle
        auto jobA = [&]() {
            encoderA.Reset();
            
            RenderPassDesc pass;
            pass.colorLoadOp = LoadAction::Clear;
            pass.clearColor[0] = 0.2f;
            pass.clearColor[1] = 0.2f;
            pass.clearColor[2] = 0.2f;
            pass.clearColor[3] = 1.0f;
            pass.initialViewport = {0, 0, width, height};
            pass.renderArea = {0, 0, width, height};
            
            encoderA.BeginRenderPass(pass);
            
            encoderA.SetPipeline(pipeline);
            encoderA.SetVertexBuffer(vbo);
            
            MaterialData mat;
            mat.diffuse = Vec4(1.0f, 0.0f, 0.0f, 1.0f); // Red
            encoderA.UpdateUniform(0, mat);
            
            // Draw first 3 vertices
            encoderA.Draw(3, 0);

            encoderA.EndRenderPass();
        };

        // Job B: Record commands for Blue Triangle
        auto jobB = [&]() {
            encoderB.Reset();

            RenderPassDesc pass;
            pass.colorLoadOp = LoadAction::Load; // DON'T CLEAR, keep encoderA's output
            pass.initialViewport = {0, 0, width, height};
            
            encoderB.BeginRenderPass(pass);
            
            encoderB.SetPipeline(pipeline);
            encoderB.SetVertexBuffer(vbo); // Redundant binding, but necessary for isolated buffer
            
            MaterialData mat;
            mat.diffuse = Vec4(0.0f, 0.0f, 1.0f, 1.0f); // Blue
            encoderB.UpdateUniform(0, mat);
            
            // Draw next 3 vertices (offset 3)
            encoderB.Draw(3, 3);

            encoderB.EndRenderPass();
        };

        // Execute jobs in parallel
        std::thread t1(jobA);
        std::thread t2(jobB);
        t1.join();
        t2.join();

        // Submit Phase (Must be on main thread)
        if (device) {
            encoderA.SubmitTo(*device);
            encoderB.SubmitTo(*device);
        }
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_text(ctx, "Multi-Thread Recording Verify");
        mu_text(ctx, "Left Triangle: Red (Thread A)");
        mu_text(ctx, "Right Triangle: Blue (Thread B)");
        mu_text(ctx, "If both appear, isolated recording works.");
    }
    
    void onEvent(const SDL_Event& e) override {}
    void onUpdate(float dt) override {}
};

static TestRegistrar registrar("RHI", "MultithreadVerify", []() { return new MultiThreadRecordTest(); });
