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

// --- Backend Agnostic Shader ID ---
static const std::string SHADER_TRIANGLE = "TriangleShader";

class GLVerifyApp : public Application {
public:
    GLVerifyApp() : Application({
        "TinyGL - OpenGL Verification", 
        1280, 720, 
        false, 
        SDL_WINDOW_OPENGL
    }) {}

protected:
    std::unique_ptr<IGraphicsDevice> m_device;
    BufferHandle m_vbo = {0};
    PipelineHandle m_pipeline = {0};
    CommandEncoder m_encoder;

    SDL_GLContext m_glContext = nullptr;
    mu_Context m_uiContext;

    bool onInit() override {
        // 1. Create OpenGL Context manually since Application doesn't do it anymore
        m_glContext = SDL_GL_CreateContext(getWindow());
        if (!m_glContext) {
            std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            return false;
        }

        m_device = std::make_unique<GLDevice>();
        IGraphicsDevice* device = m_device.get();
        if (!device) return false;

        std::cout << "Backend: OpenGL" << std::endl;

        // 1. Register Shader in Global Registry (Data-Driven)
        // GL Backend Sources
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
        // (Optional: Soft Backend Factory could be added here too)
        
        ShaderHandle shaderHandle = ShaderRegistry::GetInstance().RegisterShader(SHADER_TRIANGLE, vertex, fragment);

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
        pipeDesc.shader = shaderHandle;
        pipeDesc.inputLayout.stride = 4 * sizeof(float);
        pipeDesc.inputLayout.attributes = {
            { VertexFormat::Float4, 0, 0 } // Loc 0
        };
        m_pipeline = device->CreatePipeline(pipeDesc);

        UIRenderer::init(&m_uiContext, m_device.get());

        return true;
    }

    void onDestroy() override {
        IGraphicsDevice* device = m_device.get();
        if (device) {
            device->DestroyPipeline(m_pipeline);
            device->DestroyBuffer(m_vbo);
        }
        m_device.reset(); // Destroy device before context
        if (m_glContext) SDL_GL_DeleteContext(m_glContext);
    }

    void renderFrame() override {
        mu_begin(&m_uiContext);
        onGUI();
        mu_end(&m_uiContext);

        IGraphicsDevice* device = m_device.get();
        if (device) {
            m_encoder.Reset();
            
            RenderPassDesc pass;
            pass.colorLoadOp = LoadAction::Clear;
            pass.clearColor[0] = 0.1f;
            pass.clearColor[1] = 0.1f;
            pass.clearColor[2] = 0.12f;
            pass.clearColor[3] = 1.0f;
            
            int totalWidth = getWidth();
            int totalHeight = getHeight();
            int uiWidth = 300; 
            int renderWidth = totalWidth - uiWidth;
            pass.initialViewport = {0, 0, renderWidth, getHeight()};
            pass.initialScissor = {0, 0, renderWidth, getHeight()};
        
            m_encoder.BeginRenderPass(pass);

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

            m_encoder.EndRenderPass();
            
            m_encoder.SubmitTo(*device);

            rhi::CommandEncoder encoder;
            UIRenderer::render(&m_uiContext, encoder, getWidth(), getHeight());
            device->Submit(encoder.GetBuffer());
            // LOG_INFO("Submitting UI Command Buffer with size: " + std::to_string(encoder.GetBuffer().GetSize()) + " bytes");

            SDL_GL_SwapWindow(getWindow());
        }
    }

    void onGUI() override {         
        mu_Context* ctx = &m_uiContext;
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300;

        mu_Rect uiPanelRect = mu_rect(totalWidth - uiWidth, 0, uiWidth, totalHeight);

        mu_begin_window_ex(ctx, "Test Explorer", uiPanelRect, MU_OPT_NOCLOSE | MU_OPT_NORESIZE);
        
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        
            mu_label(ctx, "Select a test from the list.");

        mu_end_window(ctx);
    }

    void onEvent(const SDL_Event& event) override {
        UIRenderer::processInput(&m_uiContext, event);

        if (m_uiContext.hover_root != nullptr) {
            if (event.type == SDL_MOUSEWHEEL || event.type == SDL_MOUSEBUTTONDOWN) {
                return;
            }
        }
    }
};

int main(int argc, char** argv) {
    GLVerifyApp app;
    app.run();
    return 0;
}