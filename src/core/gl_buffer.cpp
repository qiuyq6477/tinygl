#include <tinygl/core/tinygl.h>

namespace tinygl {

// --- Buffers ---
void SoftRenderContext::glGenBuffers(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = m_nextID++;
        buffers[res[i]]; 
        LOG_INFO("GenBuffer ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glBindBuffer(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER) m_boundArrayBuffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) getVAO().elementBufferID = buffer;
}

void SoftRenderContext::glBufferData(GLenum target, GLsizei size, const void* data, GLenum usage) {
    GLuint id = (target == GL_ARRAY_BUFFER) ? m_boundArrayBuffer : getVAO().elementBufferID;
    if (id == 0 || buffers.find(id) == buffers.end()) {
        LOG_ERROR("Invalid Buffer Binding");
        return;
    }
    buffers[id].data.resize(size);
    if (data) std::memcpy(buffers[id].data.data(), data, size);
    buffers[id].usage = usage;
    LOG_INFO("BufferData " + std::to_string(size) + " bytes to ID " + std::to_string(id));
}

// --- VAO ---
void SoftRenderContext::glGenVertexArrays(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = m_nextID++;
        vaos[res[i]];
        LOG_INFO("GenVAO ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glDeleteBuffers(GLsizei n, const GLuint* buffers_to_delete) {
    for (GLsizei i = 0; i < n; ++i) {
        GLuint id = buffers_to_delete[i];
        if (id != 0) {
            auto it = buffers.find(id);
            if (it != buffers.end()) {
                buffers.erase(it);
                LOG_INFO("Deleted Buffer ID: " + std::to_string(id));
            }
        }
    }
}

void SoftRenderContext::glDeleteVertexArrays(GLsizei n, const GLuint* arrays) {
    for (GLsizei i = 0; i < n; ++i) {
        GLuint id = arrays[i];
        if (id != 0) {
            auto it = vaos.find(id);
            if (it != vaos.end()) {
                vaos.erase(it);
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

    auto it = buffers.find(attr.bindingBufferID);
    // 检查 1: Buffer 是否存在
    if (it == buffers.end()) return Vec4(0,0,0,1);
    
    const auto& buffer = it->second;
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