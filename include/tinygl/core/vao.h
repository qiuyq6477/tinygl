#pragma once
#include "gl_defs.h"

// VAO Components
struct VertexAttribState {
    bool enabled = false;
    GLuint bindingBufferID = 0;
    GLint size = 3;
    GLenum type = GL_FLOAT;
    GLboolean normalized = false;
    GLsizei stride = 0;
    size_t pointerOffset = 0;
    // [新增] 实例除数
    // 0 = 每顶点更新 (默认)
    // 1 = 每 1 个实例更新一次
    // N = 每 N 个实例更新一次
    GLuint divisor = 0; 
};

struct VertexArrayObject {
    VertexAttribState attributes[MAX_ATTRIBS];
    GLuint elementBufferID = 0;
};