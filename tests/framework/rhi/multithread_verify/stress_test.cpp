#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <material.h>
#include <vector>
#include <cmath>

using namespace tinygl;
using namespace framework;
using namespace rhi;
using namespace tests;

// Simple shader that draws a solid color provided by uniforms
struct StressShader : public ShaderBuiltins {
    MaterialData materialData; 

    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        gl_Position = attribs[0];
    }

    void fragment(const ShaderContext& ctx) {
        gl_FragColor = materialData.diffuse;
    }
};

class MultiThreadStressTest : public ITestCase {
public:
    // Resources shared across threads
    BufferHandle vbo;
    PipelineHandle pipeline;
    ShaderHandle shaderHandle;
    CommandEncoder encoder;

    struct VertexPacked {
        Vec4 pos;
    };
    
    int numQuadsX = 20;
    int numQuadsY = 20;
    int numVertices = 0;

    void init(rhi::IGraphicsDevice* device) override {
        // Generate a grid of quads
        std::vector<VertexPacked> vertices;
        float stepX = 2.0f / numQuadsX;
        float stepY = 2.0f / numQuadsY;
        
        for(int y=0; y<numQuadsY; ++y) {
            for(int x=0; x<numQuadsX; ++x) {
                float px = -1.0f + x * stepX;
                float py = -1.0f + y * stepY;
                
                // Quad as 2 triangles
                // Tri 1
                vertices.push_back({ Vec4(px, py + stepY, 0.0f, 1.0f) });
                vertices.push_back({ Vec4(px, py, 0.0f, 1.0f) });
                vertices.push_back({ Vec4(px + stepX, py, 0.0f, 1.0f) });
                
                // Tri 2
                vertices.push_back({ Vec4(px, py + stepY, 0.0f, 1.0f) });
                vertices.push_back({ Vec4(px + stepX, py, 0.0f, 1.0f) });
                vertices.push_back({ Vec4(px + stepX, py + stepY, 0.0f, 1.0f) });
            }
        }
        numVertices = vertices.size();

        BufferDesc bufDesc;
        bufDesc.type = BufferType::VertexBuffer;
        bufDesc.size = vertices.size() * sizeof(VertexPacked);
        bufDesc.initialData = vertices.data();
        vbo = device->CreateBuffer(bufDesc);

        ShaderDesc desc;
        desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<StressShader>>(ctx, pDesc);
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
        shaderHandle = ShaderRegistry::GetInstance().Register("StressShader", desc);

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
        encoder.Reset();
        
        RenderPassDesc pass;
        pass.colorLoadOp = LoadAction::Clear;
        pass.clearColor[0] = 0.1f;
        pass.clearColor[1] = 0.1f;
        pass.clearColor[2] = 0.1f;
        pass.clearColor[3] = 1.0f;
        pass.initialViewport = {0, 0, width, height};
        pass.renderArea = {0, 0, width, height};
        
        encoder.BeginRenderPass(pass);
        
        encoder.SetPipeline(pipeline);
        encoder.SetVertexBuffer(vbo);
        
        MaterialData mat;
        static float time = 0.0f;
        time += 0.05f;
        mat.diffuse = Vec4(0.5f + 0.5f*std::sin(time), 0.5f + 0.5f*std::cos(time), 0.5f, 1.0f); 
        encoder.UpdateUniform(0, mat);
        
        // Draw the massive grid
        encoder.Draw(numVertices, 0);

        encoder.EndRenderPass();
        
        encoder.SubmitTo(*device);
    }

    void onGui(mu_Context* ctx, const tinygl::Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_text(ctx, "Multithread Stress Test");
        std::string info = "Quads: " + std::to_string(numQuadsX * numQuadsY) + " (" + std::to_string(numVertices/3) + " Tris)";
        mu_text(ctx, info.c_str());
    }
    
    void onEvent(const SDL_Event& e) override {}
    void onUpdate(float dt) override {}
};

static TestRegistrar registrar(TINYGL_TEST_GROUP, "StressTest", []() { return new MultiThreadStressTest(); });
