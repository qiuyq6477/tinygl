#pragma once

#include <arm_neon.h>
#include "tmath.h"
// ==========================================
// macOS ARM NEON SIMD 包装类
// ==========================================
// 简单的跨平台预留，目前专为 macOS/ARM 优化

// 定义强制内联宏
#if defined(__GNUC__) || defined(__clang__)
    #define SIMD_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define SIMD_INLINE __forceinline
#else
    #define SIMD_INLINE inline
#endif
struct alignas(16) Simd4f {
    float32x4_t v;

    // 默认构造
    SIMD_INLINE Simd4f() : v(vdupq_n_f32(0.0f)) {}
    
    // 广播构造
    SIMD_INLINE explicit Simd4f(float val) : v(vdupq_n_f32(val)) {}
    
    // 从 float32x4_t 构造
    SIMD_INLINE Simd4f(float32x4_t _v) : v(_v) {}

    // 加载
    SIMD_INLINE static Simd4f load(const float* ptr) {
        return Simd4f(vld1q_f32(ptr));
    }
    
    // 存储
    SIMD_INLINE void store(float* ptr) const {
        vst1q_f32(ptr, v);
    }

    // 基础运算
    SIMD_INLINE Simd4f operator+(const Simd4f& other) const { return Simd4f(vaddq_f32(v, other.v)); }
    SIMD_INLINE Simd4f operator-(const Simd4f& other) const { return Simd4f(vsubq_f32(v, other.v)); }
    SIMD_INLINE Simd4f operator*(const Simd4f& other) const { return Simd4f(vmulq_f32(v, other.v)); }
    
    // 乘加 fused multiply-add: res = this + a * b
    SIMD_INLINE Simd4f madd(const Simd4f& a, const Simd4f& b) const {
        return Simd4f(vfmaq_f32(v, a.v, b.v)); 
    }
    // 辅助：从 Vec4 加载
    SIMD_INLINE static Simd4f load(const Vec4& v) {
        // 假设 Vec4 布局是 x,y,z,w 连续 float
        return Simd4f(vld1q_f32(&v.x));
    }

    // 辅助：存储到 Vec4
    SIMD_INLINE void store(Vec4& v) const {
        vst1q_f32(&v.x, this->v);
    }
};

// 辅助：SIMD 矩阵列式存储，用于加速 Vertex Shader
struct SimdMat4 {
    Simd4f cols[4]; // 矩阵的 4 列

    void load(const Mat4& m) {
        // Mat4 是行主序还是列主序？假设 m.m 是平铺数组
        // 这里我们需要每一列加载到一个寄存器
        // m[0], m[1], m[2], m[3] -> col0 (如果是列主序)
        // 你的 Mat4 实现看起来是标准的，我们按列加载
        cols[0] = Simd4f::load(&m.m[0]);
        cols[1] = Simd4f::load(&m.m[4]);
        cols[2] = Simd4f::load(&m.m[8]);
        cols[3] = Simd4f::load(&m.m[12]);
    }

    // 变换顶点 (假设 w=1.0)
    inline Simd4f transformPoint(const Simd4f& p) const {
        // 获取 x, y, z 分量并广播
        // NEON 取 lane 比较繁琐，这里用下标访问模拟广播，编译器会优化为 dup
        // 更好的方式是用 vdupq_laneq_f32，但为了简单：
        float buf[4]; p.store(buf);
        
        Simd4f x(buf[0]);
        Simd4f y(buf[1]);
        Simd4f z(buf[2]);
        
        // res = col0 * x + col1 * y + col2 * z + col3
        // 使用 FMA 优化: col3 + col0*x + col1*y + col2*z
        Simd4f res = cols[3];
        res = res.madd(cols[0], x);
        res = res.madd(cols[1], y);
        res = res.madd(cols[2], z);
        return res;
    }
};
