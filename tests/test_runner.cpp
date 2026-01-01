#include <tinygl/application.h>
#include <tinygl/tinygl.h>
#include <microui.h>
#include "ITestCase.h"
#include "test_registry.h"
#include <iostream>

using namespace tinygl;

class TestRunnerApp : public Application {
public:
    TestRunnerApp() : Application({"TinyGL Test Runner", 1280, 720, true}) {}

protected:
    bool onInit() override {
        // Print registered tests for verification
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

        return true;
    }

    void onDestroy() override {
        if (m_currentTest) {
            m_currentTest->destroy(getContext());
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
        mu_Context* ctx = getUIContext();
        if (ctx->hover_root != nullptr) {
            if (event.type == SDL_MOUSEWHEEL || event.type == SDL_MOUSEBUTTONDOWN) {
                return;
            }
        }

        if (m_currentTest) {
            m_currentTest->onEvent(event);
        }
    }

    void onRender() override {
        auto& ctx = getContext();
        
        // Clear the entire screen first (background color)
        ctx.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Calculate layout
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300; // Fixed width for UI panel
        int renderWidth = totalWidth - uiWidth;

        // 1. Render Scene (Left Side)
        if (m_currentTest && renderWidth > 0) {
            // Set viewport to the left side
            // Note: glViewport(x, y, w, h) where (x,y) is bottom-left
            // So x=0, y=0 is correct for the left side
            ctx.glViewport(0, 0, renderWidth, totalHeight);
            
            // Let the test render
            m_currentTest->onRender(ctx);
        }

        // 2. Render UI (Right Side) is handled in onGUI because MicroUI handles its own rendering commands
        // However, we reset viewport for any other global rendering if needed
        ctx.glViewport(0, 0, totalWidth, totalHeight);
    }

    void onGUI() override {
        mu_Context* ctx = getUIContext();
        int totalWidth = getWidth();
        int totalHeight = getHeight();
        int uiWidth = 300;

        // Define the UI area rect (Right side)
        mu_Rect uiPanelRect = mu_rect(totalWidth - uiWidth, 0, uiWidth, totalHeight);

        // Force window position and size every frame to act as a fixed panel
        // mu_Container* win = mu_get_container(ctx, "Test Explorer");
        // if (win) {
        //     win->rect = uiPanelRect;
        // }

        // --- Main Test Selector Panel ---
        mu_begin_window_ex(ctx, "Test Explorer", uiPanelRect, MU_OPT_NOCLOSE | MU_OPT_NORESIZE);
        
        // Tree View for Groups
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

        // Separator
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        
        // --- Test Case Specific UI ---
        if (m_currentTest) {
            mu_label(ctx, "Test Settings:");
            // Pass a rect relative to the window content area if needed, 
            // but for now let's just use the flow layout.
            // We create a sub-rect for the test case simply by knowing we are in the window.
            Rect rect = {uiPanelRect.x, uiPanelRect.y, uiPanelRect.w, uiPanelRect.h};
            m_currentTest->onGui(ctx, rect);
        } else {
            mu_label(ctx, "Select a test from the list.");
        }

        mu_end_window(ctx);
    }

private:
    void switchTest(const std::string& group, const std::string& name) {
        if (m_selectedGroup == group && m_selectedTestName == name) return;

        auto& ctx = getContext();

        // 1. Destroy old test
        if (m_currentTest) {
            m_currentTest->destroy(ctx);
            delete m_currentTest;
            m_currentTest = nullptr;
        }

        // 2. Create new test
        const auto& allTests = TestCaseRegistry::get().getTests();
        if (allTests.count(group) && allTests.at(group).count(name)) {
            m_currentTest = allTests.at(group).at(name)();
            if (m_currentTest) {
                m_currentTest->init(ctx);
            }
        }

        m_selectedGroup = group;
        m_selectedTestName = name;
    }

private:
    ITestCase* m_currentTest = nullptr;
    std::string m_selectedGroup;
    std::string m_selectedTestName;
};

int main(int argc, char** argv) {
    TestRunnerApp app;
    app.run();
    return 0;
}
