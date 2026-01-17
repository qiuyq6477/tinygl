#include <ITestCase.h>
#include <test_registry.h>
#include <tinygl/core/linear_allocator.h>
#include <vector>
#include <iostream>

using namespace tinygl;

class AllocatorTest : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        LinearAllocator allocator;
        size_t size = 1024; // 1KB
        allocator.Init(size);

        // Test 1: Basic Allocation
        int* p1 = allocator.New<int>(10);
        if (!p1) {
            std::cerr << "Test Failed: Basic allocation returned null" << std::endl;
        } else {
            for(int i=0; i<10; ++i) p1[i] = i;
        }

        // Test 2: Offset check (40 bytes, aligned to 8 -> 40)
        size_t expectedUsed = sizeof(int) * 10;
        if (expectedUsed % 8 != 0) expectedUsed += 8 - (expectedUsed % 8);
        
        if (allocator.GetUsedMemory() != expectedUsed) {
             std::cerr << "Test Failed: Memory usage mismatch. Expected " << expectedUsed << ", got " << allocator.GetUsedMemory() << std::endl;
        }
        
        // Test 3: Overflow
        // Try to allocate rest
        size_t remaining = size - allocator.GetUsedMemory();
        void* p2 = allocator.Allocate(remaining);
        if (!p2) std::cerr << "Test Failed: Allocating remaining failed" << std::endl;

        // Try to allocate more
        void* p3 = allocator.Allocate(1);
        if (p3) std::cerr << "Test Failed: Overflow protection failed" << std::endl;

        // Test 4: Reset
        allocator.Reset();
        if (allocator.GetUsedMemory() != 0) std::cerr << "Test Failed: Reset failed" << std::endl;
        
        // Check reuse
        void* p4 = allocator.Allocate(100);
        if (!p4) std::cerr << "Test Failed: Re-allocation after reset failed" << std::endl;
        
        std::cout << "Allocator Test Completed (Check stderr for errors)" << std::endl;
    }
    
    void onRender(SoftRenderContext& ctx) override {
        // Nothing to render
        ctx.glClearColor(0, 1, 0, 1);
        ctx.glClear(GL_COLOR_BUFFER_BIT);
    }
    
    void destroy(SoftRenderContext& ctx) override {}
    void onUpdate(float dt) override {}
    void onEvent(const SDL_Event& e) override {}
    void onGui(mu_Context* ctx, const Rect& rect) override {}
};

static TestRegistrar registry("Core", "AllocatorVerify", []() { return new AllocatorTest(); });