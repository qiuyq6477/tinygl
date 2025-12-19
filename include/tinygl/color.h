#pragma once
#include <cstdint>
#include <algorithm> // for std::clamp
#include <cmath>     // for std::round or type casting

using namespace tinygl;
namespace ColorUtils {

    // 内部常量：颜色位移定义 (AABBGGRR 格式, Little Endian)
    static constexpr int SHIFT_R = 0;
    static constexpr int SHIFT_G = 8;
    static constexpr int SHIFT_B = 16;
    static constexpr int SHIFT_A = 24;

    // ==========================================
    // 1. 归一化 Float (0.0-1.0) <-> Uint32
    // ==========================================

    // [0.0, 1.0] -> 0xAABBGGRR
    inline uint32_t FloatToUint32(const Vec4& v) {
        // 使用 255.0f + 0.5f 实现四舍五入，比直接截断更平滑
        uint32_t r = (uint32_t)(std::clamp(v.x, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint32_t g = (uint32_t)(std::clamp(v.y, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint32_t b = (uint32_t)(std::clamp(v.z, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint32_t a = (uint32_t)(std::clamp(v.w, 0.0f, 1.0f) * 255.0f + 0.5f);

        return (a << SHIFT_A) | (b << SHIFT_B) | (g << SHIFT_G) | (r << SHIFT_R);
    }

    // 0xAABBGGRR -> [0.0, 1.0]
    inline Vec4 Uint32ToFloat(uint32_t c) {
        constexpr float inv255 = 1.0f / 255.0f;
        return Vec4{
            ((c >> SHIFT_R) & 0xFF) * inv255,
            ((c >> SHIFT_G) & 0xFF) * inv255,
            ((c >> SHIFT_B) & 0xFF) * inv255,
            ((c >> SHIFT_A) & 0xFF) * inv255
        };
    }

    // ==========================================
    // 2. 字节范围 Float (0.0-255.0) <-> Uint32
    // ==========================================

    // [0.0, 255.0] -> 0xAABBGGRR
    inline uint32_t ByteFloatToUint32(const Vec4& v) {
        uint32_t r = (uint32_t)(std::clamp(v.x, 0.0f, 255.0f) + 0.5f);
        uint32_t g = (uint32_t)(std::clamp(v.y, 0.0f, 255.0f) + 0.5f);
        uint32_t b = (uint32_t)(std::clamp(v.z, 0.0f, 255.0f) + 0.5f);
        uint32_t a = (uint32_t)(std::clamp(v.w, 0.0f, 255.0f) + 0.5f);

        return (a << SHIFT_A) | (b << SHIFT_B) | (g << SHIFT_G) | (r << SHIFT_R);
    }

    // 0xAABBGGRR -> [0.0, 255.0]
    inline Vec4 Uint32ToByteFloat(uint32_t c) {
        return Vec4{
            (float)((c >> SHIFT_R) & 0xFF),
            (float)((c >> SHIFT_G) & 0xFF),
            (float)((c >> SHIFT_B) & 0xFF),
            (float)((c >> SHIFT_A) & 0xFF)
        };
    }

    // ==========================================
    // 3. Float (0-1) <-> Float (0-255)
    // ==========================================

    inline Vec4 Normalize(const Vec4& v255) {
        constexpr float inv255 = 1.0f / 255.0f;
        return Vec4{ v255.x * inv255, v255.y * inv255, v255.z * inv255, v255.w * inv255 };
    }

    inline Vec4 Denormalize(const Vec4& v1) {
        return Vec4{ v1.x * 255.0f, v1.y * 255.0f, v1.z * 255.0f, v1.w * 255.0f };
    }
}