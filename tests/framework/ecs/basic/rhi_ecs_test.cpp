#include <ITestCase.h>
#include <test_registry.h>
#include <framework/asset_manager.h>
#include <framework/shared_asset.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <rhi/soft_pipeline.h>
#include <material.h>
#include <tinygl/tinygl.h>

using namespace tinygl;
using namespace framework;
using namespace rhi;
using namespace tests;

struct PerDrawData {
    Mat4 mvp;
};

#include <framework/resources.h>

// Shader Definition
struct ECSShader : public ShaderBuiltins {
    // Slot 0: Material Data
    MaterialResource::Data material;
    // Slot 1: Transform Data
    PerDrawData transform;
    
    // Texture Object (Runtime injection)
    TextureObject* diffuseMap = nullptr;
    TextureObject* specularMap = nullptr;
    
    void BindResources(SoftRenderContext& ctx) {
        // Slot 0: Diffuse, Slot 1: Specular
        diffuseMap = ctx.getTexture(0);
        specularMap = ctx.getTexture(1);
    }

    void BindUniforms(const uint8_t* uniformData, size_t size) {
        size_t slotSize = 256; 
        
        if (size >= sizeof(material)) {
            std::memcpy(&material, uniformData, sizeof(material));
        }
        
        if (size >= slotSize + sizeof(transform)) {
            std::memcpy(&transform, uniformData + slotSize, sizeof(transform));
        }
    }
    
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        Vec4 pos = attribs[0];
        gl_Position = transform.mvp * pos;
        
        ctx.varyings[0] = attribs[2]; // Pass UV
    }

    void fragment(const ShaderContext& ctx) {
        Vec4 uv = ctx.varyings[0];
        
        Vec4 color = material.diffuse;
        if (diffuseMap) {
            color = color * diffuseMap->sample(uv.x, uv.y);
        }
        
        if (specularMap) {
            // Simple visualization of specular map presence (add to red channel)
            // Real lighting calculation would go here
            Vec4 spec = specularMap->sample(uv.x, uv.y);
            color = color + spec * 0.5f; 
        }
        
        gl_FragColor = color;
    }
};

struct Entity {
    SharedAsset<MeshResource> mesh;
    SharedAsset<MaterialResource> material;
    Mat4 transform;
};

class RHIECSTest : public ITestCase {
public:
    framework::Camera camera;
    SharedAsset<Prefab> prefab;
    std::vector<Entity> entities;
    float rotation = 0.0f;
    
    ShaderHandle shaderHandle;
    PipelineHandle pipeline;

    void init(rhi::IGraphicsDevice* device) override {
        AssetManager::Get().Init(device);

        // 1. Register Shader
        ShaderDesc sDesc;
        sDesc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<ECSShader>>(ctx, pDesc);
        };
        // ... GLSL removed for brevity in this replace call if possible, 
        // but I must provide full context for 'replace' tool.
        sDesc.glsl.vertex = R"(#version 330 core
layout(location = 0) in vec4 aPos;
layout(location = 1) in vec4 aNormal;
layout(location = 2) in vec4 aTexCoord;

layout(std140) uniform MaterialData {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    float opacity;
    int alphaTest;
    int doubleSided;
};

layout(std140) uniform PerDrawData {
    mat4 mvp;
};

out vec2 TexCoord;

void main() {
    gl_Position = mvp * aPos;
    TexCoord = aTexCoord.xy;
}
)";
        sDesc.glsl.fragment = R"(#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

layout(std140) uniform MaterialData {
    vec4 diffuse;
    vec4 ambient;
    vec4 specular;
    vec4 emissive;
    float shininess;
    float opacity;
    int alphaTest;
    int doubleSided;
};

uniform sampler2D uDiffuseMap;
uniform sampler2D uSpecularMap;

void main() {
    vec4 texColor = texture(uDiffuseMap, TexCoord);
    vec4 specColor = texture(uSpecularMap, TexCoord);
    
    vec4 color = diffuse * texColor;
    // Simple specular contribution visualization
    color += specColor * 0.5;
    
    FragColor = vec4(color.rgb, diffuse.a * texColor.a);
}
)";
        shaderHandle = ShaderRegistry::GetInstance().Register("ECSShader", sDesc);

        // 2. Create Pipeline
        PipelineDesc pDesc;
        pDesc.shader = shaderHandle;
        pDesc.cullMode = CullMode::Back;
        pDesc.depthTestEnabled = true;
        pDesc.depthWriteEnabled = true;
        
        // Input Layout (Pos:0, Normal:1, UV:2)
        // Stride = 48 (12 floats)
        pDesc.inputLayout.stride = 48;
        pDesc.inputLayout.attributes = {
            { VertexFormat::Float4, 0, 0 },  // Pos
            { VertexFormat::Float4, 16, 1 }, // Normal
            { VertexFormat::Float4, 32, 2 }  // UV
        };
        
        pipeline = device->CreatePipeline(pDesc);

        // Load Prefab (Offline format expected)
        std::string modelPath = "assets/backpack/backpack.obj";
        // Load now returns SharedAsset<Prefab> directly
        prefab = AssetManager::Get().Load<Prefab>(modelPath);
        
        Prefab* prefabData = AssetManager::Get().GetPrefab(prefab.GetHandle());
        // No nullptr check needed strictly for GetPrefab anymore, but 'prefabData' could be fallback.
        // The fallback prefab has empty nodes, so this loop is safe even if fallback.
        if (prefabData) {
            for (const auto& node : prefabData->nodes) {
                Entity e;
                e.mesh = SharedAsset<MeshResource>(node.mesh);
                e.material = SharedAsset<MaterialResource>(node.material);
                e.transform = Mat4::Translate(0, 0, 0) * Mat4::Scale(1,1,1);
                entities.push_back(e);
            }
        }
        
        camera.position = Vec4(0.0f, 0.0f, 5.0f, 1.0f);
        camera.yaw = -90.0f;
        camera.pitch = 0.0f;
    }

    void destroy(rhi::IGraphicsDevice* device) override {
        entities.clear();
        prefab.Release();
        
        // Demonstrate GC
        AssetManager::Get().GarbageCollect();
        
        AssetManager::Get().Shutdown();
        device->DestroyPipeline(pipeline);
    }

    void onUpdate(float dt) override {
        camera.Update(dt);
        rotation += dt * 30.0f;
        if (rotation > 360.0f) rotation -= 360.0f;
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_label(ctx, "RHI ECS Test");
        mu_label(ctx, "Stateless Asset Management");
        
        if (mu_button(ctx, "Garbage Collect")) {
             AssetManager::Get().GarbageCollect();
        }
    }

    void onRender(rhi::IGraphicsDevice* device, int width, int height) override {
        CommandEncoder encoder;
        
        RenderPassDesc pass;
        pass.colorLoadOp = LoadAction::Clear;
        pass.clearColor[0] = 0.2f; pass.clearColor[1] = 0.2f; pass.clearColor[2] = 0.2f; pass.clearColor[3] = 1.0f;
        pass.depthLoadOp = LoadAction::Clear;
        pass.clearDepth = 1.0f;
        
        // Ensure we clear the whole viewport
        pass.renderArea = {0, 0, width, height};
        pass.initialViewport = {0, 0, width, height};
        
        encoder.BeginRenderPass(pass);
        encoder.SetPipeline(pipeline);

        Mat4 view = camera.GetViewMatrix();
        Mat4 proj = camera.GetProjectionMatrix();

        for (const auto& e : entities) {
            MeshResource* mesh = AssetManager::Get().GetMesh(e.mesh.GetHandle());
            MaterialResource* mat = AssetManager::Get().GetMaterial(e.material.GetHandle());
            
            // GetMesh/GetMaterial now guaranteed to return valid fallback if missing
            // So we can remove the 'if (mesh && mat)' check or keep it as sanity check.
            // Fallbacks have valid VBO/EBO (or empty but safe).

            // Bind Buffers
            if (mesh->vbo.IsValid()) encoder.SetVertexBuffer(mesh->vbo);
            if (mesh->ebo.IsValid()) encoder.SetIndexBuffer(mesh->ebo);
            
            // Bind Texture (Slot 0: Diffuse, Slot 1: Specular)
            // GetRHI works on handles, inside it calls GetTexture which returns fallback if needed.
            // Wait, we need rhiTextures from MaterialResource.
            // If Material is fallback, it has default values. 
            // The textures inside fallback material are Invalid AssetHandles.
            // We should check if handle is valid before accessing.
            
            if (mat->rhiTextures[0].IsValid()) {
                encoder.SetTexture(0, mat->rhiTextures[0]);
            }
            if (mat->rhiTextures[1].IsValid()) {
                encoder.SetTexture(1, mat->rhiTextures[1]);
            }

            // Update Uniforms
            // Slot 0: Material Data
            encoder.UpdateUniform(0, &mat->data, sizeof(MaterialResource::Data));

            // Slot 1: Transform
            PerDrawData tData;
            Mat4 model = e.transform * Mat4::RotateY(rotation);
            tData.mvp = proj * view * model;
            encoder.UpdateUniform(1, tData);

            // Draw
            if (mesh->indexCount > 0) {
                encoder.DrawIndexed(mesh->indexCount);
            }
        }

        encoder.EndRenderPass();
        encoder.SubmitTo(*device);
    }
};

static TestRegistrar registrar(TINYGL_TEST_GROUP, "ECS Architecture", []() { return new RHIECSTest(); });

