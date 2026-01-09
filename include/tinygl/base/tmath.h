#pragma once

#include <cmath>
#include <cstdint>
#include <algorithm>

namespace tinygl {

// --- 基础常量 ---
const float PI = 3.14159265359f;

// --- 基础标量函数 ---
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


struct Vec2 { float x, y; };
struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    
    Vec4 operator+(const Vec4& v) const { return {x+v.x, y+v.y, z+v.z, w+v.w}; }
    Vec4 operator-(const Vec4& v) const { return {x-v.x, y-v.y, z-v.z, w-v.w}; }
    Vec4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
    Vec4 operator*(const Vec4& v) const { return {x*v.x, y*v.y, z*v.z, w*v.w}; }
};

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

// --- 插值函数 (Mix, Step, Smoothstep) ---

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

// --- 几何函数 (Geometric) ---
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

// --- 构造函数辅助 (类似 GLSL vec3(vec4)) ---

// 丢弃 w 分量，只取 xyz
inline Vec4 vec3(const Vec4& v) {
    return Vec4(v.x, v.y, v.z, 0.0f);
}

// 构造颜色
inline Vec4 vec4(float r, float g, float b, float a) {
    return Vec4(r, g, b, a);
}

struct Mat4 {
    float m[16];

    // 初始化为单位矩阵
    static Mat4 Identity() {
        Mat4 res = {0};
        res.m[0] = res.m[5] = res.m[10] = res.m[15] = 1.0f;
        return res;
    }

    // 平移矩阵
    static Mat4 Translate(float x, float y, float z) {
        Mat4 res = Identity();
        res.m[12] = x; res.m[13] = y; res.m[14] = z; // Column-major layout: translation is in last column
        return res;
    }

    // 缩放矩阵
    static Mat4 Scale(float x, float y, float z) {
        Mat4 res = Identity();
        res.m[0] = x; res.m[5] = y; res.m[10] = z;
        return res;
    }

    // 透视投影矩阵 (简易版 FOV=90, Aspect=1)
    // 使得 Z 坐标产生透视效果 (w = -z)
    static Mat4 Perspective(float fovInDegrees, float aspect, float zNear, float zFar) {
        Mat4 res = {0};
        float fovInRadians = fovInDegrees * M_PI / 180.0f;
        float f = 1.0f / std::tan(fovInRadians * 0.5f);
        res.m[0] = f / aspect;
        res.m[5] = f;
        res.m[10] = (zFar + zNear) / (zNear - zFar);
        res.m[11] = -1.0f; // Convention: Camera looks towards -Z
        res.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        return res;
    }

    // LookAt 矩阵
    static Mat4 LookAt(const Vec4& eye, const Vec4& center, const Vec4& up) {
        Vec4 f = normalize(center - eye);
        Vec4 s = normalize(cross(f, up));
        Vec4 u = cross(s, f);

        Mat4 res = Identity();
        res.m[0] = s.x;
        res.m[4] = s.y;
        res.m[8] = s.z;
        res.m[1] = u.x;
        res.m[5] = u.y;
        res.m[9] = u.z;
        res.m[2] = -f.x;
        res.m[6] = -f.y;
        res.m[10] = -f.z;
        res.m[12] = - (s.x * eye.x + s.y * eye.y + s.z * eye.z);
        res.m[13] = - (u.x * eye.x + u.y * eye.y + u.z * eye.z);
        res.m[14] =   (f.x * eye.x + f.y * eye.y + f.z * eye.z);
        return res;
    }

    // 旋转矩阵 (绕 X 轴)
    static Mat4 RotateX(float angleInDegrees) {
        Mat4 res = Identity();
        float rad = angleInDegrees * M_PI / 180.0f;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[5] = c;  res.m[6] = s;
        res.m[9] = -s; res.m[10] = c;
        return res;
    }

    // 旋转矩阵 (绕 Y 轴)
    static Mat4 RotateY(float angleInDegrees) {
        Mat4 res = Identity();
        float rad = angleInDegrees * M_PI / 180.0f;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[0] = c;   res.m[2] = -s;
        res.m[8] = s;   res.m[10] = c;
        return res;
    }

    // 旋转矩阵 (绕 Z 轴)
    static Mat4 RotateZ(float angleInDegrees) {
        Mat4 res = Identity();
        float rad = angleInDegrees * M_PI / 180.0f;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[0] = c;  res.m[1] = s;
        res.m[4] = -s; res.m[5] = c;
        return res;
    }

    // 矩阵转置
    static Mat4 Transpose(const Mat4& mat) {
        Mat4 res;
        for(int i = 0; i < 4; ++i) {
            for(int j = 0; j < 4; ++j) {
                res.m[j * 4 + i] = mat.m[i * 4 + j];
            }
        }
        return res;
    }



    // 矩阵乘法 (A * B)
    Mat4 operator*(const Mat4& r) const {
        Mat4 res = {0};
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    // Column-major access: m[col * 4 + row]
                    // But our struct is flat array, let's assume standard OpenGL Column-Major order:
                    // Index = col * 4 + row
                    sum += m[k*4 + row] * r.m[col*4 + k];
                }
                res.m[col*4 + row] = sum;
            }
        }
        return res;
    }

    // 矩阵 * 向量
    Vec4 operator*(const Vec4& v) const {
        // x = m0*vx + m4*vy + m8*vz + m12*vw
        return Vec4(
            m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        );
    }
};

}