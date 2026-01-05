#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/core/tinygl.h>
#include <vector>
#include <string>

using namespace tinygl;

class CopyBufferTest : public ITestCase {
public:
    void init(SoftRenderContext& ctx) override {
        float data1[] = { 1.0f, 2.0f, 3.0f, 4.0f };
        float data2[] = { 0.0f, 0.0f, 0.0f, 0.0f };

        ctx.glGenBuffers(1, &m_vbo1);
        ctx.glBindBuffer(GL_COPY_READ_BUFFER, m_vbo1);
        ctx.glBufferData(GL_COPY_READ_BUFFER, sizeof(data1), data1, GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &m_vbo2);
        ctx.glBindBuffer(GL_COPY_WRITE_BUFFER, m_vbo2);
        ctx.glBufferData(GL_COPY_WRITE_BUFFER, sizeof(data2), data2, GL_STATIC_DRAW);

        // Copy first 2 floats
        ctx.glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, 2 * sizeof(float));
        
        // Copy last 2 floats to the end
        ctx.glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 2 * sizeof(float), 2 * sizeof(float), 2 * sizeof(float));

        // Verify
        float* mapped = (float*)ctx.glMapBuffer(GL_COPY_WRITE_BUFFER, GL_READ_ONLY);
        if (mapped) {
            bool success = true;
            for (int i = 0; i < 4; ++i) {
                if (std::abs(mapped[i] - data1[i]) > 1e-5f) {
                    success = false;
                    LOG_ERROR("CopyBufferTest Failed at index " + std::to_string(i) + ": expected " + std::to_string(data1[i]) + ", got " + std::to_string(mapped[i]));
                }
            }
            if (success) {
                LOG_INFO("CopyBufferTest Passed!");
                m_passed = true;
            }
            ctx.glUnmapBuffer(GL_COPY_WRITE_BUFFER);
        } else {
            LOG_ERROR("CopyBufferTest Failed: Could not map buffer");
        }
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo1);
        ctx.glDeleteBuffers(1, &m_vbo2);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        // No specific UI needed for this test
    }

    void onRender(SoftRenderContext& ctx) override {
        if (m_passed) {
            ctx.glClearColor(0.0f, 1.0f, 0.0f, 1.0f); // Green
        } else {
            ctx.glClearColor(1.0f, 0.0f, 0.0f, 1.0f); // Red
        }
        ctx.glClear(GL_COLOR_BUFFER_BIT);
    }

private:
    GLuint m_vbo1 = 0;
    GLuint m_vbo2 = 0;
    bool m_passed = false;
};

static TestRegistrar registrar("Basic", "CopyBuffer", []() { return new CopyBufferTest(); });
