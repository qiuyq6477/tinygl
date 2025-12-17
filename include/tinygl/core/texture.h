#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include <string>
#include <iostream> // for log
#include "../tmath.h"
#include "../log.h"
#include "gl_defs.h"

// Texture
struct TextureObject {
    GLuint id;
    GLsizei width = 0, height = 0;
    
    // Mipmaps 存储：Level 0 是原始图像，Level 1 是 1/2 大小，以此类推
    std::vector<std::vector<uint32_t>> levels; 

    // 纹理参数状态
    GLenum wrapS = GL_REPEAT;
    GLenum wrapT = GL_REPEAT;
    GLenum minFilter = GL_NEAREST_MIPMAP_LINEAR; // 默认三线性
    GLenum magFilter = GL_LINEAR;

    // 边框颜色 (归一化浮点 RGBA)
    Vec4 borderColor = {0.0f, 0.0f, 0.0f, 0.0f}; // 默认透明黑

    float minLOD = -1000.0f;
    float maxLOD = 1000.0f;
    float lodBias = 0.0f;

    // 生成 Mipmaps (Box Filter 下采样)
    void generateMipmaps() {
        if (levels.empty()) return;
        
        // 确保 Level 0 存在
        int w = width;
        int h = height;
        int currentLevel = 0;

        while (w > 1 || h > 1) {
            const auto& src = levels[currentLevel];
            int nextW = std::max(1, w / 2);
            int nextH = std::max(1, h / 2);
            std::vector<uint32_t> dst(nextW * nextH);

            for (int y = 0; y < nextH; ++y) {
                for (int x = 0; x < nextW; ++x) {
                    // 简单的 2x2 均值采样
                    int srcX = x * 2;
                    int srcY = y * 2;
                    // 边界检查省略，因为总是缩小
                    uint32_t c00 = src[srcY * w + srcX];
                    uint32_t c10 = src[srcY * w + std::min(srcX + 1, w - 1)];
                    uint32_t c01 = src[std::min(srcY + 1, h - 1) * w + srcX];
                    uint32_t c11 = src[std::min(srcY + 1, h - 1) * w + std::min(srcX + 1, w - 1)];

                    auto unpack = [](uint32_t c, int shift) { return (c >> shift) & 0xFF; };
                    auto avg = [&](int shift) {
                        return (unpack(c00, shift) + unpack(c10, shift) + unpack(c01, shift) + unpack(c11, shift)) / 4;
                    };

                    uint8_t R = avg(0);
                    uint8_t G = avg(8);
                    uint8_t B = avg(16);
                    uint8_t A = avg(24);
                    dst[y * nextW + x] = (A << 24) | (B << 16) | (G << 8) | R;
                }
            }

            levels.push_back(std::move(dst));
            w = nextW;
            h = nextH;
            currentLevel++;
        }
        LOG_INFO("Generated " + std::to_string(levels.size()) + " mipmap levels.");
    }

    // 判断是否需要使用边框颜色
    // 返回 true 表示坐标越界且模式为 CLAMP_TO_BORDER
    bool checkBorder(float uv, GLenum wrapMode) const {
        if (wrapMode == GL_CLAMP_TO_BORDER) {
            if (uv < 0.0f || uv > 1.0f) return true;
        }
        return false;
    }

    // 处理 Wrap Mode
    // 对于 CLAMP_TO_BORDER，我们在 sample 层级处理越界，
    // 这里只需将其钳制在 0-1 之间以便计算像素索引（防止 crash）
    float applyWrap(float val, GLenum mode) const {
        switch (mode) {
            case GL_REPEAT: 
                return val - std::floor(val);
            case GL_MIRRORED_REPEAT: 
                return std::abs(val - 2.0f * std::floor(val / 2.0f)) - 1.0f;
            case GL_CLAMP_TO_EDGE: 
                return std::clamp(val, 0.0f, 0.9999f); 
            case GL_CLAMP_TO_BORDER:
                // 如果进入到这一步，说明已经决定要采样了（虽然逻辑上应该先返回 BorderColor）
                // 这里 clamp 住防止数组越界
                return std::clamp(val, 0.0f, 0.9999f);
            default: return val - std::floor(val);
        }
    }

    // 获取单个像素 (Nearest)
    Vec4 getTexel(int level, int x, int y) const {
        // 安全检查
        if(level < 0 || level >= static_cast<GLint>(levels.size())) return {0,0,0,1};
        int w = std::max(1, width >> level);
        int h = std::max(1, height >> level);
        
        // 此处不再处理 Wrap，假设传入的 x, y 已经在 [0, w-1] 范围内
        // 但为了安全起见：
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        
        uint32_t p = levels[level][y * w + x];
        return Vec4((p&0xFF)/255.0f, ((p>>8)&0xFF)/255.0f, ((p>>16)&0xFF)/255.0f, ((p>>24)&0xFF)/255.0f);
    }

    // 双线性插值采样 (Bilinear Interpolation)
    Vec4 sampleBilinear(float u, float v, int level) const {
        // 边框检测
        if (checkBorder(u, wrapS) || checkBorder(v, wrapT)) {
            return borderColor;
        }
        if(level >= static_cast<GLint>(levels.size())) return {0,0,0,1};
        int w = std::max(1, width >> level);
        int h = std::max(1, height >> level);

        // 应用 Wrap
        float uImg = applyWrap(u, wrapS) * w - 0.5f;
        float vImg = applyWrap(v, wrapT) * h - 0.5f;

        int x0 = (int)std::floor(uImg);
        int y0 = (int)std::floor(vImg);
        int x1 = x0 + 1;
        int y1 = y0 + 1;
        
        // 权重
        float s = uImg - x0;
        float t = vImg - y0;

        // 注意：getTexel 内部有 clamp，所以 wrap 逻辑最好在坐标转整数时处理
        // 对于 REPEAT 模式，如果 x1 越界，应该回到 0。
        auto wrapIdx = [](int idx, int max, GLenum mode) {
             if (mode == GL_REPEAT) return (idx % max + max) % max;
             return std::clamp(idx, 0, max - 1);
        };
        
        x0 = wrapIdx(x0, w, wrapS); x1 = wrapIdx(x1, w, wrapS);
        y0 = wrapIdx(y0, h, wrapT); y1 = wrapIdx(y1, h, wrapT);

        Vec4 c00 = getTexel(level, x0, y0);
        Vec4 c10 = getTexel(level, x1, y0);
        Vec4 c01 = getTexel(level, x0, y1);
        Vec4 c11 = getTexel(level, x1, y1);

        auto lerp = [](const Vec4& a, const Vec4& b, float f) { return a * (1.0f - f) + b * f; };
        return lerp(lerp(c00, c10, s), lerp(c01, c11, s), t);
    }

    // 在 TextureObject 结构体中
    // 假设我们知道 demo_cube 的纹理是 256x256，且 Wrap 模式是 REPEAT
    // 快速整数采样 (不处理 LOD，仅 Base Level)
    Vec4 sampleFast(float u, float v) const {
        // 假设 width 和 height 都是 2 的幂次 (例如 256)
        // 并且 wrap 模式是 GL_REPEAT
        
        // 1. 归一化坐标转纹理空间坐标 (定点数优化思路)
        // 比如 256.0f * u
        float fu = u * width;
        float fv = v * height;
        
        // 2. 转换为整数坐标
        int iu = (int)fu;
        int iv = (int)fv;
        
        // 3. 使用位运算处理 Wrap (GL_REPEAT)
        // 256 - 1 = 255 (0xFF)
        // 这一步代替了耗时的 val - floor(val)
        int x = iu & (width - 1); 
        int y = iv & (height - 1);
        
        // 4. 获取像素 (Nearest Neighbor)
        // 这里的 levels[0] 访问比 sampleBilinear 快得多
        uint32_t p = levels[0][y * width + x];
        
        // 5. 解包颜色 (0-255 -> 0.0-1.0)
        // 这一步也可以通过预计算查找表进一步加速，但目前这样够用了
        const float k = 1.0f / 255.0f;
        return Vec4((p&0xFF)*k, ((p>>8)&0xFF)*k, ((p>>16)&0xFF)*k, 1.0f);
    }
    // [通用版] 支持任意尺寸图片的最近邻采样
    Vec4 sampleNearest(float u, float v) const {
        // 1. 防止空纹理崩溃
        if (levels.empty() || levels[0].empty()) return Vec4(1.0f, 0.0f, 1.0f, 1.0f);; // 紫色作为错误色
        
        int w = width;
        int h = height;
        
        // 2. 转换为整数坐标
        // 注意：这里没有使用 width-1 这种位掩码
        int iu = (int)std::floor(u * w);
        int iv = (int)std::floor(v * h);

        // 3. 通用取模处理 Wrap (GL_REPEAT)
        // 逻辑：(idx % size + size) % size 可以正确处理负数坐标
        int x = iu % w;
        if (x < 0) x += w;
        
        int y = iv % h;
        if (y < 0) y += h;

        // 3. 获取原始像素
        uint32_t pixel = levels[0][y * w + x];

        // 4. 解包并归一化
        constexpr float k = 1.0f / 255.0f;
        return Vec4(
            (float)(pixel & 0xFF) * k,         // R
            (float)((pixel >> 8) & 0xFF) * k,  // G
            (float)((pixel >> 16) & 0xFF) * k, // B
            (float)((pixel >> 24) & 0xFF) * k  // A
        );
    }
    
    // 前提：纹理尺寸必须是 2 的幂 (256, 512...) 且 Wrap 模式为 REPEAT
    Vec4 sampleNearestFast(float u, float v) const {
        // 1. 利用位运算取模 (替代 slow floor)
        // 假设 width 和 height 是 256 (掩码为 255)
        uint32_t maskX = width - 1;
        uint32_t maskY = height - 1;

        // 2. 快速浮点转整数 (截断)
        // 注意：这里假设 UV 已经是正数。如果 UV 为负，位运算掩码依然有效(对于2的补码)
        int iu = (int)(u * width);
        int iv = (int)(v * height);

        // 3. 位运算处理 Wrap
        int x = iu & maskX;
        int y = iv & maskY;

        // 3. 获取原始像素 (0xAABBGGRR)
        uint32_t pixel = levels[0][y * width + x];
        
        // 4. 解包并归一化
        constexpr float k = 1.0f / 255.0f;
        return Vec4(
            (float)(pixel & 0xFF) * k,         // R
            (float)((pixel >> 8) & 0xFF) * k,  // G
            (float)((pixel >> 16) & 0xFF) * k, // B
            (float)((pixel >> 24) & 0xFF) * k  // A
        );
    }
    
    // 主采样函数 (根据 LOD 选择 Filter)
    Vec4 sample(float u, float v, float lod = 0.0f) const {
        if (levels.empty()) return {1, 0, 1, 1};

        // 边框检测 (Nearest 情况)
        // 如果是 Nearest 模式且未进入 Mipmap 逻辑，也需要检测
        if (magFilter == GL_NEAREST && lod <= 0.0f) {
            if (checkBorder(u, wrapS) || checkBorder(v, wrapT)) return borderColor;
        }


        // 应用 LOD Bias 和 Clamping
        lod += lodBias;
        lod = std::clamp(lod, minLOD, maxLOD);

        // 1. Magnification (放大): LOD < 0 (纹理像素比屏幕像素大)
        if (lod <= 0.0f && magFilter == GL_NEAREST) {
            int w = width; int h = height;
            int x = (int)(applyWrap(u, wrapS) * w);
            int y = (int)(applyWrap(v, wrapT) * h);
            return getTexel(0, x, y); // Nearest Base Level
        }
        
        // 2. Minification (缩小): LOD > 0
        // 限制 LOD 范围
        float maxLevel = (float)static_cast<GLint>(levels.size()) - 1;
        lod = std::clamp(lod, 0.0f, maxLevel);

        // 根据 minFilter 选择策略
        if (minFilter == GL_NEAREST) {
            return sampleBilinear(u, v, 0); // 忽略 Mipmap，仅 Base Level (实际标准是 Nearest on Base)
        }
        if (minFilter == GL_LINEAR) {
             return sampleBilinear(u, v, 0); // Base Level Bilinear
        }
        if (minFilter == GL_NEAREST_MIPMAP_NEAREST) {
            int level = (int)std::round(lod);

            // Nearest Mipmap Nearest 也需要 Border Check
            if (checkBorder(u, wrapS) || checkBorder(v, wrapT)) return borderColor;

            int w = std::max(1, width >> level);
            int h = std::max(1, height >> level);
            int x = (int)(applyWrap(u, wrapS) * w);
            int y = (int)(applyWrap(v, wrapT) * h);
            return getTexel(level, x, y);
        }
        if (minFilter == GL_LINEAR_MIPMAP_NEAREST) {
            int level = (int)std::round(lod);
            return sampleBilinear(u, v, level);
        }
        if (minFilter == GL_NEAREST_MIPMAP_LINEAR) {
            // 在两个 Mipmap 层级间插值，但层级内使用 Nearest
            // (省略实现，较少用)
            return sampleBilinear(u, v, (int)lod); 
        }

        // Default: GL_LINEAR_MIPMAP_LINEAR (Trilinear)
        int levelBase = (int)lod;
        int levelNext = std::min(levelBase + 1, (int)maxLevel);
        float f = lod - levelBase;

        Vec4 cBase = sampleBilinear(u, v, levelBase);
        Vec4 cNext = sampleBilinear(u, v, levelNext);
        return cBase * (1.0f - f) + cNext * f;
    }
};