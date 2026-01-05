#pragma once
#include <cstdint>

#if defined(_WIN32)
    #ifdef TINYGL_EXPORTS
        #define TINYGL_API __declspec(dllexport)
    #else
        #define TINYGL_API __declspec(dllimport)
    #endif
#else
    #define TINYGL_API __attribute__((visibility("default")))
#endif

namespace tinygl {

// Buffer Type for glClear
using GLbitfield = uint32_t;
const GLbitfield GL_COLOR_BUFFER_BIT = 0x00004000;
const GLbitfield GL_DEPTH_BUFFER_BIT = 0x00000100;
const GLbitfield GL_STENCIL_BUFFER_BIT = 0x00000400;

// ==========================================
// 资源定义
// ==========================================
using GLenum = uint32_t;
using GLuint = uint32_t;
using GLint = int32_t;
using GLsizei = int32_t;
using GLintptr = intptr_t;
using GLsizeiptr = intptr_t;
using GLboolean = bool;
using GLfloat = float;

const GLenum GL_ARRAY_BUFFER = 0x8892;
const GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
const GLenum GL_STATIC_DRAW = 0x88E4;
const GLenum GL_READ_ONLY = 0x88B8;
const GLenum GL_WRITE_ONLY = 0x88B9;
const GLenum GL_READ_WRITE = 0x88BA;
const GLenum GL_FLOAT = 0x1406;
const GLenum GL_TEXTURE_2D = 0x0DE1;
const GLenum GL_RGBA = 0x1908;
const GLenum GL_RGB = 0x1907;
const GLenum GL_UNSIGNED_BYTE = 0x1401;
const GLenum GL_UNSIGNED_SHORT = 0x1403;
const GLenum GL_UNSIGNED_INT = 0x1405;

const GLboolean GL_TRUE = 1;
const GLboolean GL_FALSE = 0;

const GLenum GL_DEPTH_TEST = 0x0B71;
const GLenum GL_STENCIL_TEST = 0x0B90;
const GLenum GL_CULL_FACE = 0x0B44;

const GLenum GL_NEVER = 0x0200;
const GLenum GL_LESS = 0x0201;
const GLenum GL_EQUAL = 0x0202;
const GLenum GL_LEQUAL = 0x0203;
const GLenum GL_GREATER = 0x0204;
const GLenum GL_NOTEQUAL = 0x0205;
const GLenum GL_GEQUAL = 0x0206;
const GLenum GL_ALWAYS = 0x0207;

const GLenum GL_KEEP = 0x1E00;
const GLenum GL_REPLACE = 0x1E01;
const GLenum GL_INCR = 0x1E02;
const GLenum GL_DECR = 0x1E03;
const GLenum GL_INVERT = 0x150A;
const GLenum GL_INCR_WRAP = 0x8507;
const GLenum GL_DECR_WRAP = 0x8508;
const GLenum GL_ZERO = 0x0;

const GLenum GL_POINTS = 0x0000;
const GLenum GL_LINES = 0x0001;
const GLenum GL_LINE_LOOP = 0x0002;
const GLenum GL_LINE_STRIP = 0x0003;
const GLenum GL_TRIANGLES = 0x0004;
const GLenum GL_TRIANGLE_STRIP = 0x0005;
const GLenum GL_TRIANGLE_FAN = 0x0006;

const GLenum GL_FRONT = 0x0404;
const GLenum GL_BACK = 0x0405;
const GLenum GL_FRONT_AND_BACK = 0x0408;
const GLenum GL_CW = 0x0900;
const GLenum GL_CCW = 0x0901;

const GLenum GL_FILL = 0x1B02;
const GLenum GL_LINE = 0x1B01;
const GLenum GL_POINT = 0x1B00;

// 纹理单元常量
const GLenum GL_TEXTURE0 = 0x84C0;
const GLenum GL_TEXTURE1 = 0x84C1;
const GLenum GL_TEXTURE2 = 0x84C2;
const GLenum GL_TEXTURE3 = 0x84C3;
const GLenum GL_TEXTURE4 = 0x84C4;
const GLenum GL_TEXTURE5 = 0x84C5;
const GLenum GL_TEXTURE6 = 0x84C6;
const GLenum GL_TEXTURE7 = 0x84C7;
const GLenum GL_TEXTURE8 = 0x84C8;
const GLenum GL_TEXTURE9 = 0x84C9;
const GLenum GL_TEXTURE10 = 0x84CA;
const GLenum GL_TEXTURE11 = 0x84CB;
const GLenum GL_TEXTURE12 = 0x84CC;
const GLenum GL_TEXTURE13 = 0x84CD;
const GLenum GL_TEXTURE14 = 0x84CE;
const GLenum GL_TEXTURE15 = 0x84CF;

// 纹理参数枚举
const GLenum GL_TEXTURE_WRAP_S = 0x2802;
const GLenum GL_TEXTURE_WRAP_T = 0x2803;
const GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
const GLenum GL_TEXTURE_MAG_FILTER = 0x2800;

// Wrap Modes
const GLenum GL_REPEAT = 0x2901;
const GLenum GL_CLAMP_TO_EDGE = 0x812F;
const GLenum GL_CLAMP_TO_BORDER = 0x812D;
const GLenum GL_MIRRORED_REPEAT = 0x8370;

// Filter Modes
const GLenum GL_NEAREST = 0x2600;
const GLenum GL_LINEAR = 0x2601;
const GLenum GL_NEAREST_MIPMAP_NEAREST = 0x2700;
const GLenum GL_LINEAR_MIPMAP_NEAREST = 0x2701;
const GLenum GL_NEAREST_MIPMAP_LINEAR = 0x2702;
const GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;

const GLenum GL_TEXTURE_BORDER_COLOR = 0x1004;
const GLenum GL_TEXTURE_MIN_LOD = 0x813A;
const GLenum GL_TEXTURE_MAX_LOD = 0x813B;
const GLenum GL_TEXTURE_LOD_BIAS = 0x8501; // 可选支持

// System Limits & Defaults
constexpr int MAX_ATTRIBS       = 16;   // 最大顶点属性数量
constexpr int MAX_VARYINGS      = 8;    // 最大插值变量数量
constexpr int MAX_TEXTURE_UNITS = 16;   // 最大纹理单元数量

// Math & Colors
constexpr float EPSILON         = 1e-5f;
constexpr float DEPTH_INFINITY  = 1e9f;
constexpr uint32_t COLOR_BLACK  = 0xFF000000; // AABBGGRR (Alpha=255, RGB=0)
constexpr uint32_t COLOR_WHITE  = 0xFFFFFFFF;
constexpr uint32_t COLOR_GREY   = 0xFFAAAAAA;

}