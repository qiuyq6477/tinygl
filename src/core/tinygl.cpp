#define STB_IMAGE_IMPLEMENTATION
#include <tinygl/tinygl.h>
#include <algorithm> // for std::clamp, std::fill_n

namespace tinygl {

void SoftRenderContext::glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    m_viewport.x = x;
    m_viewport.y = y;
    m_viewport.w = w;
    m_viewport.h = h;
}

void SoftRenderContext::glPolygonMode(GLenum face, GLenum mode) {
    // 暂时只支持 GL_FRONT_AND_BACK
    if (face == GL_FRONT_AND_BACK) {
        m_polygonMode = mode;
    }
}

void SoftRenderContext::glClearColor(float r, float g, float b, float a) {
    m_clearColor = {r, g, b, a};
}


void SoftRenderContext::glClear(uint32_t buffersToClear) {
    if (buffersToClear & GL_COLOR_BUFFER_BIT) {
        uint8_t R = (uint8_t)(std::clamp(m_clearColor.x, 0.0f, 1.0f) * 255);
        uint8_t G = (uint8_t)(std::clamp(m_clearColor.y, 0.0f, 1.0f) * 255);
        uint8_t B = (uint8_t)(std::clamp(m_clearColor.z, 0.0f, 1.0f) * 255);
        uint8_t A = (uint8_t)(std::clamp(m_clearColor.w, 0.0f, 1.0f) * 255);
        uint32_t clearColorInt = (A << 24) | (B << 16) | (G << 8) | R;
        // Use std::fill_n on the pointer
        std::fill_n(m_colorBufferPtr, fbWidth * fbHeight, clearColorInt);
    }
    if (buffersToClear & GL_DEPTH_BUFFER_BIT) {
        std::fill(depthBuffer.begin(), depthBuffer.end(), DEPTH_INFINITY);
    }
}

}