#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <framework/material.h>

using namespace tinygl;
using namespace framework;
using namespace rhi;

namespace {

struct ColorShader : public ShaderBuiltins {
    MaterialData materialData; 
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        gl_Position = attribs[0];
    }
    void fragment(const ShaderContext& ctx) {
        gl_FragColor = materialData.diffuse;
    }
};

class RhiBlendScissorTest : public IRHITestCase {
public:
    CommandEncoder encoder;
    
    BufferHandle vbo;
    PipelineHandle opaquePipe;
    PipelineHandle blendPipe;
    ShaderHandle shaderHandle;

    bool enableScissor = true;
    float scissorX = 100, scissorY = 100, scissorW = 600, scissorH = 400;

    void init(rhi::IGraphicsDevice* device) override {

        float vertices[] = {
            // Quad 1: Bottom-Left to Center
            -0.8f, -0.8f, 0.0f, 1.0f,
             0.2f, -0.8f, 0.0f, 1.0f,
             0.2f,  0.2f, 0.0f, 1.0f,
            -0.8f, -0.8f, 0.0f, 1.0f,
             0.2f,  0.2f, 0.0f, 1.0f,
            -0.8f,  0.2f, 0.0f, 1.0f,

            // Quad 2: Center to Top-Right
            -0.2f, -0.2f, 0.0f, 1.0f,
             0.8f, -0.2f, 0.0f, 1.0f,
             0.8f,  0.8f, 0.0f, 1.0f,
            -0.2f, -0.2f, 0.0f, 1.0f,
             0.8f,  0.8f, 0.0f, 1.0f,
            -0.2f,  0.8f, 0.0f, 1.0f,
        };

        BufferDesc bufDesc;
        bufDesc.type = BufferType::VertexBuffer;
        bufDesc.size = sizeof(vertices);
        bufDesc.initialData = vertices;
        vbo = device->CreateBuffer(bufDesc);

        ShaderDesc desc;
        desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<ColorShader>>(ctx, pDesc);
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
        shaderHandle = ShaderRegistry::GetInstance().Register("RhiColorShader", desc);
        
        // Opaque Pipeline
        PipelineDesc opaqueDesc;
        opaqueDesc.shader = shaderHandle;
        opaqueDesc.cullMode = CullMode::None;
        opaqueDesc.inputLayout.stride = sizeof(Vec4);
        opaqueDesc.inputLayout.attributes = {{ VertexFormat::Float4, 0, 0 }};
        opaqueDesc.blend.enabled = false;
        opaquePipe = device->CreatePipeline(opaqueDesc);

        // Blend Pipeline
        PipelineDesc blendDesc = opaqueDesc;
        blendDesc.blend.enabled = true;
        blendDesc.blend.srcRGB = BlendFactor::SrcAlpha;
        blendDesc.blend.dstRGB = BlendFactor::OneMinusSrcAlpha;
        blendPipe = device->CreatePipeline(blendDesc);
    }

    void destroy(rhi::IGraphicsDevice* device) override {
        if (device) {
            device->DestroyPipeline(opaquePipe);
            device->DestroyPipeline(blendPipe);
            device->DestroyBuffer(vbo);
        }
    }

    void onRender(rhi::IGraphicsDevice* device, int width, int height) override {
        encoder.Reset();

        // 1. Setup Render Pass
        RenderPassDesc passDesc;
        passDesc.colorLoadOp = LoadAction::Clear;
        passDesc.clearColor[0] = 0.1f;
        passDesc.clearColor[1] = 0.1f;
        passDesc.clearColor[2] = 0.15f;
        passDesc.clearColor[3] = 1.0f;
        passDesc.depthLoadOp = LoadAction::Clear;
        passDesc.clearDepth = 1.0f;
        passDesc.initialViewport = {0, 0, width, height};

        if (enableScissor) {
            passDesc.initialScissor = {(int)scissorX, (int)scissorY, (int)scissorW, (int)scissorH};
        } else {
            passDesc.initialScissor = {0, 0, -1, -1}; // Disable
        }

        encoder.BeginRenderPass(passDesc);

        // 2. Draw Opaque Quad
        encoder.SetPipeline(opaquePipe);
        encoder.SetVertexBuffer(vbo);
        MaterialData mat1;
        mat1.diffuse = Vec4(1.0f, 0.0f, 0.0f, 1.0f);
        encoder.UpdateUniform(0, mat1);
        encoder.Draw(6, 0);

        // 3. Draw Blended Quad
        encoder.SetPipeline(blendPipe);
        MaterialData mat2;
        mat2.diffuse = Vec4(0.0f, 1.0f, 0.0f, 0.5f); // Semi-transparent green
        encoder.UpdateUniform(0, mat2);
        encoder.Draw(6, 6);

        encoder.EndRenderPass();

        encoder.SubmitTo(*device);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_checkbox(ctx, "Enable Scissor", (int*)&enableScissor);
        mu_layout_row(ctx, 2, (int[]) { 60, -1 }, 0);
        mu_label(ctx, "X"); mu_slider(ctx, &scissorX, 0, 800);
        mu_label(ctx, "Y"); mu_slider(ctx, &scissorY, 0, 600);
        mu_label(ctx, "W"); mu_slider(ctx, &scissorW, 0, 800);
        mu_label(ctx, "H"); mu_slider(ctx, &scissorH, 0, 600);
    }
    
    void onEvent(const SDL_Event& e) override {}
    void onUpdate(float dt) override {}
};

static TestRegistrar registrar("RHI", "BlendScissor", []() { return new RhiBlendScissorTest(); }); 

}
