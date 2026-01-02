#pragma once
#include <vector>
#include <string>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <memory>
#include "../base/tmath.h"
#include "gl_defs.h"
#include "gl_texture.h"

namespace tinygl {

// Shader & Program
struct ShaderContext { 
    Vec4 varyings[MAX_VARYINGS]; 
    float rho = 0.0f; // 纹理导数模长 ( UV单位 / 屏幕像素 ) (UV Span per Screen Pixel)
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

}