#include <filesystem>
#include <vector>
#include <string>
#include <microui.h>
#include <tinygl/core/tinygl.h>
#include <tinygl/framework/application.h>
#include <tinygl/framework/camera.h>
#include <tinygl/framework/model.h>
#include <tinygl/framework/shader_pass.h>
#include "../ITestCase.h"
#include "../test_registry.h"

using namespace tinygl;
namespace fs = std::filesystem;

// Simple Blinn-Phong Shader
// 注意：现在不需要手动管理成员变量的赋值，ShaderPass 会自动注入
struct ModelShader : public ShaderBuiltins {
    // 1. 全局 Uniforms (自动注入)
    Mat4 model;
    Mat4 view;
    Mat4 projection;
    Vec4 viewPos;
    Vec4 lightPos;
    Vec4 lightColor;

    // 2. 材质数据 (自动注入)
    MaterialData materialData;

    // 3. 上下文 (自动注入)
    SoftRenderContext* renderCtx = nullptr;

    // Vertex Shader
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        Vec4 aPos = attribs[0];
        Vec4 aNormal = attribs[1];
        Vec4 aTexCoords = attribs[2];

        // World Space Position
        Vec4 worldPos = model * aPos;
        ctx.varyings[0] = worldPos; // FragPos
        
        // Normal
        Vec4 n = model * aNormal; // Simplified normal matrix
        ctx.varyings[1] = normalize(n); 

        // TexCoords
        ctx.varyings[2] = aTexCoords;

        gl_Position = projection * view * worldPos;
    }

    // Fragment Shader
    void fragment(const ShaderContext& ctx) {
        if (!renderCtx) return; // Should be set by ShaderPass now

        Vec4 fragPos = ctx.varyings[0];
        Vec4 normal = normalize(ctx.varyings[1]);
        Vec4 texCoords = ctx.varyings[2];

        // Fetch textures using bound units (Logic moved from user code to Mesh::Draw/ShaderPass)
        TextureObject* texDiff = renderCtx->getTexture(0);
        TextureObject* texSpec = renderCtx->getTexture(1);
        TextureObject* texOpacity = renderCtx->getTexture(5);

        // Alpha test
        float alpha = materialData.opacity;
        if (texOpacity) {
            alpha *= texOpacity->sample(texCoords.x, texCoords.y).x;
        } else if (texDiff) {
            alpha *= texDiff->sample(texCoords.x, texCoords.y).w;
        }

        if (materialData.alphaTest && alpha < materialData.alphaCutoff) {
            discard();
            return;
        }

        // Ambient
        Vec4 ambient = lightColor * materialData.ambient * 0.1f;

        // Diffuse
        Vec4 lightDir = normalize(lightPos - fragPos);
        float diff = std::max(dot(normal, lightDir), 0.0f);
        Vec4 diffuse = lightColor * materialData.diffuse * diff;

        // Specular
        Vec4 viewDir = normalize(viewPos - fragPos);
        Vec4 halfwayDir = normalize(lightDir + viewDir);
        float spec = std::pow(std::max(dot(normal, halfwayDir), 0.0f), materialData.shininess);
        Vec4 specular = lightColor * materialData.specular * spec;

        // Texture colors
        Vec4 texColor = texDiff ? texDiff->sample(texCoords.x, texCoords.y) : Vec4(1,1,1,1);
        Vec4 specColor = texSpec ? texSpec->sample(texCoords.x, texCoords.y) : Vec4(1,1,1,1);

        // Combine
        Vec4 result = (ambient + diffuse) * texColor + specular * specColor.x;
        result.w = alpha;
        
        gl_FragColor = result;
    }
};

// New: Normal Visualization Shader
struct NormalShader : public ShaderBuiltins {
    Mat4 model;
    Mat4 view;
    Mat4 projection;
    
    // We don't need lights or materials for this debug shader, 
    // but the system might try to bind them via if constexpr, so we can leave them out safely.

    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        Vec4 worldPos = model * attribs[0];
        // Pass World Normal to Fragment
        Vec4 n = model * attribs[1]; 
        ctx.varyings[0] = normalize(n); 
        gl_Position = projection * view * worldPos;
    }

    void fragment(const ShaderContext& ctx) {
        Vec4 normal = normalize(ctx.varyings[0]);
        // Map [-1, 1] to [0, 1]
        gl_FragColor = normal * 0.5f + Vec4(0.5f, 0.5f, 0.5f, 0.0f);
        gl_FragColor.w = 1.0f;
    }
};

class ModelLoadingTest : public ITestCase {
public:
    Camera camera;
    Model* currentModel = nullptr;
    
    // Shader Passes
    std::shared_ptr<ShaderPass<ModelShader>> phongPass;
    std::shared_ptr<ShaderPass<NormalShader>> normalPass;
    
    int currentShaderMode = 0; // 0 = Phong, 1 = Normal

    SoftRenderContext* m_ctx = nullptr;
    
    // UI State
    std::string loadStatus = "Select a model";
    std::vector<std::string> modelKeys;
    int selectedModelIdx = -1;
    float modelScale = 10.0f;

    ModelLoadingTest() : camera({.position = Vec4(0, 0, 3, 1)}) {
        phongPass = std::make_shared<ShaderPass<ModelShader>>();
        normalPass = std::make_shared<ShaderPass<NormalShader>>();
    }

    void init(SoftRenderContext& ctx) override {
        m_ctx = &ctx;
        ctx.glEnable(GL_DEPTH_TEST);
        scanModels();
    }

    void scanModels() {
        modelKeys.clear();
        std::string assetsPath = "model_loading/assets";
        if (fs::exists(assetsPath) && fs::is_directory(assetsPath)) {
            for (const auto& entry : fs::directory_iterator(assetsPath)) {
                if (entry.is_directory()) {
                    modelKeys.push_back(entry.path().filename().string());
                }
            }
        } else {
            loadStatus = "Error: Assets dir not found";
        }
    }

    void destroy(SoftRenderContext& ctx) override {
        if (currentModel) {
            delete currentModel;
            currentModel = nullptr;
        }
    }

    void onEvent(const SDL_Event& e) override {
        camera.ProcessEvent(e);
    }

    void onUpdate(float dt) override {
        camera.Update(dt);
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (currentModel) {
            // 构建 Render State
            RenderState state;
            state.view = camera.GetViewMatrix();
            state.projection = camera.GetProjectionMatrix();
            state.viewPos = camera.position;
            state.lightPos = Vec4(2.0f, 2.0f, 2.0f, 1.0f);
            state.lightColor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);

            // 模型矩阵
            Mat4 modelMatrix = Mat4::Translate(0.0f, -0.5f, 0.0f) * Mat4::Scale(modelScale, modelScale, modelScale);
            
            // Draw
            currentModel->Draw(modelMatrix, state);
        }
    }

    void applyShaderToModel() {
        if (!currentModel) return;
        std::shared_ptr<IShaderPass> targetPass = (currentShaderMode == 1) 
            ? std::static_pointer_cast<IShaderPass>(normalPass) 
            : std::static_pointer_cast<IShaderPass>(phongPass);

        // 【关键步骤】：为加载的 Mesh 分配 Shader
        // 在商业引擎中，这一步通常由 Material 序列化数据决定
        for (auto& mesh : currentModel->meshes) {
            mesh.material.shader = targetPass;
        }
    }

    void onGui(mu_Context* mu_ctx, const Rect& rect) override {
            mu_layout_row(mu_ctx, 1, (int[]) { -1 }, 0);
            mu_label(mu_ctx, "Select Model:");

            if (modelKeys.empty()) {
                mu_text(mu_ctx, "No models found in assets/");
            } else {
                for (int i = 0; i < (int)modelKeys.size(); ++i) {
                    if (mu_button(mu_ctx, modelKeys[i].c_str())) {
                        selectedModelIdx = i;
                        if (m_ctx) loadModelByKey(modelKeys[i], *m_ctx);
                    }
                }
            }
            mu_text(mu_ctx, loadStatus.c_str());
            
            if (currentModel) {
                mu_label(mu_ctx, "Shader Selection:");
                int oldMode = currentShaderMode;
                if (mu_header_ex(mu_ctx, "Shader Type", MU_OPT_EXPANDED)) {
                     if (mu_checkbox(mu_ctx, "Standard Phong", &currentShaderMode)) currentShaderMode = 0; // Checkbox logic tweak for radio behavior
                     // Simple Radio button emulation with buttons
                     if (mu_button(mu_ctx, currentShaderMode == 0 ? "[X] Standard Phong" : "[ ] Standard Phong")) currentShaderMode = 0;
                     if (mu_button(mu_ctx, currentShaderMode == 1 ? "[X] Normal Debug" : "[ ] Normal Debug")) currentShaderMode = 1;
                }
                
                if (oldMode != currentShaderMode) {
                    applyShaderToModel();
                }

                mu_label(mu_ctx, "Model Scale:");
                mu_slider(mu_ctx, &modelScale, 0.01f, 10.0f);
            }
    }

private:
    void loadModelByKey(const std::string& key, SoftRenderContext& ctx) {
        std::string path = "model_loading/assets/" + key + "/" + key + ".obj";
        loadModel(path, ctx);
    }

    void loadModel(const std::string& path, SoftRenderContext& ctx) {
        if (currentModel) {
            delete currentModel;
            currentModel = nullptr;
        }
        try {
            loadStatus = "Loading...";
            currentModel = new Model(path, ctx);
            
            // Apply current selected shader
            applyShaderToModel();

            if (currentModel->meshes.empty()) {
                loadStatus = "Failed: No meshes found";
            } else {
                loadStatus = "Loaded: " + path;
            }
        } catch (const std::exception& e) {
            loadStatus = "Error: " + std::string(e.what());
        }
    }
};

static TestRegistrar registry("Model", "Assimp Loader", []() { return new ModelLoadingTest(); });