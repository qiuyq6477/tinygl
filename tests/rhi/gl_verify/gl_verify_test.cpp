#include <framework/application.h>
#include <rhi/commands.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <iostream>
#include <cmath>

using namespace framework;
using namespace rhi;

// --- Backend Agnostic Shader ID ---
static constexpr uint32_t SHADER_TRIANGLE = 100;

class GLVerifyApp : public Application {
public:
    GLVerifyApp() : Application({
        "TinyGL - OpenGL Verification", 
        800, 600, 
        false, 
        AppConfig::Backend::OpenGL
    }) {}

protected:
    BufferHandle m_vbo = {0};
    PipelineHandle m_pipeline = {0};
    CommandEncoder m_encoder;

    bool onInit() override {
        IGraphicsDevice* device = getGraphicsDevice();
        if (!device) return false;

        std::cout << "Backend: OpenGL" << std::endl;

        // 1. Register Shader in Global Registry (Data-Driven)
        ShaderDesc triangleShader;
        // GL Backend Sources
        triangleShader.glsl.vertex = R"(
            #version 410 core
            layout(location = 0) in vec4 aPos;
            void main() {
                gl_Position = aPos;
            }
        )";
        triangleShader.glsl.fragment = R"(
            #version 410 core
            layout(std140) uniform Slot0 {
                vec4 uColor;
            };
            out vec4 FragColor;
            void main() {
                FragColor = uColor;
            }
        )";
        // (Optional: Soft Backend Factory could be added here too)
        
        ShaderRegistry::Get().Register(SHADER_TRIANGLE, triangleShader);

        // 2. Create Geometry
        float vertices[] = {
             0.0f,  0.5f, 0.0f, 1.0f,
            -0.5f, -0.5f, 0.0f, 1.0f,
             0.5f, -0.5f, 0.0f, 1.0f
        };
        BufferDesc bufDesc;
        bufDesc.type = BufferType::VertexBuffer;
        bufDesc.size = sizeof(vertices);
        bufDesc.initialData = vertices;
        m_vbo = device->CreateBuffer(bufDesc);

        // 3. Create Pipeline
        PipelineDesc pipeDesc;
        pipeDesc.shader = { SHADER_TRIANGLE };
        pipeDesc.inputLayout.stride = 4 * sizeof(float);
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float4, 0, 0 } // Loc 0
        };
        m_pipeline = device->CreatePipeline(pipeDesc);

        return true;
    }

    void onDestroy() override {
        IGraphicsDevice* device = getGraphicsDevice();
        if (device) {
            device->DestroyPipeline(m_pipeline);
            device->DestroyBuffer(m_vbo);
        }
    }

    void onRender() override {
        IGraphicsDevice* device = getGraphicsDevice();
        if (device) {
            m_encoder.Reset();
            
            m_encoder.Clear(0.1f, 0.1f, 0.12f, 1.0f);

            // Update Color via Slot 0
            static float time = 0.0f;
            time += 0.02f;
            struct {
                float r, g, b, a;
            } colorData = { (sinf(time) + 1.0f) * 0.5f, 0.8f, 0.2f, 1.0f };
            m_encoder.UpdateUniform(0, colorData);

            m_encoder.SetPipeline(m_pipeline);
            m_encoder.SetVertexBuffer(m_vbo);
            m_encoder.Draw(3);

            m_encoder.SubmitTo(*device);
        }
    }
};

int main(int argc, char** argv) {
    GLVerifyApp app;
    app.run();
    return 0;
}