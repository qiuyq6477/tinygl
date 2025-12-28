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

struct TextureObject;

// 定义函数指针类型，用于存储当前生效的采样逻辑
using SamplerFunc = Vec4 (*)(const TextureObject& obj, float u, float v, float rho);

// ==========================================
// 策略模版 (Policy Templates)
// ==========================================

// --- 1. 环绕策略 (Wrap Policies) ---
// 定义不同 Wrap 模式的静态行为
template <GLenum Mode>
struct WrapPolicy {
    static float apply(float coord) { return coord - std::floor(coord); } // Default to Repeat
    static int applyInt(int coord, int size) { return (coord % size + size) % size; }
    static bool checkBorder(float coord) { return false; }
};

template <> struct WrapPolicy<GL_REPEAT> {
    static float apply(float coord) { return coord - std::floor(coord); }
    static int applyInt(int coord, int size) { return (coord % size + size) % size; }
    static bool checkBorder(float coord) { return false; }
};

template <> struct WrapPolicy<GL_MIRRORED_REPEAT> {
    static float apply(float coord) { return std::abs(coord - 2.0f * std::floor(coord / 2.0f + 0.5f)); }
    static int applyInt(int coord, int size) { 
        // 简化处理：MIRRORED_REPEAT 的整数版本比较复杂，这里暂退化为 Clamp 或 Repeat
        // 正确实现需要模拟镜像逻辑
        return std::clamp(coord, 0, size - 1); 
    }
    static bool checkBorder(float coord) { return false; }
};

template <> struct WrapPolicy<GL_CLAMP_TO_EDGE> {
    static float apply(float coord) { return std::clamp(coord, 0.0f, 1.0f); }
    static int applyInt(int coord, int size) { return std::clamp(coord, 0, size - 1); }
    static bool checkBorder(float coord) { return false; }
};

template <> struct WrapPolicy<GL_CLAMP_TO_BORDER> {
    static float apply(float coord) { return coord; }
    static int applyInt(int coord, int size) { return coord; } // 超出边界的逻辑由调用者 checkBorder 决定
    static bool checkBorder(float coord) { return coord < 0.0f || coord > 1.0f; }
};

// --- 2. 过滤策略 (Filter Policies) ---
// MinFilter, MagFilter: 过滤模式
// TWrapS, TWrapT: 环绕策略类
template <GLenum MinFilter, GLenum MagFilter, typename TWrapS, typename TWrapT>
struct FilterPolicy {
    static Vec4 sample(const TextureObject& obj, float u, float v, float rho);
    
private:
    // 辅助：获取单个像素 (Nearest)
    static Vec4 getTexel(const TextureObject& obj, int level, int x, int y);
    // 辅助：双线性插值 (Bilinear)
    static Vec4 sampleBilinear(const TextureObject& obj, int level, float uw, float vw);
};

// Texture Object Definition
struct TextureObject {
    // 主采样入口：直接调用函数指针，无运行时分支
    Vec4 sample(float u, float v, float rho = 0.0f) const {
        return activeSampler(*this, u, v, rho);
    }

    // --- Mipmap Generation (Box Filter) ---
    void generateMipmaps();

    // 更新采样器函数指针
    // 当 glTexParameteri 改变 wrapS, wrapT, minFilter, magFilter 时调用
    void updateSampler();
    
    // --- Data ---
    GLuint id;
    GLsizei width = 0, height = 0;
    
    // Flattened Mipmap Storage
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

    // 当前激活的采样函数指针
    SamplerFunc activeSampler = nullptr;
    
    // 默认构造时初始化一个默认采样器
    TextureObject() {
        // 默认为 Repeat + Nearest/Linear
        updateSampler();
    }
};

// ==========================================
// 模版实现细节 (Template Implementations)
// ==========================================

// 辅助：获取 Texel
inline Vec4 getTexelRaw(const TextureObject& obj, int level, int x, int y) {
    if (level < 0 || level >= static_cast<int>(obj.mipLevels.size())) return {0,0,0,1};
    const auto& info = obj.mipLevels[level];
    // 这里不再做 Clamp，假设调用者已经处理好了 Wrap
    uint32_t p = obj.data[info.offset + y * info.width + x];
    constexpr float k = 1.0f / 255.0f;
    return Vec4((p&0xFF)*k, ((p>>8)&0xFF)*k, ((p>>16)&0xFF)*k, ((p>>24)&0xFF)*k);
}

template <GLenum MinFilter, GLenum MagFilter, typename TWrapS, typename TWrapT>
Vec4 FilterPolicy<MinFilter, MagFilter, TWrapS, TWrapT>::getTexel(const TextureObject& obj, int level, int x, int y) {
    // 处理 Wrap (Integer domain)
    int wx = TWrapS::applyInt(x, obj.mipLevels[level].width);
    int wy = TWrapT::applyInt(y, obj.mipLevels[level].height);
    return getTexelRaw(obj, level, wx, wy);
}

template <GLenum MinFilter, GLenum MagFilter, typename TWrapS, typename TWrapT>
Vec4 FilterPolicy<MinFilter, MagFilter, TWrapS, TWrapT>::sampleBilinear(const TextureObject& obj, int level, float uw, float vw) {
    const auto& info = obj.mipLevels[level];
    float uImg = uw * info.width - 0.5f;
    float vImg = vw * info.height - 0.5f;

    int x0 = (int)std::floor(uImg);
    int y0 = (int)std::floor(vImg);
    float s = uImg - (float)x0;
    float t = vImg - (float)y0;

    // 利用 WrapPolicy 处理坐标
    // 注意：这里需要对 4 个点分别处理 Wrap，不能只处理起始点
    int w = info.width;
    int h = info.height;
    
    int x0w = TWrapS::applyInt(x0, w);
    int x1w = TWrapS::applyInt(x0 + 1, w);
    int y0w = TWrapT::applyInt(y0, h);
    int y1w = TWrapT::applyInt(y0 + 1, h);

    Vec4 c00 = getTexelRaw(obj, level, x0w, y0w);
    Vec4 c10 = getTexelRaw(obj, level, x1w, y0w);
    Vec4 c01 = getTexelRaw(obj, level, x0w, y1w);
    Vec4 c11 = getTexelRaw(obj, level, x1w, y1w);

    return mix(mix(c00, c10, s), mix(c01, c11, s), t);
}

template <GLenum MinFilter, GLenum MagFilter, typename TWrapS, typename TWrapT>
Vec4 FilterPolicy<MinFilter, MagFilter, TWrapS, TWrapT>::sample(const TextureObject& obj, float u, float v, float rho) {
    if (obj.mipLevels.empty()) return {1, 0, 1, 1};

    // 1. 边界检查 (编译期消除)
    if (TWrapS::checkBorder(u) || TWrapT::checkBorder(v)) return obj.borderColor;

    // 2. 坐标 Wrap (编译期内联)
    float uw = TWrapS::apply(u);
    float vw = TWrapT::apply(v);

    float level = 0.0f;
    
    // LOD 计算
    if (rho > 0.0f) {
        float size = (float)std::max(obj.mipLevels[0].width, obj.mipLevels[0].height);
        level = std::log2(rho * size);
    }
    level += obj.lodBias;
    level = std::clamp(level, obj.minLOD, obj.maxLOD);
    
    float maxLevel = (float)static_cast<int>(obj.mipLevels.size()) - 1;

    // --- Magnification (level <= 0) ---
    if (level <= 0.0f) {
        // 编译期 if constexpr
        if constexpr (MagFilter == GL_NEAREST) {
            const auto& info = obj.mipLevels[0];
            return getTexel(obj, 0, (int)(uw * info.width), (int)(vw * info.height));
        } else {
            return sampleBilinear(obj, 0, uw, vw);
        }
    }

    // --- Minification (level > 0) ---
    level = std::clamp(level, 0.0f, maxLevel);

    if constexpr (MinFilter == GL_NEAREST) {
        return getTexel(obj, 0, (int)(uw * obj.mipLevels[0].width), (int)(vw * obj.mipLevels[0].height));
    } 
    else if constexpr (MinFilter == GL_LINEAR) {
        return sampleBilinear(obj, 0, uw, vw);
    }
    else if constexpr (MinFilter == GL_NEAREST_MIPMAP_NEAREST) {
        int lvl = (int)std::round(level);
        const auto& info = obj.mipLevels[lvl];
        return getTexel(obj, lvl, (int)(uw * info.width), (int)(vw * info.height));
    }
    else if constexpr (MinFilter == GL_LINEAR_MIPMAP_NEAREST) {
        return sampleBilinear(obj, (int)std::round(level), uw, vw);
    }
    else if constexpr (MinFilter == GL_NEAREST_MIPMAP_LINEAR) {
        int l0 = (int)std::floor(level);
        int l1 = std::min(l0 + 1, (int)maxLevel);
        float f = level - (float)l0;
        
        const auto& i0 = obj.mipLevels[l0];
        const auto& i1 = obj.mipLevels[l1];
        // 两个层级分别做 Nearest
        Vec4 c0 = getTexel(obj, l0, (int)(uw * i0.width), (int)(vw * i0.height));
        Vec4 c1 = getTexel(obj, l1, (int)(uw * i1.width), (int)(vw * i1.height));
        return mix(c0, c1, f);
    }
    else { // GL_LINEAR_MIPMAP_LINEAR
        int l0 = (int)std::floor(level);
        int l1 = std::min(l0 + 1, (int)maxLevel);
        float f = level - (float)l0;
        return mix(sampleBilinear(obj, l0, uw, vw), sampleBilinear(obj, l1, uw, vw), f);
    }
}

} // namespace tinygl