#pragma once
#include <tinygl/base/tmath.h>

namespace tests {

// 渲染过程中的全局状态（通常由 Frame/Camera/Scene 决定）
struct RenderState {
    Mat4 view;
    Mat4 projection;
    Vec4 viewPos;
    
    // 简单的灯光数据用于演示
    Vec4 lightPos;
    Vec4 lightColor;
};

}
