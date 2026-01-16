#include <framework/application.h>
#include <rhi/commands.h>
#include <rhi/encoder.h>
#include <rhi/shader_registry.h>
#include <rhi/gl_device.h>
#include <iostream>
#include <cmath>
#include <string>

using namespace framework;
using namespace rhi;

class GLBlendScissorApp : public Application {
public:
    GLBlendScissorApp() : Application({
        "GL Blend & Scissor Test", 
        800, 600, 
        false, 
        SDL_WINDOW_OPENGL
    }) {}

protected:
    std::unique_ptr<IGraphicsDevice> m_device;
    SDL_GLContext m_glContext = nullptr;
    BufferHandle m_vbo = {0};
    PipelineHandle m_opaquePipeline = {0};
    PipelineHandle m_blendPipeline = {0};
    CommandEncoder m_encoder;

    bool onInit() override {
        // 1. Create OpenGL Context manually
        m_glContext = SDL_GL_CreateContext(getWindow());
        if (!m_glContext) {
            std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            return false;
        }
        
        m_device = std::make_unique<GLDevice>();
        IGraphicsDevice* device = m_device.get();
        if (!device) return false;

        std::cout << "Backend: OpenGL" << std::endl;

        // 1. Register Shader
        std::string vertex = R"(
            #version 410 core
            layout(location = 0) in vec4 aPos;
            void main() {
                gl_Position = aPos;
            }
        )";
        std::string fragment = R"(
            #version 410 core
            layout(std140) uniform Slot0 {
                vec4 uColor;
            };
            out vec4 FragColor;
            void main() {
                FragColor = uColor;
            }
        )";
        ShaderHandle shaderHandle = ShaderRegistry::GetInstance().RegisterShader("ColorShader", vertex, fragment);

        // 2. Create Geometry (Quad centered at origin)
        float vertices[] = {
             -0.5f, -0.5f, 0.0f, 1.0f,
              0.5f, -0.5f, 0.0f, 1.0f,
              0.5f,  0.5f, 0.0f, 1.0f,
             -0.5f, -0.5f, 0.0f, 1.0f,
              0.5f,  0.5f, 0.0f, 1.0f,
             -0.5f,  0.5f, 0.0f, 1.0f,
        };
        BufferDesc bufDesc;
        bufDesc.type = BufferType::VertexBuffer;
        bufDesc.size = sizeof(vertices);
        bufDesc.initialData = vertices;
        m_vbo = device->CreateBuffer(bufDesc);

        // 3. Create Opaque Pipeline
        PipelineDesc opaqueDesc;
        opaqueDesc.shader = shaderHandle;
        opaqueDesc.inputLayout.stride = 4 * sizeof(float);
        opaqueDesc.inputLayout.attributes = { { VertexFormat::Float4, 0, 0 } };
        opaqueDesc.depthTestEnabled = false; // Disable depth test for 2D overlay
        m_opaquePipeline = device->CreatePipeline(opaqueDesc);

        // 4. Create Blend Pipeline
        PipelineDesc blendDesc = opaqueDesc;
        blendDesc.blend.enabled = true;
        blendDesc.blend.srcRGB = BlendFactor::SrcAlpha;
        blendDesc.blend.dstRGB = BlendFactor::OneMinusSrcAlpha;
        m_blendPipeline = device->CreatePipeline(blendDesc);

        return true;
    }

    void onDestroy() override {
        IGraphicsDevice* device = m_device.get();
        if (device) {
            device->DestroyPipeline(m_opaquePipeline);
            device->DestroyPipeline(m_blendPipeline);
            device->DestroyBuffer(m_vbo);
        }
        m_device.reset();
        if (m_glContext) SDL_GL_DeleteContext(m_glContext);
    }

    void renderFrame() override {
        IGraphicsDevice* device = m_device.get();
        if (device) {
            m_encoder.Reset();
            
            // 1. Clear Full Screen (Disable Scissor first)
            m_encoder.SetScissor(0, 0, -1, -1); // Disable Scissor
            m_encoder.Clear(0.2f, 0.2f, 0.2f, 1.0f);

            // 2. Set Scissor for Drawing (Clip left half)
            m_encoder.SetScissor(0, 0, 400, 600);

            // 3. Draw Opaque Quad (Red)
            struct { float r, g, b, a; } red = { 1.0f, 0.0f, 0.0f, 1.0f };
            m_encoder.UpdateUniform(0, red);
            
            m_encoder.SetPipeline(m_opaquePipeline);
            m_encoder.SetVertexBuffer(m_vbo);
            
            // Translate the quad to show overlap. 
            // Since we don't have matrix uniforms in this simple shader, we rely on the vertex positions.
            // The VBO defines a quad from (0,0) to (1,1).
            // Let's just draw it. It will cover top-right quadrant.
            m_encoder.Draw(6);

            // 3. Draw Blended Quad (Green, 50% alpha) - Same position
            struct { float r, g, b, a; } green = { 0.0f, 1.0f, 0.0f, 0.5f };
            m_encoder.UpdateUniform(0, green);
            
            m_encoder.SetPipeline(m_blendPipeline);
            // Draw again, should blend on top of red
            m_encoder.Draw(6);
            
            m_encoder.SubmitTo(*device);
            device->Present();

            SDL_GL_SwapWindow(getWindow());
        }
    }
};

int main(int argc, char** argv) {
    GLBlendScissorApp app;
    app.run();
    return 0;
}
