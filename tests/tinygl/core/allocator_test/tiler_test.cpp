#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/core/tiler.h>
#include <iostream>

using namespace tinygl;

class TilerTest : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        TileBinningSystem tiler;
        tiler.Init(800, 600, 64);

        TriangleData tri;
        // Triangle covering a bit of the center
        tri.p[0] = { 400.0f, 300.0f, 0.5f, 1.0f };
        tri.p[1] = { 450.0f, 300.0f, 0.5f, 1.0f };
        tri.p[2] = { 400.0f, 350.0f, 0.5f, 1.0f };

        tiler.BinTriangle(tri, 1, 0, 0);

        int count = 0;
        for (int y = 0; y < tiler.GetGridHeight(); ++y) {
            for (int x = 0; x < tiler.GetGridWidth(); ++x) {
                if (!tiler.GetTile(x, y).commands.empty()) {
                    count++;
                }
            }
        }

        if (count == 0) {
            std::cerr << "Test Failed: No tiles binned!" << std::endl;
        } else {
            std::cout << "Tiler Test: Triangle binned to " << count << " tiles." << std::endl;
        }
    }
    
    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0, 0, 1, 1);
        ctx.glClear(GL_COLOR_BUFFER_BIT);
    }
    
    void destroy(SoftRenderContext& ctx) override {}
    void onUpdate(float dt) override {}
    void onEvent(const SDL_Event& e) override {}
    void onGui(mu_Context* ctx, const Rect& rect) override {}
};

static TestRegistrar registry(TINYGL_TEST_GROUP, "TilerVerify", []() { return new TilerTest(); });
