#include <third_party/glad/glad.h>
#include <rhi/soft_device.h>
#include <rhi/gl_device.h>
#include <framework/ui_renderer.h>
#include <framework/application.h>
#include <optional>
#include <test_registry.h>

using namespace tinygl;
using namespace framework;

class FrameworkRunnerApp : public Application {
public:
    enum class Backend { Software, OpenGL };
    FrameworkRunnerApp(const AppConfig& config) : Application(config) {
        m_context = std::make_unique<SoftRenderContext>(config.width, config.height);
    }

    ~FrameworkRunnerApp() {
        if (m_blitTexture) glDeleteTextures(1, &m_blitTexture);
        if (m_blitVAO) glDeleteVertexArrays(1, &m_blitVAO);
        if (m_blitVBO) glDeleteBuffers(1, &m_blitVBO);
        if (m_blitProgram) glDeleteProgram(m_blitProgram);
        
        if (m_glContext) SDL_GL_DeleteContext(m_glContext);
        UIRenderer::shutdown();
    }

protected:
    bool onInit() override {
        // Initialize OpenGL Context
        m_glContext = SDL_GL_CreateContext(getWindow());
        if (!m_glContext) {
            std::cerr << "Failed to create OpenGL context: " << SDL_GetError() << std::endl;
            return false;
        }

        // Initialize GLAD
        if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            return false;
        }

        // Initialize Device (Default to Software for testing TBR)
        doSetBackend(Backend::Software);

        initBlitResources();

        printRegisteredTests();
        return true;
    }

    void setBackend(Backend b) {
        m_nextBackend = b;
    }

    void doSetBackend(Backend b) {
        if (b == Backend::OpenGL && !m_glContext) {
            std::cerr << "Cannot switch to OpenGL: No Context" << std::endl;
            return;
        }

        UIRenderer::shutdown();
        m_backend = b;
        m_device.reset(); // Destroy old device
        if (m_backend == Backend::OpenGL) {
            LOG_INFO("Switching RHI Backend to OpenGL");
            m_device = std::make_unique<rhi::GLDevice>();
            SDL_SetWindowTitle(getWindow(), (getConfig().title + " [Backend: OpenGL]").c_str());
        } else {
            LOG_INFO("Switching RHI Backend to Software");
            m_device = std::make_unique<rhi::SoftDevice>(*m_context);
            SDL_SetWindowTitle(getWindow(), (getConfig().title + " [Backend: Software]").c_str());
        }
        // Initialize RHI UI Renderer
        UIRenderer::init(&m_uiContext, m_device.get());
    }
    
    void onDestroy() override {
        if (m_currentTest) {
            m_currentTest->destroy(m_device.get());
            delete m_currentTest;
            m_currentTest = nullptr;
        }
    }

    void onUpdate(float dt) override {
        if (m_currentTest) {
            m_currentTest->onUpdate(dt);
        }
    }

    void onEvent(const SDL_Event& event) override {
        UIRenderer::processInput(&m_uiContext, event);

        if (m_uiContext.hover_root != nullptr) {
            if (event.type == SDL_MOUSEWHEEL || event.type == SDL_MOUSEBUTTONDOWN) {
                return;
            }
        }

        if (m_currentTest) {
            m_currentTest->onEvent(event);
        }
    }
    
    void renderFrame() override {
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300; 
        int renderWidth = totalWidth - uiWidth;

        // Prepare UI
        mu_begin(&m_uiContext);
        onGUI();
        mu_end(&m_uiContext);

        if (m_backend == Backend::OpenGL) {
            if (m_currentTest && renderWidth > 0) {
                m_currentTest->onRender(m_device.get(), renderWidth, totalHeight); 
            }
            
            rhi::CommandEncoder encoder;
            UIRenderer::render(&m_uiContext, encoder, totalWidth, totalHeight);
            m_device->Submit(encoder.GetBuffer());
            
            SDL_GL_SwapWindow(getWindow());

        } else if(m_backend == Backend::Software) {
            if (m_currentTest && renderWidth > 0) {
                m_currentTest->onRender(m_device.get(), renderWidth, totalHeight);
            }
            
            rhi::CommandEncoder encoder;
            UIRenderer::render(&m_uiContext, encoder, totalWidth, totalHeight);
            m_device->Submit(encoder.GetBuffer());
            
            uint32_t* buffer = m_context->getColorBuffer();
            if (buffer) {
                glBindTexture(GL_TEXTURE_2D, m_blitTexture);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, getWidth(), getHeight(), GL_RGBA, GL_UNSIGNED_BYTE, buffer);
            }

            // Draw Full Screen Quad
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, getWidth(), getHeight());
            glClear(GL_COLOR_BUFFER_BIT); // Optional if we cover everything
            
            glUseProgram(m_blitProgram);
            glBindVertexArray(m_blitVAO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, m_blitTexture);
            glUniform1i(glGetUniformLocation(m_blitProgram, "uTexture"), 0);
            
            glDrawArrays(GL_TRIANGLES, 0, 6);
            
            // 5. Present
            SDL_GL_SwapWindow(getWindow());
        }

        // Handle deferred backend switch
        if (m_nextBackend.has_value()) {
            Backend next = *m_nextBackend;
            m_nextBackend = std::nullopt;
            
            std::string group = m_selectedGroup;
            std::string name = m_selectedTestName;

            if (m_currentTest) {
                m_currentTest->destroy(m_device.get());
                delete m_currentTest;
                m_currentTest = nullptr;
            }

            doSetBackend(next);

            if (!group.empty() && !name.empty()) {
                m_selectedGroup = ""; // Force reload
                switchTest(group, name);
            }
        }
    }

    // Helper for Tree Construction
    struct TestNode {
        std::map<std::string, TestNode> children;
        std::vector<std::pair<std::string, std::string>> tests; // Name, FullGroupKey
    };

    void onGUI() override {         
        mu_Context* ctx = &m_uiContext;
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300;

        mu_Rect uiPanelRect = mu_rect(totalWidth - uiWidth, 0, uiWidth, totalHeight);

        mu_begin_window_ex(ctx, "Test Explorer", uiPanelRect, MU_OPT_NOCLOSE | MU_OPT_NORESIZE);
        
        // Backend Toggles
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        const char* backendLabel = (m_backend == Backend::OpenGL) ? "Backend: OpenGL" : "Backend: Software";
        if (mu_button(ctx, backendLabel)) {
            Backend next = (m_backend == Backend::OpenGL) ? Backend::Software : Backend::OpenGL;
            setBackend(next);
        }

        // Build Tree (Cache this if performance becomes an issue)
        TestNode root;
        const auto& allTests = TestCaseRegistry::get().getRHITests();
        for (const auto& groupPair : allTests) {
            std::string groupPath = groupPair.first;
            std::string fullOriginalPath = groupPair.first; // Keep original for lookup
            
            // Strip "framework/" prefix and the last directory component (e.g., "framework/rhi/basic_triangle" -> "rhi")
            // This groups tests directly under their category folder.
            const std::string prefix = "framework/";
            if (groupPath.rfind(prefix, 0) == 0) {
                groupPath = groupPath.substr(prefix.length());
            }
            
            size_t lastSlash = groupPath.find_last_of('/');
            if (lastSlash != std::string::npos) {
                groupPath = groupPath.substr(0, lastSlash);
            }

            TestNode* current = &root;
            
            // Split path manually
            size_t start = 0;
            size_t end = groupPath.find('/');
            while (end != std::string::npos) {
                std::string token = groupPath.substr(start, end - start);
                current = &current->children[token];
                start = end + 1;
                end = groupPath.find('/', start);
            }
            std::string lastToken = groupPath.substr(start);
            if (!lastToken.empty()) {
                current = &current->children[lastToken];
            }
            
            // Add tests to this leaf node
            for (const auto& testPair : groupPair.second) {
                current->tests.push_back({testPair.first, fullOriginalPath});
            }
        }

        // Render Tree
        renderTree(ctx, root);

        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        
        if (m_currentTest) {
            mu_label(ctx, "Test Settings:");
            tinygl::Rect rect = {uiPanelRect.x, uiPanelRect.y, uiPanelRect.w, uiPanelRect.h};
            m_currentTest->onGui(ctx, rect);
        } else {
            mu_label(ctx, "Select a test from the list.");
        }

        mu_end_window(ctx);
    }

    void renderTree(mu_Context* ctx, const TestNode& node) {
        // Render Children Folders
        for (const auto& childPair : node.children) {
            const std::string& folderName = childPair.first;
            mu_push_id(ctx, folderName.c_str(), folderName.size());
            if (mu_begin_treenode(ctx, folderName.c_str())) {
                renderTree(ctx, childPair.second);
                mu_end_treenode(ctx);
            }
            mu_pop_id(ctx);
        }

        // Render Tests in this folder
        for (const auto& testPair : node.tests) {
            const std::string& testName = testPair.first;
            const std::string& fullGroup = testPair.second;
            
            mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
            mu_push_id(ctx, testName.c_str(), testName.size());
            if (mu_button(ctx, testName.c_str())) {
                switchTest(fullGroup, testName);
            }
            mu_pop_id(ctx);
        }
    }

    void switchTest(const std::string& group, const std::string& name) {
        if (m_selectedGroup == group && m_selectedTestName == name) return;

        try {
            if (m_currentTest) {
                m_currentTest->destroy(m_device.get());
                delete m_currentTest;
                m_currentTest = nullptr;
            }

            const auto& allTests = TestCaseRegistry::get().getRHITests();
            if (allTests.count(group) && allTests.at(group).count(name)) {
                m_currentTest = allTests.at(group).at(name)();
                if (m_currentTest) {
                    m_currentTest->init(m_device.get());
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[TestRunner] Exception caught during test switch: " << e.what() << std::endl;
            m_currentTest = nullptr;
        }

        m_selectedGroup = group;
        m_selectedTestName = name;
    }

    void printRegisteredTests() {
        std::cout << "-------------------------" << std::endl;
        std::cout << "Registered Test Cases:" << std::endl;
        const auto& allTests = TestCaseRegistry::get().getRHITests();
        if (allTests.empty()) {
            std::cout << "  (None)" << std::endl;
        } else {
            for (const auto& groupPair : allTests) {
                std::cout << "  - Group: " << groupPair.first << std::endl;
                for (const auto& testPair : groupPair.second) {
                    std::cout << "    - " << testPair.first << std::endl;
                }
            }
        }
        std::cout << "-------------------------" << std::endl;
    }

private:
    void initBlitResources() {
        if (!m_glContext) return;

        const char* vsSource = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            out vec2 TexCoord;
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";

        const char* fsSource = R"(
            #version 330 core
            out vec4 FragColor;
            in vec2 TexCoord;
            uniform sampler2D uTexture;
            void main() {
                FragColor = texture(uTexture, TexCoord);
            }
        )";

        auto compileShader = [](GLenum type, const char* src) -> GLuint {
            GLuint shader = 0;
            shader = glCreateShader(type);
            if (shader == 0) return 0;
            
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);
            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512]; 
                glGetShaderInfoLog(shader, 512, nullptr, infoLog);
                std::cerr << "Shader Error: " << infoLog << std::endl;
            }
            return shader;
        };

        GLuint vs = compileShader(GL_VERTEX_SHADER, vsSource);
        GLuint fs = compileShader(GL_FRAGMENT_SHADER, fsSource);
        
        m_blitProgram = glCreateProgram();
        glAttachShader(m_blitProgram, vs);
        glAttachShader(m_blitProgram, fs);
        glLinkProgram(m_blitProgram);

        GLint linkSuccess;
        glGetProgramiv(m_blitProgram, GL_LINK_STATUS, &linkSuccess);
        if (!linkSuccess) {
            char infoLog[512];
            glGetProgramInfoLog(m_blitProgram, 512, nullptr, infoLog);
            std::cerr << "Link Error: " << infoLog << std::endl;
        }
        glDeleteShader(vs);
        glDeleteShader(fs);

        glGenTextures(1, &m_blitTexture);
        glBindTexture(GL_TEXTURE_2D, m_blitTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, getWidth(), getHeight(), 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0);

        glGenVertexArrays(1, &m_blitVAO);
        glGenBuffers(1, &m_blitVBO);

        glBindVertexArray(m_blitVAO);
        glBindBuffer(GL_ARRAY_BUFFER, m_blitVBO);
        float vertices[] = {
            -1.0f,  1.0f, 0.0f, 0.0f,
            -1.0f, -1.0f, 0.0f, 1.0f,
            1.0f, -1.0f, 1.0f, 1.0f,
            -1.0f,  1.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 1.0f, 1.0f,
            1.0f,  1.0f, 1.0f, 0.0f
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glUseProgram(m_blitProgram);
        GLint texLoc;
        texLoc = glGetUniformLocation(m_blitProgram, "uTexture");
        if (texLoc != -1) {
            glUniform1i(texLoc, 0);
        }
        glUseProgram(0);
    }

    Backend m_backend = Backend::Software;
    std::optional<Backend> m_nextBackend;
    SDL_GLContext m_glContext = nullptr;
    std::unique_ptr<SoftRenderContext> m_context;
    std::unique_ptr<rhi::IGraphicsDevice> m_device;
    mu_Context m_uiContext;
    ITestCase* m_currentTest = nullptr;
    std::string m_selectedGroup;
    std::string m_selectedTestName;
    // Blit Resources
    GLuint m_blitTexture = 0;
    GLuint m_blitVAO = 0;
    GLuint m_blitVBO = 0;
    GLuint m_blitProgram = 0;
};

int main(int argc, char** argv) {
    AppConfig config;
    config.title = "TinyGL RHI Runner";
    config.width = 1280;
    config.height = 720;
    config.resizable = false;
    config.windowFlags = SDL_WINDOW_OPENGL;
    FrameworkRunnerApp app(config);
    app.run();
    return 0;
}
