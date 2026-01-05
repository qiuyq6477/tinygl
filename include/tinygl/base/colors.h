#pragma once

#if defined(__ARM_NEON) || defined(__aarch64__)
    #include <arm_neon.h>
#elif defined(__SSE4_1__) || defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    #include <immintrin.h>
#endif

#include "tmath.h"
#include <algorithm>

namespace ColorUtilsSIMD {

#if defined(__ARM_NEON) || defined(__aarch64__)

    // 预加载常量
    const float32x4_t v255 = vdupq_n_f32(255.0f);
    const float32x4_t vInv255 = vdupq_n_f32(1.0f / 255.0f);
    const float32x4_t vHalf = vdupq_n_f32(0.5f); // 用于四舍五入

    // ---------------------------------------------------------
    // 1. 单个像素转换 (Vec4 -> uint32_t)
    // ---------------------------------------------------------
    inline uint32_t FloatToUint32(const tinygl::Vec4& v) {
        // 1. 加载 (x, y, z, w) 到寄存器
        float32x4_t val = vld1q_f32(&v.x); 
        
        // 2. 钳制到 [0.0, 1.0]
        val = vminq_f32(vmaxq_f32(val, vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));

        // 3. 乘 255 并加 0.5 (四舍五入)
        val = vmlaq_f32(vHalf, val, v255);

        // 4. 转为整数 (Convert to U32)
        uint32x4_t ival = vcvtq_u32_f32(val);

        // 5. 提取并打包
        uint32_t r = vgetq_lane_u32(ival, 0);
        uint32_t g = vgetq_lane_u32(ival, 1);
        uint32_t b = vgetq_lane_u32(ival, 2);
        uint32_t a = vgetq_lane_u32(ival, 3);

        return (a << 24) | (b << 16) | (g << 8) | r;
    }

    // ---------------------------------------------------------
    // 2. 批量转换 (4个像素)
    // ---------------------------------------------------------
    inline void FloatToUint32_Batch4(const tinygl::Vec4* pixelsIn, uint32_t* colorsOut) {
        float32x4x4_t pixelBlock = vld4q_f32(&pixelsIn[0].x);

        // R
        pixelBlock.val[0] = vminq_f32(vmaxq_f32(pixelBlock.val[0], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[0] = vmlaq_f32(vHalf, pixelBlock.val[0], v255);
        uint32x4_t rInt = vcvtq_u32_f32(pixelBlock.val[0]);

        // G
        pixelBlock.val[1] = vminq_f32(vmaxq_f32(pixelBlock.val[1], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[1] = vmlaq_f32(vHalf, pixelBlock.val[1], v255);
        uint32x4_t gInt = vcvtq_u32_f32(pixelBlock.val[1]);

        // B
        pixelBlock.val[2] = vminq_f32(vmaxq_f32(pixelBlock.val[2], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[2] = vmlaq_f32(vHalf, pixelBlock.val[2], v255);
        uint32x4_t bInt = vcvtq_u32_f32(pixelBlock.val[2]);

        // A
        pixelBlock.val[3] = vminq_f32(vmaxq_f32(pixelBlock.val[3], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[3] = vmlaq_f32(vHalf, pixelBlock.val[3], v255);
        uint32x4_t aInt = vcvtq_u32_f32(pixelBlock.val[3]);

        // Pack
        uint32x4_t packed = rInt;
        packed = vorrq_u32(packed, vshlq_n_u32(gInt, 8));
        packed = vorrq_u32(packed, vshlq_n_u32(bInt, 16));
        packed = vorrq_u32(packed, vshlq_n_u32(aInt, 24));

        vst1q_u32(colorsOut, packed);
    }

#elif defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)

    // ---------------------------------------------------------
    // SSE 实现
    // ---------------------------------------------------------
    
    // 辅助: 处理单向量 (Clamp [0,1] -> *255 -> +0.5 -> Int)
    inline __m128i ProcessPixelVector(__m128 v) {
        // Clamp 0.0, 1.0
        v = _mm_max_ps(v, _mm_setzero_ps());
        v = _mm_min_ps(v, _mm_set1_ps(1.0f));
        
        // Scale * 255.0 + 0.5
        v = _mm_add_ps(_mm_mul_ps(v, _mm_set1_ps(255.0f)), _mm_set1_ps(0.5f));
        
        // Convert to Int32
        return _mm_cvtps_epi32(v);
    }

    inline uint32_t FloatToUint32(const tinygl::Vec4& v) {
        __m128 val = _mm_loadu_ps(&v.x);
        __m128i ival = ProcessPixelVector(val);
        
        // 此时 ival 包含 R, G, B, A (32-bit integers)
        // 我们需要 pack 成 8-bit integers.
        // SSE4.1 有 _mm_packus_epi32.
        // 如果只有 SSE2，需要 tricky bit ops.
        // 假设 SSE4.1 可用 (2025年了)，或提供 fallback.
        
        #if defined(__SSE4_1__) || defined(__AVX__)
            // Pack 32->16 (0, 1, 2, 3, 0, 1, 2, 3) - 只需要低 4 个
            __m128i p16 = _mm_packus_epi32(ival, ival); 
            // Pack 16->8
            __m128i p8 = _mm_packus_epi16(p16, p16);
            // 此时低32位包含 A B G R (顺序取决于机器字节序，通常 Little Endian: Byte0=R)
            return (uint32_t)_mm_cvtsi128_si32(p8);
        #else
            // SSE2 Fallback (手动提取)
            // 比较慢，但在单像素函数中可以接受
            alignas(16) int data[4];
            _mm_store_si128((__m128i*)data, ival);
            uint32_t r = std::clamp(data[0], 0, 255);
            uint32_t g = std::clamp(data[1], 0, 255);
            uint32_t b = std::clamp(data[2], 0, 255);
            uint32_t a = std::clamp(data[3], 0, 255);
            return (a << 24) | (b << 16) | (g << 8) | r;
        #endif
    }

    inline void FloatToUint32_Batch4(const tinygl::Vec4* pixelsIn, uint32_t* colorsOut) {
        // 加载 4 个像素
        __m128 v0 = _mm_loadu_ps(&pixelsIn[0].x);
        __m128 v1 = _mm_loadu_ps(&pixelsIn[1].x);
        __m128 v2 = _mm_loadu_ps(&pixelsIn[2].x);
        __m128 v3 = _mm_loadu_ps(&pixelsIn[3].x);

        // 处理每个像素 (转为 32位整数)
        __m128i i0 = ProcessPixelVector(v0);
        __m128i i1 = ProcessPixelVector(v1);
        __m128i i2 = ProcessPixelVector(v2);
        __m128i i3 = ProcessPixelVector(v3);

        #if defined(__SSE4_1__) || defined(__AVX__)
            // 利用 Pack 指令高效压缩
            // Pack 32-bit integers to 16-bit integers (unsigned saturation)
            // p01: R0 G0 B0 A0 R1 G1 B1 A1 (8 x 16-bit)
            __m128i p01 = _mm_packus_epi32(i0, i1);
            // p23: R2 G2 B2 A2 R3 G3 B3 A3 (8 x 16-bit)
            __m128i p23 = _mm_packus_epi32(i2, i3);

            // Pack 16-bit integers to 8-bit integers (unsigned saturation)
            // res: R0 G0 B0 A0 R1 G1 B1 A1 R2 G2 B2 A2 R3 G3 B3 A3 (16 x 8-bit)
            __m128i res = _mm_packus_epi16(p01, p23);

            // 存储结果 (4个 uint32)
            _mm_storeu_si128((__m128i*)colorsOut, res);
        #else
            // SSE2 Fallback
             alignas(16) int d0[4]; _mm_store_si128((__m128i*)d0, i0);
             alignas(16) int d1[4]; _mm_store_si128((__m128i*)d1, i1);
             alignas(16) int d2[4]; _mm_store_si128((__m128i*)d2, i2);
             alignas(16) int d3[4]; _mm_store_si128((__m128i*)d3, i3);
             
             auto pack = [](int* d) -> uint32_t {
                 return (std::clamp(d[3],0,255)<<24) | (std::clamp(d[2],0,255)<<16) | (std::clamp(d[1],0,255)<<8) | std::clamp(d[0],0,255);
             };
             
             colorsOut[0] = pack(d0);
             colorsOut[1] = pack(d1);
             colorsOut[2] = pack(d2);
             colorsOut[3] = pack(d3);
        #endif
    }

#else

    // 标量 Fallback
    inline uint32_t FloatToUint32(const tinygl::Vec4& v) {
        uint32_t r = (uint32_t)(std::clamp(v.x, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint32_t g = (uint32_t)(std::clamp(v.y, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint32_t b = (uint32_t)(std::clamp(v.z, 0.0f, 1.0f) * 255.0f + 0.5f);
        uint32_t a = (uint32_t)(std::clamp(v.w, 0.0f, 1.0f) * 255.0f + 0.5f);
        return (a << 24) | (b << 16) | (g << 8) | r;
    }

    inline void FloatToUint32_Batch4(const tinygl::Vec4* pixelsIn, uint32_t* colorsOut) {
        for(int i=0; i<4; ++i) {
            colorsOut[i] = FloatToUint32(pixelsIn[i]);
        }
    }

#endif

}