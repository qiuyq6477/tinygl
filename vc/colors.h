#pragma once
#include <arm_neon.h>

namespace ColorUtilsSIMD {

    // 预加载常量
    const float32x4_t v255 = vdupq_n_f32(255.0f);
    const float32x4_t vInv255 = vdupq_n_f32(1.0f / 255.0f);
    const float32x4_t vHalf = vdupq_n_f32(0.5f); // 用于四舍五入

    // ---------------------------------------------------------
    // 1. 单个像素转换 (Vec4 -> uint32_t)
    // ---------------------------------------------------------
    inline uint32_t FloatToUint32(const Vec4& v) {
        // 1. 加载 (x, y, z, w) 到寄存器
        float32x4_t val = vld1q_f32(&v.x); 
        
        // 2. 钳制到 [0.0, 1.0]
        // NEON min/max 指令
        val = vminq_f32(vmaxq_f32(val, vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));

        // 3. 乘 255 并加 0.5 (四舍五入)
        // val = val * 255.0 + 0.5
        val = vmlaq_f32(vHalf, val, v255);

        // 4. 转为整数 (Convert to U32)
        uint32x4_t ival = vcvtq_u32_f32(val);

        // 5. 提取并打包 (这是 SIMD 处理单像素的瓶颈，无法避免)
        // 利用 vgetq_lane 提取分量
        // 假设 Vec4 内存顺序是 RGBA (x=r, y=g, z=b, w=a)
        // 我们需要输出 0xAABBGGRR
        uint32_t r = vgetq_lane_u32(ival, 0);
        uint32_t g = vgetq_lane_u32(ival, 1);
        uint32_t b = vgetq_lane_u32(ival, 2);
        uint32_t a = vgetq_lane_u32(ival, 3);

        return (a << 24) | (b << 16) | (g << 8) | r;
    }

    // ---------------------------------------------------------
    // 2. 批量转换 (4个像素) - 这是 SIMD 真正快的地方
    // 输入: 4个 Vec4
    // 输出: 4个 uint32_t 存储到数组
    // ---------------------------------------------------------
    inline void FloatToUint32_Batch4(const Vec4* pixelsIn, uint32_t* colorsOut) {
        // 我们无法一次加载4个Vec4变成 Struct of Arrays (SoA)，
        // 这里的内存是 Array of Structs (AoS): xyzw xyzw xyzw xyzw
        // 使用 vld4q_f32 可以直接加载并解以此交错数据
        // rrrr, gggg, bbbb, aaaa
        
        float32x4x4_t pixelBlock = vld4q_f32(&pixelsIn[0].x);

        // R通道处理
        pixelBlock.val[0] = vminq_f32(vmaxq_f32(pixelBlock.val[0], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[0] = vmlaq_f32(vHalf, pixelBlock.val[0], v255);
        uint32x4_t rInt = vcvtq_u32_f32(pixelBlock.val[0]);

        // G通道处理
        pixelBlock.val[1] = vminq_f32(vmaxq_f32(pixelBlock.val[1], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[1] = vmlaq_f32(vHalf, pixelBlock.val[1], v255);
        uint32x4_t gInt = vcvtq_u32_f32(pixelBlock.val[1]);

        // B通道处理
        pixelBlock.val[2] = vminq_f32(vmaxq_f32(pixelBlock.val[2], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[2] = vmlaq_f32(vHalf, pixelBlock.val[2], v255);
        uint32x4_t bInt = vcvtq_u32_f32(pixelBlock.val[2]);

        // A通道处理
        pixelBlock.val[3] = vminq_f32(vmaxq_f32(pixelBlock.val[3], vdupq_n_f32(0.0f)), vdupq_n_f32(1.0f));
        pixelBlock.val[3] = vmlaq_f32(vHalf, pixelBlock.val[3], v255);
        uint32x4_t aInt = vcvtq_u32_f32(pixelBlock.val[3]);

        // 打包 (Packing)
        // 目标是: 0xAABBGGRR
        // R (0-255)
        // G (0-255) << 8
        // B (0-255) << 16
        // A (0-255) << 24
        
        uint32x4_t packed = rInt;
        packed = vorrq_u32(packed, vshlq_n_u32(gInt, 8));
        packed = vorrq_u32(packed, vshlq_n_u32(bInt, 16));
        packed = vorrq_u32(packed, vshlq_n_u32(aInt, 24));

        // 存储结果
        vst1q_u32(colorsOut, packed);
    }
}