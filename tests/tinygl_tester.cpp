#pragma once
#include <iostream>
#include <framework/application.h>
#include <tinygl/tinygl.h>
#include "ITestCase.h"
#include "test_registry.h"
#include <framework/ui_renderer_fast.h>

using namespace tinygl;
using namespace framework;

class TestRunnerApp : public Application {
public:
    TestRunnerApp(const AppConfig& config) : Application(config) {
        m_context = std::make_unique<SoftRenderContext>(config.width, config.height);
    }

    virtual ~TestRunnerApp() {
        if (m_texture) SDL_DestroyTexture(m_texture);
        if (m_renderer) SDL_DestroyRenderer(m_renderer);
    }

protected:
    bool onInit() override {
        // If we requested an OpenGL window, we likely want to manage the context ourselves (e.g. RHI Runner)
        // Creating an SDL_Renderer on top might conflict with SDL_GL_CreateContext on some platforms.
        if (getConfig().windowFlags & SDL_WINDOW_OPENGL) {
            // Initialize UI (Fast)
            UIRendererFast::init(&m_uiContext);
            printRegisteredTests();
            return true;
        }

        // Initialize SDL Renderer for Software Blitting
        // Note: SDL_CreateRenderer might fail if Window was created with OpenGL flag on some platforms/drivers
        // if we try to create a software renderer on top. 
        // But usually providing -1 and SDL_RENDERER_ACCELERATED works if we want a 2D accelerator.
        // If we are in RHI mode (OpenGL window), we might not want to create this renderer HERE if we plan to use GL.
        // However, the mandate says TestRunner handles software rendering.
        // We will defer creation or handle failure gracefully? 
        // Actually, if we are in GL mode, we can still use SDL_Renderer if the driver supports it, 
        // OR we should avoid creating it if we are going to use pure GL.
        
        // Let's assume for now we always create it for the base TestRunner functionality,
        // unless the subclass overrides onInit and avoids it?
        // But TestRunnerApp::onInit needs to be called.
        
        // We will try to create it.
        m_renderer = SDL_CreateRenderer(getWindow(), -1, SDL_RENDERER_ACCELERATED);
        if (!m_renderer) {
            // Fallback to software renderer
             m_renderer = SDL_CreateRenderer(getWindow(), -1, SDL_RENDERER_SOFTWARE);
        }
        
        if (m_renderer) {
             m_texture = SDL_CreateTexture(
                m_renderer,
                SDL_PIXELFORMAT_RGBA32,
                SDL_TEXTUREACCESS_STREAMING,
                getWidth(), getHeight()
            );
        }

        // Initialize UI (Fast)
        UIRendererFast::init(&m_uiContext);

        printRegisteredTests();

        return true;
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
        UIRendererFast::processInput(&m_uiContext, event);

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
        // 1. Clear Soft Context
        m_context->glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        m_context->glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 2. Prepare UI
        mu_begin(&m_uiContext);
        onGUI();
        mu_end(&m_uiContext);

        // 3. Render Scene
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300; 
        int renderWidth = totalWidth - uiWidth;

        if (m_currentTest && renderWidth > 0) {
            m_context->glViewport(0, 0, renderWidth, totalHeight);
            m_currentTest->onRender(*m_context);
        }

        // 4. Render UI (Fast Soft Rasterization)
        m_context->glViewport(0, 0, totalWidth, totalHeight);
        UIRendererFast::render(&m_uiContext, *m_context);

        // 5. Blit to SDL Window
        presentSoftBuffer();
    }

    void presentSoftBuffer() {
        if (!m_renderer || !m_texture) return;
        
        uint32_t* buffer = m_context->getColorBuffer();
        if (buffer) {
            SDL_UpdateTexture(
                m_texture,
                NULL,
                buffer,
                getWidth() * sizeof(uint32_t)
            );
            SDL_RenderClear(m_renderer);
            SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
            SDL_RenderPresent(m_renderer);
        }
    }

    void onGUI() override {
        mu_Context* ctx = &m_uiContext;
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300;

        mu_Rect uiPanelRect = mu_rect(totalWidth - uiWidth, 0, uiWidth, totalHeight);

        mu_begin_window_ex(ctx, "Test Explorer", uiPanelRect, MU_OPT_NOCLOSE | MU_OPT_NORESIZE);
        
        const auto& allTests = TestCaseRegistry::get().getTinyGLTests();
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

            m_context->glBindVertexArray(0);
            m_context->glBindBuffer(GL_ARRAY_BUFFER, 0);
            m_context->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
            m_context->glBindTexture(GL_TEXTURE_2D, 0);

            const auto& allTests = TestCaseRegistry::get().getTinyGLTests();
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
        const auto& allTests = TestCaseRegistry::get().getTinyGLTests();
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

protected:
    std::unique_ptr<SoftRenderContext> m_context;
    mu_Context m_uiContext;
    ITinyGLTestCase* m_currentTest = nullptr;
    std::string m_selectedGroup;
    std::string m_selectedTestName;
    
    SDL_Renderer* m_renderer = nullptr;
    SDL_Texture* m_texture = nullptr;
};

int main(int argc, char** argv) {
    AppConfig config;
    config.title = "TinyGL Test Runner";
    config.width = 1280;
    config.height = 720;
    config.resizable = true;
    config.windowFlags = 0; // Pure Software Mode

    TestRunnerApp app(config);
    app.run();
    return 0;
}
