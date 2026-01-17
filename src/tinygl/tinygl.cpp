#include <algorithm> // for std::clamp, std::fill_n
#include <tinygl/tinygl.h>
namespace tinygl {

void SoftRenderContext::glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    m_state.viewport.x = x;
    m_state.viewport.y = y;
    m_state.viewport.w = w;
    m_state.viewport.h = h;
}

void SoftRenderContext::glPolygonMode(GLenum face, GLenum mode) {
    // 暂时只支持 GL_FRONT_AND_BACK
    if (face == GL_FRONT_AND_BACK) {
        m_state.polygonMode = mode;
    }
}

void SoftRenderContext::glClearColor(float r, float g, float b, float a) {
    m_clearColor = {r, g, b, a};
}


void SoftRenderContext::glClear(uint32_t buffersToClear) {
    int minX = 0, minY = 0, maxX = fbWidth, maxY = fbHeight;
    if (m_state.scissorTest) {
        minX = std::max(0, m_state.scissor.x);
        minY = std::max(0, m_state.scissor.y);
        maxX = std::min(fbWidth, m_state.scissor.x + m_state.scissor.w);
        maxY = std::min(fbHeight, m_state.scissor.y + m_state.scissor.h);
    }

    if (minX >= maxX || minY >= maxY) return;

    bool fullClear = (minX == 0 && minY == 0 && maxX == fbWidth && maxY == fbHeight);

    if (buffersToClear & GL_COLOR_BUFFER_BIT) {
        uint8_t R = (uint8_t)(std::clamp(m_clearColor.x, 0.0f, 1.0f) * 255);
        uint8_t G = (uint8_t)(std::clamp(m_clearColor.y, 0.0f, 1.0f) * 255);
        uint8_t B = (uint8_t)(std::clamp(m_clearColor.z, 0.0f, 1.0f) * 255);
        uint8_t A = (uint8_t)(std::clamp(m_clearColor.w, 0.0f, 1.0f) * 255);
        uint32_t clearColorInt = (A << 24) | (B << 16) | (G << 8) | R;
        
        if (fullClear) {
            std::fill_n(m_colorBufferPtr, fbWidth * fbHeight, clearColorInt);
        } else {
            for (int y = minY; y < maxY; ++y) {
                std::fill_n(m_colorBufferPtr + y * fbWidth + minX, maxX - minX, clearColorInt);
            }
        }
    }
    if (buffersToClear & GL_DEPTH_BUFFER_BIT) {
        // NOTE: glClear is NOT affected by glDepthMask in standard OpenGL, 
        // but it IS affected by it in some old versions. 
        // OpenGL 4.6 says: "The masked subset of the color, depth, and stencil buffers are cleared"
        // So we should respect the masks.
        if (m_state.depthMask) {
            if (fullClear) {
                std::fill(depthBuffer.begin(), depthBuffer.end(), m_state.clearDepth);
            } else {
                for (int y = minY; y < maxY; ++y) {
                    std::fill_n(depthBuffer.data() + y * fbWidth + minX, maxX - minX, m_state.clearDepth);
                }
            }
        }
    }
    if (buffersToClear & GL_STENCIL_BUFFER_BIT) {
        uint8_t s = (uint8_t)(m_state.clearStencil & 0xFF);
        // Stencil mask also affects glClear
        if (m_state.stencilWriteMask != 0) {
             if (fullClear && m_state.stencilWriteMask == 0xFF) {
                std::fill(stencilBuffer.begin(), stencilBuffer.end(), s);
            } else {
                for (int y = minY; y < maxY; ++y) {
                    uint8_t* row = stencilBuffer.data() + y * fbWidth + minX;
                    for (int x = 0; x < (maxX - minX); ++x) {
                        row[x] = (row[x] & ~m_state.stencilWriteMask) | (s & m_state.stencilWriteMask);
                    }
                }
            }
        }
    }
}

void SoftRenderContext::printContextState() {
    LOG_INFO("=== SoftRenderContext State ===");
    LOG_INFO("Framebuffer: " + std::to_string(fbWidth) + "x" + std::to_string(fbHeight));
    LOG_INFO("Viewport: x=" + std::to_string(m_state.viewport.x) + 
             ", y=" + std::to_string(m_state.viewport.y) + 
             ", w=" + std::to_string(m_state.viewport.w) + 
             ", h=" + std::to_string(m_state.viewport.h));
    
    LOG_INFO("--- Buffers State ---");
    LOG_INFO("Bound Array Buffer: " + std::to_string(m_boundArrayBuffer));
    LOG_INFO("Bound Vertex Array: " + std::to_string(m_boundVertexArray));
    LOG_INFO("Bound Copy Read Buffer: " + std::to_string(m_boundCopyReadBuffer));
    LOG_INFO("Bound Copy Write Buffer: " + std::to_string(m_boundCopyWriteBuffer));

    LOG_INFO("--- Active Buffers Details ---");
    for (size_t i = 1; i < buffers.size(); ++i) {
        if (buffers.isActive((GLuint)i)) {
            BufferObject* buf = buffers.get((GLuint)i);
            LOG_INFO("  Buffer #" + std::to_string(i) + 
                     ": Size=" + std::to_string(buf->data.size()) + 
                     " bytes, Usage=" + std::to_string(buf->usage) +
                     ", Immutable=" + (buf->immutable ? "True" : "False") +
                     ", Mapped=" + (buf->mapped ? "True" : "False"));
        }
    }

    LOG_INFO("--- Active VAOs Details ---");
    for (size_t i = 0; i < vaos.size(); ++i) {
        if (vaos.isActive((GLuint)i)) {
            VertexArrayObject* vao = vaos.get((GLuint)i);
            LOG_INFO("  VAO #" + std::to_string(i) + (i == m_boundVertexArray ? " (Bound)" : "") +
                     ", EBO=" + std::to_string(vao->elementBufferID) + 
                     ", Dirty=" + (vao->isDirty ? "True" : "False"));
            
            for(int j=0; j<MAX_ATTRIBS; ++j) {
                if(vao->attributes[j].enabled) {
                    LOG_INFO("    Attrib #" + std::to_string(j) + 
                             ": Binding=" + std::to_string(vao->attributes[j].bindingIndex) +
                             ", Size=" + std::to_string(vao->attributes[j].size) +
                             ", Type=" + std::to_string(vao->attributes[j].type) +
                             ", Norm=" + std::to_string(vao->attributes[j].normalized) +
                             ", RelOffset=" + std::to_string(vao->attributes[j].relativeOffset));
                }
            }
            for(int j=0; j<MAX_BINDINGS; ++j) {
                // Only print bindings that are used or have a buffer attached
                if(vao->bindings[j].bufferID != 0 || vao->bindings[j].stride != 0) {
                     LOG_INFO("    Binding #" + std::to_string(j) + 
                             ": Buffer=" + std::to_string(vao->bindings[j].bufferID) +
                             ", Offset=" + std::to_string(vao->bindings[j].offset) +
                             ", Stride=" + std::to_string(vao->bindings[j].stride) + 
                             ", Divisor=" + std::to_string(vao->bindings[j].divisor));
                }
            }
        }
    }

    LOG_INFO("--- Textures State ---");
    LOG_INFO("Active Texture Unit: " + std::to_string(m_activeTextureUnit));
    for (int i = 0; i < MAX_TEXTURE_UNITS; ++i) {
        if (m_boundTextures[i] != 0) {
            LOG_INFO("  Unit " + std::to_string(i) + ": Bound Texture " + std::to_string(m_boundTextures[i]));
        }
    }

    LOG_INFO("--- Active Textures Details ---");
    for (size_t i = 1; i < textures.size(); ++i) {
        if (textures.isActive((GLuint)i)) {
            TextureObject* tex = textures.get((GLuint)i);
            LOG_INFO("  Texture #" + std::to_string(i) + 
                     ": " + std::to_string(tex->width) + "x" + std::to_string(tex->height) + 
                     ", MipLevels=" + std::to_string(tex->mipLevels.size()) +
                     ", WrapS=" + std::to_string(tex->wrapS) +
                     ", WrapT=" + std::to_string(tex->wrapT) +
                     ", MinFilter=" + std::to_string(tex->minFilter) + 
                     ", MagFilter=" + std::to_string(tex->magFilter));
        }
    }

    LOG_INFO("--- Rasterizer State ---");
    LOG_INFO("Polygon Mode: " + std::to_string(m_state.polygonMode));
    LOG_INFO("Cull Face Mode: " + std::to_string(m_state.cullFaceMode));
    LOG_INFO("Front Face: " + std::to_string(m_state.frontFace));
    LOG_INFO("Clear Color: (" + std::to_string(m_clearColor.x) + ", " + 
             std::to_string(m_clearColor.y) + ", " + 
             std::to_string(m_clearColor.z) + ", " + 
             std::to_string(m_clearColor.w) + ")");

    LOG_INFO("--- Depth & Stencil ---");
    LOG_INFO("Depth Test Enabled: " + std::string(m_state.depthTest ? "TRUE" : "FALSE"));
    LOG_INFO("Depth Mask: " + std::string(m_state.depthMask ? "TRUE" : "FALSE"));
    LOG_INFO("Depth Func: " + std::to_string(m_state.depthFunc));
    
    LOG_INFO("Stencil Test Enabled: " + std::string(m_state.stencilTest ? "TRUE" : "FALSE"));
    LOG_INFO("Stencil Func: " + std::to_string(m_state.stencilFunc) + 
             ", Ref: " + std::to_string(m_state.stencilRef) + 
             ", Mask: " + std::to_string(m_state.stencilValueMask));
    LOG_INFO("Stencil Op (Fail, ZFail, ZPass): " + 
             std::to_string(m_state.stencilFail) + ", " + 
             std::to_string(m_state.stencilPassDepthFail) + ", " + 
             std::to_string(m_state.stencilPassDepthPass));
    LOG_INFO("Stencil Write Mask: " + std::to_string(m_state.stencilWriteMask));
    
    LOG_INFO("Cull Face Enabled: " + std::string(m_state.cullFace ? "TRUE" : "FALSE"));
    LOG_INFO("=============================");
}

}