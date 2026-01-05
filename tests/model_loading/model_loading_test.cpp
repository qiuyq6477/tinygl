#include <filesystem>
#include <vector>
#include <string>
#include <tinygl/third_party/microui.h>
#include <tinygl/core/tinygl.h>
#include <tinygl/framework/application.h>
#include <tinygl/framework/camera.h>
#include <tinygl/framework/model.h>
#include "../ITestCase.h"
#include "../test_registry.h"

using namespace tinygl;
namespace fs = std::filesystem;

// Simple Blinn-Phong Shader
struct ModelShader : public ShaderBuiltins {
    // Uniforms
    Mat4 model;
    Mat4 view;
    Mat4 projection;
    
    Vec4 viewPos;
    Vec4 lightPos;
    Vec4 lightColor = {1.0f, 1.0f, 1.0f, 1.0f};

    // Material Uniforms (set by Mesh usually, but we can set defaults)
    // Mesh::Draw binds Diffuse to Unit 0, Specular to Unit 1
    TextureObject* texDiff = nullptr;
    TextureObject* texSpec = nullptr;

    // Vertex Shader
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[0]: Position
        // attribs[1]: Normal
        // attribs[2]: TexCoords
        
        Vec4 aPos = attribs[0];
        Vec4 aNormal = attribs[1];
        Vec4 aTexCoords = attribs[2];

        // World Space Position
        Vec4 worldPos = model * aPos;
        ctx.varyings[0] = worldPos; // Store FragPos in varying 0
        
        // Normal (simplified, better to use Normal Matrix)
        Vec4 n = model * aNormal;
        ctx.varyings[1] = normalize(n); // Store Normal in varying 1

        // TexCoords
        ctx.varyings[2] = aTexCoords;

        gl_Position = projection * view * worldPos;
    }

    // Fragment Shader
    void fragment(const ShaderContext& ctx) {
        Vec4 fragPos = ctx.varyings[0];
        Vec4 normal = normalize(ctx.varyings[1]);
        Vec4 texCoords = ctx.varyings[2];

        if (!renderCtx) return; // Error pink

        // Ambient
        float ambientStrength = 0.1f;
        Vec4 ambient = lightColor * ambientStrength;

        // Diffuse
        Vec4 lightDir = normalize(lightPos - fragPos);
        float diff = std::max(dot(normal, lightDir), 0.0f);
        Vec4 diffuse = lightColor * diff;

        // Specular
        float specularStrength = 0.5f;
        Vec4 viewDir = normalize(viewPos - fragPos);
        Vec4 halfwayDir = normalize(lightDir + viewDir);
        float spec = std::pow(std::max(dot(normal, halfwayDir), 0.0f), 32.0f);
        Vec4 specular = lightColor * spec;

        // Texture colors
        Vec4 diffColor = texDiff ? texDiff->sample(texCoords.x, texCoords.y) : Vec4(1.0f, 1.0f, 1.0f, 1.0f);

        Vec4 specColor = texSpec ? texSpec->sample(texCoords.x, texCoords.y) : Vec4(0.5f, 0.5f, 0.5f, 1.0f);

        // Combine
        Vec4 result = (ambient + diffuse) * diffColor + specular * specColor.x;
        result.w = diffColor.w;
        
        gl_FragColor = result;
    }

    SoftRenderContext* renderCtx = nullptr;
};

class ModelLoadingTest : public ITestCase {
public:
    Camera camera;
    Model* currentModel = nullptr;
    ModelShader shader;
    SoftRenderContext* m_ctx = nullptr;
    
    // UI State
    std::string loadStatus = "Select a model";
    std::vector<std::string> modelKeys;
    int selectedModelIdx = -1;
    float modelScale = 10.0f;

    ModelLoadingTest() : camera(Vec4(0, 0, 3, 1)) {}

    void init(SoftRenderContext& ctx) override {
        shader.renderCtx = &ctx;
        m_ctx = &ctx;
        
        // Enable Depth Test
        ctx.glEnable(GL_DEPTH_TEST);

        // Scan for models
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
        // Clear
        ctx.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (currentModel) {
            shader.model = Mat4::Translate(0.0f, -0.5f, 0.0f) * Mat4::Scale(modelScale, modelScale, modelScale); 
            shader.view = camera.GetViewMatrix();
            shader.projection = camera.GetProjectionMatrix();
            shader.viewPos = camera.position;
            shader.lightPos = Vec4(2.0f, 2.0f, 2.0f, 1.0f); 
            shader.texDiff = ctx.getTexture(0);
            shader.texSpec = ctx.getTexture(1);
            currentModel->Draw(ctx, shader);
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
                mu_label(mu_ctx, "Model Scale:");
                mu_slider(mu_ctx, &modelScale, 0.01f, 10.0f);

                mu_label(mu_ctx, "Model Stats:");
                
                char buf[128];
                snprintf(buf, sizeof(buf), "Meshes: %zu", currentModel->meshes.size());
                mu_text(mu_ctx, buf);

                size_t totalVerts = 0;
                size_t totalIndices = 0;
                for (const auto& mesh : currentModel->meshes) {
                    totalVerts += mesh.vertices.size();
                    totalIndices += mesh.indices.size();
                }

                snprintf(buf, sizeof(buf), "Total Verts: %zu", totalVerts);
                mu_text(mu_ctx, buf);
                
                snprintf(buf, sizeof(buf), "Total Indices: %zu", totalIndices);
                mu_text(mu_ctx, buf);
                
                snprintf(buf, sizeof(buf), "Textures Loaded: %zu", currentModel->loadedTextures.size());
                mu_text(mu_ctx, buf);
                
                if (mu_header_ex(mu_ctx, "Mesh Details", MU_OPT_CLOSED)) {
                    for (size_t i = 0; i < currentModel->meshes.size(); ++i) {
                        const auto& mesh = currentModel->meshes[i];
                        snprintf(buf, sizeof(buf), "Mesh %zu: %zu v, %zu i", i, mesh.vertices.size(), mesh.indices.size());
                        mu_text(mu_ctx, buf);
                    }
                }
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
            if (currentModel->meshes.empty()) {
                loadStatus = "Failed: No meshes found";
                delete currentModel;
                currentModel = nullptr;
            } else {
                loadStatus = "Loaded: " + path;
            }
        } catch (const std::exception& e) {
            loadStatus = "Error: " + std::string(e.what());
            if (currentModel) delete currentModel;
            currentModel = nullptr;
        }
    }
};

static TestRegistrar registry("Model", "Assimp Loader", []() { return new ModelLoadingTest(); });
