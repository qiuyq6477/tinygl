#include <tinygl/tinygl.h>

namespace tinygl {


// ==========================================
// TextureObject
// ==========================================

void TextureObject::generateMipmaps() {
    if (mipLevels.empty()) return;
    
    int currentLevel = 0;
    while (true) {
        int srcW = mipLevels[currentLevel].width;
        int srcH = mipLevels[currentLevel].height;

        if (srcW <= 1 && srcH <= 1) break;

        int nextW = std::max(1, srcW / 2);
        int nextH = std::max(1, srcH / 2);
        
        // Calculate size for Tiled Storage (aligned to 4x4)
        int blocksX = (nextW + 3) / 4;
        int blocksY = (nextH + 3) / 4;
        size_t nextSize = blocksX * blocksY * 16;
        
        size_t newOffset = data.size();
        data.resize(newOffset + nextSize);
        
        // Push back first
        mipLevels.push_back({newOffset, nextW, nextH});

        uint32_t* dstPtr = data.data() + newOffset;

        // Helper for Tiled Index
        auto getTiledAddr = [](int x, int y, int w) -> size_t {
            int bx = x / 4; int by = y / 4;
            int lx = x % 4; int ly = y % 4;
            int blocksW = (w + 3) / 4;
            return (by * blocksW + bx) * 16 + (ly * 4 + lx);
        };

        for (int y = 0; y < nextH; ++y) {
            for (int x = 0; x < nextW; ++x) {
                int srcX = x * 2;
                int srcY = y * 2;
                
                // Read from Source (already tiled) using getTexelRaw
                // getTexelRaw handles the Tiled addressing of the source level
                Vec4 c00 = getTexelRaw(*this, currentLevel, srcX, srcY);
                Vec4 c10 = getTexelRaw(*this, currentLevel, std::min(srcX + 1, srcW - 1), srcY);
                Vec4 c01 = getTexelRaw(*this, currentLevel, srcX, std::min(srcY + 1, srcH - 1));
                Vec4 c11 = getTexelRaw(*this, currentLevel, std::min(srcX + 1, srcW - 1), std::min(srcY + 1, srcH - 1));

                Vec4 avg = (c00 + c10 + c01 + c11) * 0.25f;
                
                uint32_t R = (uint32_t)(avg.x * 255.0f);
                uint32_t G = (uint32_t)(avg.y * 255.0f);
                uint32_t B = (uint32_t)(avg.z * 255.0f);
                uint32_t A = (uint32_t)(avg.w * 255.0f);
                
                // Write to Dest (needs Tiled addressing)
                size_t destIdx = getTiledAddr(x, y, nextW);
                dstPtr[destIdx] = (A << 24) | (B << 16) | (G << 8) | R;
            }
        }
        currentLevel++;
    }
    LOG_INFO("Generated " + std::to_string(mipLevels.size()) + " mipmap levels (Tiled).");
}

// 辅助宏：检查 MagFilter 并赋值
#define CHECK_MAG(MIN, MAG, WRAPS, WRAPT) \
    if (magFilter == MAG) { \
        activeSampler = &FilterPolicy<MIN, MAG, WRAPS, WRAPT>::sample; \
        return; \
    }

// 辅助宏：为每个 MinFilter 生成 case，内部检查 MagFilter
#define CASE_MIN(MIN, WRAPS, WRAPT) \
    case MIN: \
        CHECK_MAG(MIN, GL_NEAREST, WRAPS, WRAPT) \
        CHECK_MAG(MIN, GL_LINEAR,  WRAPS, WRAPT) \
        break;

// 辅助宏：展开所有 MinFilter 模式
#define DISPATCH_FILTERS(WRAPS, WRAPT) \
    switch (minFilter) { \
        CASE_MIN(GL_NEAREST, WRAPS, WRAPT) \
        CASE_MIN(GL_LINEAR,  WRAPS, WRAPT) \
        CASE_MIN(GL_NEAREST_MIPMAP_NEAREST, WRAPS, WRAPT) \
        CASE_MIN(GL_LINEAR_MIPMAP_NEAREST,  WRAPS, WRAPT) \
        CASE_MIN(GL_NEAREST_MIPMAP_LINEAR,  WRAPS, WRAPT) \
        CASE_MIN(GL_LINEAR_MIPMAP_LINEAR,   WRAPS, WRAPT) \
    }

void TextureObject::updateSampler() {
    if (wrapS == wrapT) {
        switch (wrapS) {
            case GL_REPEAT: 
                DISPATCH_FILTERS(WrapPolicy<GL_REPEAT>, WrapPolicy<GL_REPEAT>); 
                break;
            case GL_CLAMP_TO_EDGE: 
                DISPATCH_FILTERS(WrapPolicy<GL_CLAMP_TO_EDGE>, WrapPolicy<GL_CLAMP_TO_EDGE>); 
                break;
            case GL_MIRRORED_REPEAT:
                DISPATCH_FILTERS(WrapPolicy<GL_MIRRORED_REPEAT>, WrapPolicy<GL_MIRRORED_REPEAT>);
                break;
            case GL_CLAMP_TO_BORDER:
                DISPATCH_FILTERS(WrapPolicy<GL_CLAMP_TO_BORDER>, WrapPolicy<GL_CLAMP_TO_BORDER>);
                break;
            default:
                DISPATCH_FILTERS(WrapPolicy<GL_REPEAT>, WrapPolicy<GL_REPEAT>);
                break;
        }
    } else {
        if (wrapS == GL_REPEAT && wrapT == GL_CLAMP_TO_EDGE) {
            DISPATCH_FILTERS(WrapPolicy<GL_REPEAT>, WrapPolicy<GL_CLAMP_TO_EDGE>);
        }
        else if (wrapS == GL_CLAMP_TO_EDGE && wrapT == GL_REPEAT) {
            DISPATCH_FILTERS(WrapPolicy<GL_CLAMP_TO_EDGE>, WrapPolicy<GL_REPEAT>);
        }
        else {
            auto getModeName = [](GLenum mode) -> const char* {
                switch (mode) {
                    case GL_REPEAT: return "GL_REPEAT";
                    case GL_MIRRORED_REPEAT: return "GL_MIRRORED_REPEAT";
                    case GL_CLAMP_TO_EDGE: return "GL_CLAMP_TO_EDGE";
                    case GL_CLAMP_TO_BORDER: return "GL_CLAMP_TO_BORDER";
                    default: return "UNKNOWN";
                }
            };
            
            char buf[128];
            snprintf(buf, sizeof(buf), "Mixed wrap mode unsupported: S=%s, T=%s. Fallback to Repeat.", 
                     getModeName(wrapS), getModeName(wrapT));
            LOG_WARN(buf);
            DISPATCH_FILTERS(WrapPolicy<GL_REPEAT>, WrapPolicy<GL_REPEAT>);
        }
    }
}


// ==========================================
// SoftRenderContext
// ==========================================
void SoftRenderContext::glGenTextures(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = textures.allocate();
        // 【Fix】: 立即创建 Texture 对象，防止后续 lookup 失败
        TextureObject* tex = textures.get(res[i]);
        if (tex) tex->id = res[i];
        LOG_INFO("GenTexture ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glActiveTexture(GLenum texture) {
    if (texture >= GL_TEXTURE0 && texture < GL_TEXTURE0 + 32) m_activeTextureUnit = texture - GL_TEXTURE0;
}

void SoftRenderContext::glBindTexture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D) {
        m_boundTextures[m_activeTextureUnit] = texture;
        // 【Robustness】: 如果绑定的 ID 合法但 Map 中不存在（某些用法允许Bind时创建），则补建
        if (texture != 0 && !textures.isActive(texture)) {
            textures.forceAllocate(texture)->id = texture;
            LOG_INFO("Implicitly created Texture ID: " + std::to_string(texture));
        }
    }
}
TextureObject* SoftRenderContext::getTextureObject(GLuint id) {
    return textures.get(id);
}

TextureObject* SoftRenderContext::getTexture(GLuint unit) {
    if (unit >= 32) return nullptr;
    GLuint id = m_boundTextures[unit];
    return textures.get(id);
}

void SoftRenderContext::glDeleteTextures(GLsizei n, const GLuint* textures_to_delete) {
    for (GLsizei i = 0; i < n; ++i) {
        GLuint id = textures_to_delete[i];
        if (id != 0) {
            // Check all texture units and unbind if necessary
            for (int unit = 0; unit < MAX_TEXTURE_UNITS; ++unit) {
                if (m_boundTextures[unit] == id) {
                    m_boundTextures[unit] = 0;
                }
            }

            if (textures.isActive(id)) {
                textures.release(id);
                LOG_INFO("Deleted Texture ID: " + std::to_string(id));
            }
        }
    }
}

void SoftRenderContext::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* p) {
    auto* tex = getTexture(m_activeTextureUnit); if(!tex) return;

    // Phase 1: Basic Parameter Handling
    if (target != GL_TEXTURE_2D) {
        LOG_WARN("glTexImage2D: Only GL_TEXTURE_2D is supported for target.");
        return;
    }
    if (border != 0) {
        LOG_WARN("glTexImage2D: Border must be 0.");
        return;
    }
    if (internalformat != GL_RGBA) {
        LOG_WARN("glTexImage2D: Only GL_RGBA internalformat is fully supported for storage.");
        // Continue with GL_RGBA storage regardless
    }

    tex->width = w; 
    tex->height = h;

    // Handle level metadata
    if (level >= static_cast<int>(tex->mipLevels.size())) {
        tex->mipLevels.resize(level + 1);
    }
    
    // Calculate new size needed with 4x4 tiling alignment
    // Even if user provides w,h, we store in blocks of 4x4
    int blocksX = (w + 3) / 4;
    int blocksY = (h + 3) / 4;
    size_t sizeNeeded = blocksX * blocksY * 16;
    
    // Simplification: Assume Level 0 is uploaded first and resets the buffer.
    if (level == 0) {
        tex->data.resize(sizeNeeded);
        tex->mipLevels[0] = {0, w, h};
        // Clear other levels if they existed from previous usage
        tex->mipLevels.resize(1); 
    } else {
        // Appending a level
        size_t currentEnd = tex->data.size();
        tex->data.resize(currentEnd + sizeNeeded);
        tex->mipLevels[level] = {currentEnd, w, h};
    }

    // Convert and Copy Data with SWIZZLING
    if (p) {
        std::vector<uint32_t> temp;
        // Convert input to linear uint32_t buffer first
        if (convertToInternalFormat(p, w, h, format, type, temp)) {
            size_t offset = tex->mipLevels[level].offset;
            uint32_t* destBase = tex->data.data() + offset;
            
            // Swizzle Loop
            // Iterate over destination blocks
            for (int by = 0; by < blocksY; ++by) {
                for (int bx = 0; bx < blocksX; ++bx) {
                    // Block Base Index in Destination
                    int blockIdx = by * blocksX + bx;
                    uint32_t* blockPtr = destBase + blockIdx * 16;
                    
                    // Iterate pixels in block
                    for (int ly = 0; ly < 4; ++ly) {
                        for (int lx = 0; lx < 4; ++lx) {
                            int srcX = bx * 4 + lx;
                            int srcY = by * 4 + ly;
                            
                            // Check bounds
                            if (srcX < w && srcY < h) {
                                blockPtr[ly * 4 + lx] = temp[srcY * w + srcX];
                            } else {
                                // Padding (transparent black)
                                blockPtr[ly * 4 + lx] = 0;
                            }
                        }
                    }
                }
            }
        } else {
            LOG_ERROR("glTexImage2D: Failed to convert source pixel data.");
        }
    }
}

void SoftRenderContext::glTexParameteri(GLenum target, GLenum pname, GLint param) {
    if (target != GL_TEXTURE_2D) return;
    TextureObject* tex = getTexture(m_activeTextureUnit);
    if (!tex) return;

    switch (pname) {
        case GL_TEXTURE_WRAP_S: tex->wrapS = (GLenum)param; tex->updateSampler(); break;
        case GL_TEXTURE_WRAP_T: tex->wrapT = (GLenum)param; tex->updateSampler(); break;
        case GL_TEXTURE_MIN_FILTER: tex->minFilter = (GLenum)param; tex->updateSampler(); break;
        case GL_TEXTURE_MAG_FILTER: tex->magFilter = (GLenum)param; tex->updateSampler(); break;
        case GL_TEXTURE_MIN_LOD: tex->minLOD = (float)param; break;
        case GL_TEXTURE_MAX_LOD: tex->maxLOD = (float)param; break;
        case GL_TEXTURE_LOD_BIAS: tex->lodBias = (float)param; break;
        default: break;
    }
}

void SoftRenderContext::glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
    if (target != GL_TEXTURE_2D) return;
    TextureObject* tex = getTexture(m_activeTextureUnit);
    if (!tex) return;

    switch (pname) {
        case GL_TEXTURE_MIN_LOD: tex->minLOD = param; break;
        case GL_TEXTURE_MAX_LOD: tex->maxLOD = param; break;
        case GL_TEXTURE_LOD_BIAS: tex->lodBias = param; break;
        // 对于 Enum 类型的参数，允许用 float 传进来 (兼容性)
        case GL_TEXTURE_WRAP_S: tex->wrapS = (GLenum)param; tex->updateSampler(); break;
        case GL_TEXTURE_WRAP_T: tex->wrapT = (GLenum)param; tex->updateSampler(); break;
        case GL_TEXTURE_MIN_FILTER: tex->minFilter = (GLenum)param; tex->updateSampler(); break;
        case GL_TEXTURE_MAG_FILTER: tex->magFilter = (GLenum)param; tex->updateSampler(); break;
    }
}

void SoftRenderContext::glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
    if (target != GL_TEXTURE_2D || !params) return;
    TextureObject* tex = getTexture(m_activeTextureUnit);
    if (!tex) return;

    switch (pname) {
        case GL_TEXTURE_BORDER_COLOR:
            // 将 Int 颜色 (0-2147483647 或 0-255? OpenGL 这里的 int 实际上是映射到 0-1)
            // 标准规定：Int 值会线性映射。通常 0 -> 0.0, MAX_INT -> 1.0
            // 但为了简化软渲染器，我们假设用户传入的是 0-255 范围
            tex->borderColor = Vec4(params[0]/255.0f, params[1]/255.0f, params[2]/255.0f, params[3]/255.0f);
            break;
        default:
            glTexParameteri(target, pname, params[0]);
            break;
    }
}

void SoftRenderContext::glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
    if (target != GL_TEXTURE_2D || !params) return;
    TextureObject* tex = getTexture(m_activeTextureUnit);
    if (!tex) return;

    switch (pname) {
        case GL_TEXTURE_BORDER_COLOR:
            tex->borderColor = Vec4(params[0], params[1], params[2], params[3]);
            break;
        default:
            // 如果用户传的是标量参数的指针形式
            glTexParameterf(target, pname, params[0]);
            break;
    }
}

void SoftRenderContext::glGenerateMipmap(GLenum target) {
    if (target != GL_TEXTURE_2D) {
        LOG_WARN("glGenerateMipmap: Only GL_TEXTURE_2D is supported.");
        return;
    }
    
    TextureObject* tex = getTexture(m_activeTextureUnit);
    if (tex) {
        tex->generateMipmaps();
    } else {
        LOG_WARN("glGenerateMipmap: No texture bound to active unit.");
    }
}

}