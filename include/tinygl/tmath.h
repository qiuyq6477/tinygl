#pragma once

#include <cmath>
#include <cstdint>

namespace tinygl {

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
    static Mat4 Perspective(float fovInDegrees, float aspect, float near, float far) {
        Mat4 res = {0};
        float fovInRadians = fovInDegrees * M_PI / 180.0f;
        float f = 1.0f / std::tan(fovInRadians * 0.5f);
        res.m[0] = f / aspect;
        res.m[5] = f;
        res.m[10] = (far + near) / (near - far);
        res.m[11] = -1.0f; // Convention: Camera looks towards -Z
        res.m[14] = (2.0f * far * near) / (near - far);
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