#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <cmath>
#include <algorithm>
#include "gl_defs.h"
#include "../tmath.h"

// Shader & Program
struct ShaderContext { 
    Vec4 varyings[MAX_VARYINGS]; 
    float lod = 0.0f; // [新增]
    // [新增] 构造函数强制清零，防止 NaN 传染
    ShaderContext() {
        std::memset(varyings, 0, sizeof(varyings));
    }
};

// VOut: 顶点着色器的输出，也是裁剪阶段的输入
struct VOut { 
    Vec4 pos;       // Clip Space Position (未除以 w)
    Vec4 scn;       // Screen Space Position (已除以 w)
    ShaderContext ctx; 
};

struct UniformValue {
    enum Type { INT, FLOAT, MAT4 } type;
    union { int i; float f; float mat[16]; } data;
};


struct ShaderBase {

    // --- 0. 基础常量 ---
    const float PI = 3.14159265359f;

    // --- 1. 基础标量函数 ---
    inline float fract(float x) { return x - std::floor(x); }
    inline float radians(float deg) { return deg * 0.01745329251f; }
    inline float degrees(float rad) { return rad * 57.2957795131f; }
    inline float sign(float x) { return (x > 0.0f) ? 1.0f : ((x < 0.0f) ? -1.0f : 0.0f); }
    inline float mod(float x, float y) { return x - y * std::floor(x / y); }
    inline float step(float edge, float x) { return x < edge ? 0.0f : 1.0f; }
    
    inline float smoothstep(float edge0, float edge1, float x) {
        float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    inline float mix(float x, float y, float a) {
        return x * (1.0f - a) + y * a;
    }

    inline float length(float x) { return std::abs(x); }
    inline float distance(float p0, float p1) { return std::abs(p0 - p1); }

    // --- 2. Vec4 扩展运算符 (如果你的 Vec4 结构体没重载这些) ---
    // 假设 Vec4 已经重载了 +, -, *, / (与标量和向量)
    // 这里补充一些 Component-wise 的操作

    // --- 3. 向量版通用数学函数 (Component-wise) ---
    
    // fract
    inline Vec4 fract(const Vec4& v) {
        return Vec4(fract(v.x), fract(v.y), fract(v.z), fract(v.w));
    }

    // abs
    inline Vec4 abs(const Vec4& v) {
        return Vec4(std::abs(v.x), std::abs(v.y), std::abs(v.z), std::abs(v.w));
    }

    // min
    inline Vec4 min(const Vec4& a, const Vec4& b) {
        return Vec4(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));
    }
    inline Vec4 min(const Vec4& a, float b) {
        return Vec4(std::min(a.x, b), std::min(a.y, b), std::min(a.z, b), std::min(a.w, b));
    }

    // max
    inline Vec4 max(const Vec4& a, const Vec4& b) {
        return Vec4(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w));
    }
    inline Vec4 max(const Vec4& a, float b) {
        return Vec4(std::max(a.x, b), std::max(a.y, b), std::max(a.z, b), std::max(a.w, b));
    }

    // clamp
    inline Vec4 clamp(const Vec4& v, float minVal, float maxVal) {
        return Vec4(
            std::clamp(v.x, minVal, maxVal),
            std::clamp(v.y, minVal, maxVal),
            std::clamp(v.z, minVal, maxVal),
            std::clamp(v.w, minVal, maxVal)
        );
    }

    // --- 4. 插值函数 (Mix, Step, Smoothstep) ---

    // mix (Vector, Vector, float) -> 最常用
    inline Vec4 mix(const Vec4& x, const Vec4& y, float a) {
        return x * (1.0f - a) + y * a;
    }
    
    // mix (Vector, Vector, Vector) -> 逐分量混合
    inline Vec4 mix(const Vec4& x, const Vec4& y, const Vec4& a) {
        return Vec4(
            mix(x.x, y.x, a.x),
            mix(x.y, y.y, a.y),
            mix(x.z, y.z, a.z),
            mix(x.w, y.w, a.w)
        );
    }

    // smoothstep (Vector)
    inline Vec4 smoothstep(float edge0, float edge1, const Vec4& x) {
        return Vec4(
            smoothstep(edge0, edge1, x.x),
            smoothstep(edge0, edge1, x.y),
            smoothstep(edge0, edge1, x.z),
            smoothstep(edge0, edge1, x.w)
        );
    }

    // --- 5. 几何函数 (Geometric) ---
    // 注意：Vec4 通常被视作 Vec3 运算 (忽略 w 或 w=0)，或者纯 4D 运算
    // 这里针对光照计算，实现 Vec3 语义的函数

    // Dot Product (点乘) - 3D 语义 (忽略 w)
    inline float dot(const Vec4& a, const Vec4& b) {
        return a.x * b.x + a.y * b.y + a.z * b.z; 
        // 如果需要 4D 点乘，请另写 dot4
    }

    // Cross Product (叉乘) - 仅 3D 有效
    inline Vec4 cross(const Vec4& a, const Vec4& b) {
        return Vec4(
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x,
            0.0f // 向量 w=0
        );
    }

    // Length (模长)
    inline float length(const Vec4& v) {
        return std::sqrt(dot(v, v));
    }

    // Distance (距离)
    inline float distance(const Vec4& a, const Vec4& b) {
        return length(a - b);
    }

    // Normalize (归一化)
    inline Vec4 normalize(const Vec4& v) {
        float len = length(v);
        if (len < 1e-6f) return Vec4(0,0,0,0);
        float invLen = 1.0f / len;
        return v * invLen;
    }

    // Reflect (反射)
    // I: 入射向量 (Incident), N: 法线 (Normal, 必须已归一化)
    inline Vec4 reflect(const Vec4& I, const Vec4& N) {
        return I - N * 2.0f * dot(N, I);
    }
    
    // --- 6. 构造函数辅助 (类似 GLSL vec3(vec4)) ---
    
    // 丢弃 w 分量，只取 xyz
    inline Vec4 vec3(const Vec4& v) {
        return Vec4(v.x, v.y, v.z, 0.0f);
    }
    
    // 构造颜色
    inline Vec4 vec4(float r, float g, float b, float a) {
        return Vec4(r, g, b, a);
    }
};