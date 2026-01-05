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

// ShaderBuiltins: Base class for shaders to emulate GLSL built-in variables
struct ShaderBuiltins {
    // --- Vertex Shader Outputs ---
    Vec4 gl_Position;
    float gl_PointSize = 1.0f;

    // --- Fragment Shader Inputs ---
    Vec4 gl_FragCoord;   // (x, y, z, 1/w) in screen space
    bool gl_FrontFacing; // true if front facing

    // --- Fragment Shader Outputs ---
    Vec4 gl_FragColor;
    bool gl_Discard = false;

    // Helper to trigger discard
    void discard() { gl_Discard = true; }
};

// Shader & Program
struct ShaderContext { 
    Vec4 varyings[MAX_VARYINGS]; 
    float rho = 0.0f; // 纹理导数模长 ( UV单位 / 屏幕像素 ) (UV Span per Screen Pixel)
    
    // Per-Fragment Operations control
    bool discarded = false;
    bool fragDepthWritten = false;
    float fragDepth = 0.0f;

    ShaderContext() {
        std::memset(varyings, 0, sizeof(varyings));
    }

    void discard() { discarded = true; }
    void setDepth(float z) { fragDepth = z; fragDepthWritten = true; }
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