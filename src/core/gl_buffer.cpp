#include <tinygl/core/tinygl.h>

namespace tinygl {

// --- Buffers ---
void SoftRenderContext::glGenBuffers(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = buffers.allocate();
        LOG_INFO("GenBuffer ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glBindBuffer(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER) m_boundArrayBuffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) getVAO().elementBufferID = buffer;
    else if (target == GL_COPY_READ_BUFFER) m_boundCopyReadBuffer = buffer;
    else if (target == GL_COPY_WRITE_BUFFER) m_boundCopyWriteBuffer = buffer;
}

void SoftRenderContext::glBufferData(GLenum target, GLsizei size, const void* data, GLenum usage) {
    GLuint id = getBufferID(target);
    BufferObject* buffer = buffers.get(id);
    if (!buffer) {
        LOG_ERROR("Invalid Buffer Binding");
        return;
    }
    buffer->data.resize(size);
    if (data) std::memcpy(buffer->data.data(), data, size);
    buffer->usage = usage;
    LOG_INFO("BufferData " + std::to_string(size) + " bytes to ID " + std::to_string(id));
}

void SoftRenderContext::glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data) {
    GLuint id = getBufferID(target);
    BufferObject* bufferPtr = buffers.get(id);
    if (!bufferPtr) {
        LOG_ERROR("glBufferSubData: Invalid Buffer Binding");
        return;
    }
    BufferObject& buffer = *bufferPtr;
    if (offset < 0 || size < 0 || (size_t)(offset + size) > buffer.data.size()) {
        LOG_ERROR("glBufferSubData: Out of bounds");
        return;
    }
    if (data) {
        std::memcpy(buffer.data.data() + offset, data, size);
    }
}

void SoftRenderContext::glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size) {
    GLuint readID = getBufferID(readTarget);
    GLuint writeID = getBufferID(writeTarget);

    BufferObject* readBufPtr = buffers.get(readID);
    BufferObject* writeBufPtr = buffers.get(writeID);

    if (!readBufPtr || !writeBufPtr) {
        LOG_ERROR("glCopyBufferSubData: Invalid Buffer Binding");
        return;
    }

    BufferObject& readBuf = *readBufPtr;
    BufferObject& writeBuf = *writeBufPtr;

    if (readOffset < 0 || size < 0 || (size_t)(readOffset + size) > readBuf.data.size() ||
        writeOffset < 0 || (size_t)(writeOffset + size) > writeBuf.data.size()) {
        LOG_ERROR("glCopyBufferSubData: Out of bounds");
        return;
    }

    // Standard says: If readTarget and writeTarget are the same, the ranges must not overlap.
    if (readID == writeID) {
         if (!( (readOffset + size <= writeOffset) || (writeOffset + size <= readOffset) )) {
             LOG_ERROR("glCopyBufferSubData: Overlapping copy within the same buffer");
             return;
         }
    }

    std::memcpy(writeBuf.data.data() + writeOffset, readBuf.data.data() + readOffset, size);
}

void* SoftRenderContext::glMapBuffer(GLenum target, GLenum access) {
    GLuint id = getBufferID(target);
    BufferObject* bufferPtr = buffers.get(id);
    if (!bufferPtr) {
        LOG_ERROR("glMapBuffer: Invalid Buffer Binding");
        return nullptr;
    }
    BufferObject& buffer = *bufferPtr;
    if (buffer.mapped) {
        LOG_ERROR("glMapBuffer: Buffer already mapped");
        return nullptr;
    }
    buffer.mapped = true;
    buffer.mappedAccess = access;
    return buffer.data.data();
}

GLboolean SoftRenderContext::glUnmapBuffer(GLenum target) {
    GLuint id = getBufferID(target);
    BufferObject* bufferPtr = buffers.get(id);
    if (!bufferPtr) {
        LOG_ERROR("glUnmapBuffer: Invalid Buffer Binding");
        return GL_FALSE;
    }
    BufferObject& buffer = *bufferPtr;
    if (!buffer.mapped) {
        LOG_ERROR("glUnmapBuffer: Buffer not mapped");
        return GL_FALSE;
    }
    buffer.mapped = false;
    return GL_TRUE;
}

// --- VAO ---
void SoftRenderContext::glGenVertexArrays(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = vaos.allocate();
        LOG_INFO("GenVAO ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glDeleteBuffers(GLsizei n, const GLuint* buffers_to_delete) {
    for (GLsizei i = 0; i < n; ++i) {
        GLuint id = buffers_to_delete[i];
        if (id != 0) {
            // Check and reset binding points
            if (m_boundArrayBuffer == id) m_boundArrayBuffer = 0;
            if (m_boundCopyReadBuffer == id) m_boundCopyReadBuffer = 0;
            if (m_boundCopyWriteBuffer == id) m_boundCopyWriteBuffer = 0;

            // Check current VAO's Element Buffer
            VertexArrayObject* vao = vaos.get(m_boundVertexArray);
            if (vao && vao->elementBufferID == id) {
                vao->elementBufferID = 0;
            }

            if (buffers.isActive(id)) {
                buffers.release(id);
                LOG_INFO("Deleted Buffer ID: " + std::to_string(id));
            }
        }
    }
}

void SoftRenderContext::glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    for (GLsizei i = 0; i < n; ++i) {
        GLuint id = arrays[i];
        if (id != 0) {
            // If the deleted VAO is currently bound, unbind it (reset to default VAO 0)
            if (m_boundVertexArray == id) {
                m_boundVertexArray = 0;
            }

            if (vaos.isActive(id)) {
                vaos.release(id);
                LOG_INFO("Deleted VAO ID: " + std::to_string(id));
            }
        }
    }
}

void SoftRenderContext::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* pointer) {
    if (index >= MAX_ATTRIBS) return;
    if (m_boundArrayBuffer == 0) {
        LOG_ERROR("No VBO bound!");
        return;
    }
    VertexAttribState& attr = getVAO().attributes[index];
    attr.bindingBufferID = m_boundArrayBuffer;
    attr.size = size;
    attr.type = type;
    attr.normalized = norm; 
    attr.stride = stride;
    attr.pointerOffset = reinterpret_cast<size_t>(pointer);
    LOG_INFO("Attrib " + std::to_string(index) + " bound to VBO " + std::to_string(m_boundArrayBuffer));
}

void SoftRenderContext::glEnableVertexAttribArray(GLuint index) {
    if (index < MAX_ATTRIBS) getVAO().attributes[index].enabled = true;
}

void SoftRenderContext::glVertexAttribDivisor(GLuint index, GLuint divisor) {
    if (index < MAX_ATTRIBS) {
        getVAO().attributes[index].divisor = divisor;
    }
}

Vec4 SoftRenderContext::fetchAttribute(const VertexAttribState& attr, int vertexIdx, int instanceIdx) {
    if (!attr.enabled) return Vec4(0,0,0,1);

    BufferObject* bufferPtr = buffers.get(attr.bindingBufferID);
    // 检查 1: Buffer 是否存在
    if (!bufferPtr) return Vec4(0,0,0,1);
    
    const auto& buffer = *bufferPtr;
    // 检查 2: Buffer 是否为空
    if (buffer.data.empty()) return Vec4(0,0,0,1);

    // 计算 stride 
    // TODO: 支持其他类型的 Attribute
    size_t stride = attr.stride ? attr.stride : attr.size * sizeof(float); // Default stride for floats
    // Instancing 核心逻辑
    // 如果 divisor 为 0，使用 vertexIdx。
    // 如果 divisor > 0，使用 instanceIdx / divisor。
    int effectiveIdx = (attr.divisor == 0) ? vertexIdx : (instanceIdx / attr.divisor);
    // 计算起始偏移量
    size_t offset = attr.pointerOffset + effectiveIdx * stride;
    
    // 计算读取该属性所需的总大小
    // 注意：attr.type 可能是 float 或 byte，这里假设标准 float 为 4 字节
    // 如果你的系统支持更多类型，这里需要根据 attr.type 动态计算 dataSize
    size_t elementSize = (attr.type == GL_UNSIGNED_BYTE) ? sizeof(uint8_t) : sizeof(float);
    size_t readSize = attr.size * elementSize;

    // 检查 3: 严格的越界检查
    // offset + readSize > buffer.size()
    if (offset >= buffer.data.size() || (buffer.data.size() - offset) < readSize) {
        // 返回默认值，防止崩溃
        return Vec4(0,0,0,1);
    }

    float raw[4] = {0,0,0,1};
    
    // 读取逻辑 (使用 readSafe 进一步保证，或者直接 memcpy 因为上面已检查)
    switch (attr.type) {
        case GL_FLOAT: {
            // for(int i=0; i<attr.size; ++i) {
            //     it->second.readSafe<float>(offset + i*sizeof(float), raw[i]);
            // }
            // 直接拷贝，比循环 readSafe 快
            std::memcpy(raw, buffer.data.data() + offset, attr.size * sizeof(float));
            break;
        }
        case GL_UNSIGNED_BYTE: {
            // stride = attr.stride ? attr.stride : attr.size * sizeof(uint8_t);
            // offset = attr.pointerOffset + vertexIdx * stride;
            // 如果是 BYTE 类型，stride 默认值计算可能需要区分，但上面已经根据类型计算了 stride，
            // 这里的逻辑复用 fetchAttribute 原有逻辑即可，重点是 offset 计算变了。
            uint8_t ubyte_raw[4] = {0,0,0,255};
            // for(int i=0; i<attr.size; ++i) {
            //     it->second.readSafe<uint8_t>(offset + i*sizeof(uint8_t), ubyte_raw[i]);
            // }
            std::memcpy(ubyte_raw, buffer.data.data() + offset, attr.size * sizeof(uint8_t));
            
            if (attr.normalized) {
                for(int i=0; i<4; ++i) raw[i] = ubyte_raw[i] / 255.0f;
            } else {
                for(int i=0; i<4; ++i) raw[i] = (float)ubyte_raw[i];
            }
            break;
        }
        default:
            LOG_WARN("Unsupported vertex attribute type.");
            break;
    }
    
    return Vec4(raw[0], raw[1], raw[2], raw[3]);
}

}