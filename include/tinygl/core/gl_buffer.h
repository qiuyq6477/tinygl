#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include "tinygl/base/log.h"
#include "gl_defs.h"

namespace tinygl {

// Buffer
struct BufferObject {
    std::vector<uint8_t> data;
    GLenum usage = GL_STATIC_DRAW; // Default usage
    
    template <typename T>
    bool readSafe(size_t offset, T& outValue) const {
        if (offset + sizeof(T) > data.size()) {
            static bool logged = false;
            if (!logged) {
                LOG_ERROR("Buffer Read Overflow! Offset: " + std::to_string(offset) + ", Size: " + std::to_string(data.size()));
                logged = true;
            }
            return false;
        }
        std::memcpy(&outValue, data.data() + offset, sizeof(T));
        return true;
    }
};

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

}