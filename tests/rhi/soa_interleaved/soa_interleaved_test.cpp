#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <rhi/soft_device.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <framework/geometry.h>
#include <framework/camera.h>
#include <vector>

using namespace tinygl;
using namespace framework;
using namespace rhi;

// Shader that accepts Position from Binding 0 and Normal from Binding 1
struct SoAPlaneShader : public ShaderBuiltins {
    struct Uniforms {
        SimdMat4 mvp;
    } uniformData;
    
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[0] is Position
        // attribs[1] is Normal
        Simd4f pos = Simd4f::load(attribs[0]);
        Simd4f res = uniformData.mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        gl_Position = Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
        
        // Pass Normal as Color to fragment shader
        Vec4 normal = attribs[1];
        Vec4 color = normal * 0.5f + Vec4(0.5f, 0.5f, 0.5f, 0.0f);
        color.w = 1.0f;
        ctx.varyings[0] = color;
    }

    void fragment(const ShaderContext& ctx) {
        gl_FragColor = ctx.varyings[0];
    }

    void BindUniforms(const std::vector<uint8_t>& data) {
        if (data.size() >= sizeof(Uniforms)) {
            memcpy(&uniformData, data.data(), sizeof(Uniforms));
        }
    }
};

class SoAInterleavedTest : public IRHITestCase {
public:
    CommandEncoder encoder;
    
    BufferHandle combinedBuffer;
    BufferHandle ibo;
    PipelineHandle pipeline;
    ShaderHandle shaderHandle;
    Camera camera;
    float angle = 0.0f;
    
    uint32_t indexCount = 0;
    
    // Offsets in the combined buffer
    uint32_t posOffset = 0;
    uint32_t nrmOffset = 0;

    void init(rhi::IGraphicsDevice* device) override {
        // Setup Camera
        CameraCreateInfo camInfo;
        camInfo.position = Vec4(0.0f, 0.0f, 2.0f, 1.0f);
        camera = Camera(camInfo);

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
        ShaderDesc desc;
        desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<SoAPlaneShader>>(ctx, pDesc);
        };
        desc.glsl.vertex = R"(#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec3 aNormal;
out vec4 vertexColor;
layout (std140) uniform Uniforms {
    mat4 mvp;
};
void main() {
    gl_Position = mvp * aPos;
    vertexColor = vec4(aNormal * 0.5 + 0.5, 1.0);
}
)";
        desc.glsl.fragment = R"(#version 330 core
in vec4 vertexColor;
out vec4 FragColor;
void main() {
    FragColor = vertexColor;
}
)";
        shaderHandle = ShaderRegistry::GetInstance().Register("SoAPlaneShader", desc);
        
        PipelineDesc pipeDesc;
        pipeDesc.shader = shaderHandle;
        pipeDesc.useInterleavedAttributes = false;
        pipeDesc.cullMode = CullMode::Back;
        pipeDesc.depthTestEnabled = true;
        
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float4, 0, 0 }, // Loc 0 (Pos) -> Binding 0
            { VertexFormat::Float3, 0, 1 }  // Loc 1 (Nrm) -> Binding 1
        };

        pipeline = device->CreatePipeline(pipeDesc);
    }

    void destroy(rhi::IGraphicsDevice* device) override {
        if (device) {
            device->DestroyPipeline(pipeline);
            device->DestroyBuffer(combinedBuffer);
            device->DestroyBuffer(ibo);
        }
    }

    void onRender(rhi::IGraphicsDevice* device, int width, int height) override {
        // Calculate MVP
        camera.aspect = (float)width / (float)height;
        Mat4 model = Mat4::RotateY(angle) * Mat4::RotateX(angle * 0.5f);
        Mat4 view = camera.GetViewMatrix();
        Mat4 proj = camera.GetProjectionMatrix();
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
        passDesc.initialViewport = {0, 0, width, height};

        encoder.BeginRenderPass(passDesc);

        encoder.SetPipeline(pipeline);

        SoAPlaneShader::Uniforms u;
        u.mvp.load(mvp);
        encoder.UpdateUniform(0, u);
        
        // Bind Stream 0: Positions from combined buffer
        encoder.SetVertexStream(0, combinedBuffer, posOffset, 4 * sizeof(float));
        
        // Bind Stream 1: Normals from combined buffer
        encoder.SetVertexStream(1, combinedBuffer, nrmOffset, 3 * sizeof(float));
        
        encoder.SetIndexBuffer(ibo);
        
        encoder.DrawIndexed(indexCount); 

        encoder.EndRenderPass();

        encoder.SubmitTo(*device);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_text(ctx, "SoA Single Buffer Test");
        mu_text(ctx, "Cube should be visible with Normal-based colors.");
        mu_text(ctx, "Pos and Normals are in one buffer but separate regions.");
    }
    
    void onEvent(const SDL_Event& e) override {
        camera.ProcessEvent(e);
    }
    void onUpdate(float dt) override {
        angle += 50.0f * dt;
        camera.Update(dt);
    }
};

static TestRegistrar registrar("RHI", "SoAInterleaved", []() { return new SoAInterleavedTest(); });
