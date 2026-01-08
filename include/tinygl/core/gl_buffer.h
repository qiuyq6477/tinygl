#pragma once
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>
#include <cstdint>
#include "../base/log.h"
#include "gl_defs.h"

namespace tinygl {

// Buffer
struct BufferObject {
    std::vector<uint8_t> data;
    GLenum usage = GL_STATIC_DRAW; // Default usage
    bool mapped = false;           // Track if buffer is mapped
    GLenum mappedAccess = 0;       // Mapping access
    
    // [Phase 1] DSA Support
    bool immutable = false;        // Track if buffer was created with glNamedBufferStorage
    GLsizeiptr size = 0;           // Allocated size
    GLbitfield storageFlags = 0;   // Flags from glNamedBufferStorage

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

// VAO Components (DSA Style)

// 1. 属性格式描述 (Format) - 解耦后的逻辑状态
struct VertexAttribFormat {
    bool enabled = false;
    GLint size = 4;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    GLuint relativeOffset = 0;
    GLuint bindingIndex = 0; // 指向哪个 Binding Slot
};

// 2. 缓冲绑定描述 (Binding Slot)
struct VertexBufferBinding {
    GLuint bufferID = 0;
    GLintptr offset = 0;
    GLsizei stride = 0;
    GLuint divisor = 0; // 支持 Instancing
};

// 3. 执行快照：Baking 的结果 (The Performance Engine)
struct ResolvedAttribute {
    const uint8_t* basePointer = nullptr; // 最终计算出的起始内存地址
    GLsizei stride = 0;
    GLint size = 4;
    GLenum type = GL_FLOAT;
    GLboolean normalized = GL_FALSE;
    bool enabled = false;
    GLuint divisor = 0; // 支持 Instancing
};

struct VertexArrayObject {
    // 逻辑状态 (Logical State)
    VertexAttribFormat attributes[MAX_ATTRIBS];
    VertexBufferBinding bindings[MAX_BINDINGS];
    GLuint elementBufferID = 0;

    // 状态检查 (Dirty Flag)
    bool isDirty = true; // 任何 Format 或 Binding 改变时设为 true
    
    // 烘焙出的结果：fetchAttribute 直接读取这个数组 (Baked State)
    ResolvedAttribute bakedAttributes[MAX_ATTRIBS];
};

// Keep old name for compatibility if needed elsewhere temporarily, 
// but most code should move to the new structures.
using VertexAttribState = VertexAttribFormat; 

}