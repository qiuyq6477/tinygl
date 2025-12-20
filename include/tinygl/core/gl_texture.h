#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream>
#include "../tmath.h"
#include "../log.h"
#include "gl_defs.h"

namespace tinygl {

// Texture
struct TextureObject {
    // 主采样入口
    Vec4 sample(float u, float v, float lod = 0.0f) const {
        if (mipLevels.empty()) return {1, 0, 1, 1};

        lod = std::clamp(lod + lodBias, minLOD, maxLOD);
        float maxLevel = (float)static_cast<int>(mipLevels.size()) - 1;
        
        // 放大过滤 (LOD <= 0)
        if (lod <= 0.0f) {
            if (magFilter == GL_NEAREST) {
                const auto& info = mipLevels[0];
                return getTexel(0, (int)(applyWrap(u, wrapS) * info.width), (int)(applyWrap(v, wrapT) * info.height));
            }
            return sampleBilinear(u, v, 0);
        }
        
        // 缩小过滤 (LOD > 0)
        lod = std::clamp(lod, 0.0f, maxLevel);

        if (minFilter == GL_NEAREST) return getTexel(0, (int)(applyWrap(u, wrapS) * mipLevels[0].width), (int)(applyWrap(v, wrapT) * mipLevels[0].height));
        if (minFilter == GL_LINEAR) return sampleBilinear(u, v, 0);
        
        if (minFilter == GL_NEAREST_MIPMAP_NEAREST) {
            int level = (int)std::round(lod);
            const auto& info = mipLevels[level];
            return getTexel(level, (int)(applyWrap(u, wrapS) * info.width), (int)(applyWrap(v, wrapT) * info.height));
        }
        if (minFilter == GL_LINEAR_MIPMAP_NEAREST) {
            return sampleBilinear(u, v, (int)std::round(lod));
        }

        // 三线性过滤 (Default: GL_LINEAR_MIPMAP_LINEAR)
        int l0 = (int)std::floor(lod);
        int l1 = std::min(l0 + 1, (int)maxLevel);
        float f = lod - (float)l0;
        return mix(sampleBilinear(u, v, l0), sampleBilinear(u, v, l1), f);
    }

    // --- Mipmap Generation (Box Filter) ---
    void generateMipmaps() {
        if (mipLevels.empty()) return;
        
        // 从 Level 0 开始迭代
        int currentLevel = 0;
        
        while (true) {
            // Fix: Use index access and copy values to avoid reference invalidation
            // because mipLevels.push_back() below might reallocate the vector.
            size_t srcOffset = mipLevels[currentLevel].offset;
            int srcW = mipLevels[currentLevel].width;
            int srcH = mipLevels[currentLevel].height;

            if (srcW <= 1 && srcH <= 1) break;

            int nextW = std::max(1, srcW / 2);
            int nextH = std::max(1, srcH / 2);
            size_t nextSize = nextW * nextH;
            
            size_t newOffset = data.size();
            data.resize(newOffset + nextSize);
            
            // 记录新层级元数据 (Might invalidate vector pointers/references)
            mipLevels.push_back({newOffset, nextW, nextH});

            // 获取源和目标指针（resize 后重新获取，防止迭代器失效）
            const uint32_t* srcPtr = data.data() + srcOffset; // Use copied offset
            uint32_t* dstPtr = data.data() + newOffset;

            for (int y = 0; y < nextH; ++y) {
                for (int x = 0; x < nextW; ++x) {
                    int srcX = x * 2;
                    int srcY = y * 2;
                    
                    int x0 = srcX;
                    int x1 = std::min(srcX + 1, srcW - 1);
                    int y0 = srcY;
                    int y1 = std::min(srcY + 1, srcH - 1);

                    uint32_t c00 = srcPtr[y0 * srcW + x0];
                    uint32_t c10 = srcPtr[y0 * srcW + x1];
                    uint32_t c01 = srcPtr[y1 * srcW + x0];
                    uint32_t c11 = srcPtr[y1 * srcW + x1];

                    auto unpack = [](uint32_t c, int shift) { return (c >> shift) & 0xFF; };
                    auto avg = [&](int shift) {
                        return (unpack(c00, shift) + unpack(c10, shift) + unpack(c01, shift) + unpack(c11, shift)) / 4;
                    };

                    uint32_t R = avg(0);
                    uint32_t G = avg(8);
                    uint32_t B = avg(16);
                    uint32_t A = avg(24);
                    dstPtr[y * nextW + x] = (A << 24) | (B << 16) | (G << 8) | R;
                }
            }
            currentLevel++;
        }
        LOG_INFO("Generated " + std::to_string(mipLevels.size()) + " mipmap levels.");
    }

    Vec4 sampleBilinear(float u, float v, int level) const {
        if (checkBorder(u, wrapS) || checkBorder(v, wrapT)) return borderColor;
        if (level >= static_cast<int>(mipLevels.size())) return {0,0,0,1};

        const auto& info = mipLevels[level];
        float uImg = applyWrap(u, wrapS) * info.width - 0.5f;
        float vImg = applyWrap(v, wrapT) * info.height - 0.5f;

        int x0 = (int)std::floor(uImg);
        int y0 = (int)std::floor(vImg);
        float s = uImg - (float)x0;
        float t = vImg - (float)y0;

        auto wrapIdx = [](int idx, int max, GLenum mode) {
             if (mode == GL_REPEAT) return (idx % max + max) % max;
             return std::clamp(idx, 0, max - 1);
        };
        
        int ix0 = wrapIdx(x0, info.width, wrapS);
        int ix1 = wrapIdx(x0 + 1, info.width, wrapS);
        int iy0 = wrapIdx(y0, info.height, wrapT);
        int iy1 = wrapIdx(y0 + 1, info.height, wrapT);

        Vec4 c00 = getTexel(level, ix0, iy0);
        Vec4 c10 = getTexel(level, ix1, iy0);
        Vec4 c01 = getTexel(level, ix0, iy1);
        Vec4 c11 = getTexel(level, ix1, iy1);

        return mix(mix(c00, c10, s), mix(c01, c11, s), t);
    }

    Vec4 sampleNearest(float u, float v) const {
        if (mipLevels.empty()) return {1,0,1,1};
        const auto& info = mipLevels[0];
        // 快速位运算仅适用于 POT 且 REPEAT
        uint32_t x = ((int)(u * info.width)) & (info.width - 1);
        uint32_t y = ((int)(v * info.height)) & (info.height - 1);
        uint32_t p = data[info.offset + y * info.width + x];
        constexpr float k = 1.0f / 255.0f;
        return Vec4((p&0xFF)*k, ((p>>8)&0xFF)*k, ((p>>16)&0xFF)*k, ((p>>24)&0xFF)*k);
    }
    
    bool checkBorder(float uv, GLenum wrapMode) const {
        if (wrapMode == GL_CLAMP_TO_BORDER) {
            if (uv < 0.0f || uv > 1.0f) return true;
        }
        return false;
    }

    float applyWrap(float val, GLenum mode) const {
        switch (mode) {
            case GL_REPEAT: 
                return val - std::floor(val);
            case GL_MIRRORED_REPEAT: 
                return std::abs(val - 2.0f * std::floor(val / 2.0f + 0.5f));
            case GL_CLAMP_TO_EDGE: 
                return std::clamp(val, 0.0f, 1.0f); 
            case GL_CLAMP_TO_BORDER:
                return std::clamp(val, 0.0f, 1.0f);
            default: return val - std::floor(val);
        }
    }

    // --- Sampling Logic ---
    Vec4 getTexel(int level, int x, int y) const {
        if (level < 0 || level >= static_cast<int>(mipLevels.size())) return {0,0,0,1};
        const auto& info = mipLevels[level];
        x = std::clamp(x, 0, info.width - 1);
        y = std::clamp(y, 0, info.height - 1);
        uint32_t p = data[info.offset + y * info.width + x];
        constexpr float k = 1.0f / 255.0f;
        return Vec4((p&0xFF)*k, ((p>>8)&0xFF)*k, ((p>>16)&0xFF)*k, ((p>>24)&0xFF)*k);
    }

    GLuint id;
    GLsizei width = 0, height = 0;
    
    // Flattened Mipmap Storage: 所有层级数据连续存放
    std::vector<uint32_t> data; 

    struct MipLevelInfo {
        size_t offset;
        int width, height;
    };
    std::vector<MipLevelInfo> mipLevels;

    // 纹理参数状态
    GLenum wrapS = GL_REPEAT;
    GLenum wrapT = GL_REPEAT;
    GLenum minFilter = GL_NEAREST_MIPMAP_LINEAR; 
    GLenum magFilter = GL_LINEAR;

    // 边框颜色
    Vec4 borderColor = {0.0f, 0.0f, 0.0f, 0.0f};

    float minLOD = -1000.0f;
    float maxLOD = 1000.0f;
    float lodBias = 0.0f;
};

} // namespace tinygl
