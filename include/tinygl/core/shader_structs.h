#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <functional>
#include <cstring>
#include "gl_defs.h"

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

struct ProgramObject {
    GLuint id;
    std::unordered_map<std::string, GLint> uniformLocs;
    std::unordered_map<GLint, UniformValue> uniforms;
    std::function<Vec4(const std::vector<Vec4>&, ShaderContext&)> vertexShader;
    std::function<Vec4(const ShaderContext&)> fragmentShader;
};
