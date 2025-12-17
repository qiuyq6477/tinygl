#pragma once
#include "gl_defs.h"

// 辅助：计算平面梯度的结构
struct Gradients {
    float dfdx, dfdy;
};

struct Viewport {
    GLint x, y;
    GLsizei w, h;
};