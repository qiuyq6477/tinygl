#pragma once

#if defined(__ARM_NEON) || defined(__aarch64__)
    #include <arm_neon.h>
#elif defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    #include <immintrin.h>
#else
    #warning "No SIMD support detected. Performance will be degraded."
#endif

#include "tmath.h"

namespace tinygl {

// ==========================================
// SIMD 包装类 (NEON / SSE)
// ==========================================

// 定义强制内联宏
#if defined(__GNUC__) || defined(__clang__)
    #define SIMD_INLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
    #define SIMD_INLINE __forceinline
#else
    #define SIMD_INLINE inline
#endif

#if defined(__ARM_NEON) || defined(__aarch64__)

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

#elif defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)

struct alignas(16) Simd4f {
    __m128 v;

    // 默认构造
    SIMD_INLINE Simd4f() : v(_mm_setzero_ps()) {}
    
    // 广播构造
    SIMD_INLINE explicit Simd4f(float val) : v(_mm_set1_ps(val)) {}
    
    // 从 __m128 构造
    SIMD_INLINE Simd4f(__m128 _v) : v(_v) {}

    // 加载
    SIMD_INLINE static Simd4f load(const float* ptr) {
        return Simd4f(_mm_loadu_ps(ptr));
    }
    
    // 存储
    SIMD_INLINE void store(float* ptr) const {
        _mm_storeu_ps(ptr, v);
    }

    // 基础运算
    SIMD_INLINE Simd4f operator+(const Simd4f& other) const { return Simd4f(_mm_add_ps(v, other.v)); }
    SIMD_INLINE Simd4f operator-(const Simd4f& other) const { return Simd4f(_mm_sub_ps(v, other.v)); }
    SIMD_INLINE Simd4f operator*(const Simd4f& other) const { return Simd4f(_mm_mul_ps(v, other.v)); }
    
    // 乘加 fused multiply-add: res = this + a * b
    SIMD_INLINE Simd4f madd(const Simd4f& a, const Simd4f& b) const {
        // x86 FMA check or fallback
        #if defined(__FMA__)
            return Simd4f(_mm_fmadd_ps(a.v, b.v, v));
        #else
            return Simd4f(_mm_add_ps(v, _mm_mul_ps(a.v, b.v)));
        #endif
    }
    // 辅助：从 Vec4 加载
    SIMD_INLINE static Simd4f load(const Vec4& v) {
        return Simd4f(_mm_loadu_ps(&v.x));
    }

    // 辅助：存储到 Vec4
    SIMD_INLINE void store(Vec4& v) const {
        _mm_storeu_ps(&v.x, this->v);
    }
};

#else

// 标量回退 (如果需要的话，或者直接报错)
// 这里为了简单，如果不是 NEON 也不是 SSE，可能无法编译通过，除非实现标量版本。
// 鉴于 tinygl 定位高性能软渲染，暂时要求 SIMD 支持。
struct alignas(16) Simd4f {
    float v[4];
    // ... Implement scalar fallback if needed ...
};

#endif

// 辅助：SIMD 矩阵列式存储，用于加速 Vertex Shader
// 这里的逻辑对所有平台通用，只要 Simd4f 接口一致
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

}