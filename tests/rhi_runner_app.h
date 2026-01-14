#include <third_party/glad/glad.h>
#include "test_runner_app.h"
#include <rhi/soft_device.h>
#include <rhi/gl_device.h>
#include <framework/ui_renderer.h>

class RhiRunnerApp : public Application {
public:
    enum class Backend { Software, OpenGL };
    RhiRunnerApp(const AppConfig& config) : Application(config) {
        m_context = std::make_unique<SoftRenderContext>(config.width, config.height);
    }

    ~RhiRunnerApp() {
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

        // Initialize Device (Default to GL if available)
        setBackend(m_glContext ? Backend::OpenGL : Backend::Software);

        initBlitResources();

        printRegisteredTests();
        return true;
    }

    void setBackend(Backend b) {
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
            m_currentTest->destroy(*m_context);
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
            // --- OpenGL Backend ---
            
            // 1. Clear Default Framebuffer
            // glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // 2. Render Test
            if (m_currentTest && renderWidth > 0) {
                glViewport(0, 0, renderWidth, totalHeight);
                m_currentTest->onRender(*m_context); 
            }
            
            // 3. Render UI (RHI -> GL)
            rhi::CommandEncoder encoder;
            UIRenderer::render(&m_uiContext, encoder, totalWidth, totalHeight);
            m_device->Submit(encoder.GetBuffer());
            
            // 4. Present
            SDL_GL_SwapWindow(getWindow());

        } else if(m_backend == Backend::Software) {
            // --- Software Backend (via RHI SoftDevice) ---
            
            // 1. Clear Soft Context
            // m_context->glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            // m_context->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            
            // // 2. Render Test
            // if (m_currentTest && renderWidth > 0) {
            //     m_context->glViewport(0, 0, renderWidth, totalHeight);
            //     m_currentTest->onRender(*m_context);
            // }
            
            // 3. Render UI (RHI -> SoftDevice -> m_context)
            rhi::CommandEncoder encoder;
            UIRenderer::render(&m_uiContext, encoder, totalWidth, totalHeight);
            m_device->Submit(encoder.GetBuffer());
            
            // 4. Blit SoftBuffer to Screen using OpenGL Quad
            // This allows us to keep the GL Context active and just draw the software result as a texture.
            
            // Upload to Texture
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
    }

    void onGUI() override {         
        mu_Context* ctx = &m_uiContext;
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300;

        mu_Rect uiPanelRect = mu_rect(totalWidth - uiWidth, 0, uiWidth, totalHeight);

        mu_begin_window_ex(ctx, "Test Explorer", uiPanelRect, MU_OPT_NOCLOSE | MU_OPT_NORESIZE);
        
        const auto& allTests = TestCaseRegistry::get().getTests();
        for (const auto& groupPair : allTests) {
            const std::string& groupName = groupPair.first;
            
            mu_push_id(ctx, groupName.c_str(), groupName.size());
            if (mu_begin_treenode(ctx, groupName.c_str())) {
                for (const auto& testPair : groupPair.second) {
                    const std::string& testName = testPair.first;
                    
                    mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
                    
                    mu_push_id(ctx, testName.c_str(), testName.size());
                    if (mu_button(ctx, testName.c_str())) {
                        switchTest(groupName, testName);
                    }
                    mu_pop_id(ctx);
                }
                mu_end_treenode(ctx);
            }
            mu_pop_id(ctx);
        }

        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        
        if (m_currentTest) {
            mu_label(ctx, "Test Settings:");
            Rect rect = {uiPanelRect.x, uiPanelRect.y, uiPanelRect.w, uiPanelRect.h};
            m_currentTest->onGui(ctx, rect);
        } else {
            mu_label(ctx, "Select a test from the list.");
        }

        mu_end_window(ctx);
    }

    void switchTest(const std::string& group, const std::string& name) {
        if (m_selectedGroup == group && m_selectedTestName == name) return;

        try {
            if (m_currentTest) {
                m_currentTest->destroy(*m_context);
                delete m_currentTest;
                m_currentTest = nullptr;
            }

            const auto& allTests = TestCaseRegistry::get().getTests();
            if (allTests.count(group) && allTests.at(group).count(name)) {
                m_currentTest = allTests.at(group).at(name)();
                if (m_currentTest) {
                    m_currentTest->init(*m_context);
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
        const auto& allTests = TestCaseRegistry::get().getTests();
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

        // 1. Shader 源码 (保持 330 core 兼容 4.1)
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

        // 2. 编译逻辑
        auto compileShader = [](GLenum type, const char* src) -> GLuint {
            GLuint shader = 0;
            shader = glCreateShader(type);
            if (shader == 0) return 0;
            
            glShaderSource(shader, 1, &src, nullptr);
            glCompileShader(shader);
            GLint success;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
            if (!success) {
                char infoLog[512]; // 显式分配
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

        // 校验链接状态 (4.1 必须通过 glGetProgramiv 检查)
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
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
            1.0f, -1.0f, 1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f, 1.0f,
            1.0f, -1.0f, 1.0f, 0.0f,
            1.0f,  1.0f, 1.0f, 1.0f
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // 4.1 规范下，必须在 VAO 绑定期间启用并指定属性指针
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

    Backend m_backend = Backend::OpenGL;
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