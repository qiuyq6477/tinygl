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

// --- DSA Buffers (Phase 1) ---
void SoftRenderContext::glCreateBuffers(GLsizei n, GLuint* res) {
    for (int i = 0; i < n; i++) {
        res[i] = buffers.allocate();
        // Initialize as DSA buffer
        BufferObject* buf = buffers.get(res[i]);
        if (buf) {
            buf->immutable = false;
        }
        LOG_INFO("CreateBuffer (DSA) ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags) {
    BufferObject* buf = buffers.get(buffer);
    if (!buf) {
        LOG_ERROR("glNamedBufferStorage: Invalid buffer ID " + std::to_string(buffer));
        return;
    }
    if (buf->immutable) {
        LOG_ERROR("glNamedBufferStorage: Buffer " + std::to_string(buffer) + " is already immutable");
        return;
    }

    buf->data.resize(size);
    if (data) std::memcpy(buf->data.data(), data, size);
    
    buf->immutable = true;
    buf->size = size;
    buf->storageFlags = flags;
    LOG_INFO("NamedBufferStorage " + std::to_string(size) + " bytes to ID " + std::to_string(buffer));
}

// --- VAO ---
void SoftRenderContext::glGenVertexArrays(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = vaos.allocate();
        LOG_INFO("GenVAO ID: " + std::to_string(res[i]));
    }
}

// --- DSA Vertex Arrays (Phase 1) ---
void SoftRenderContext::glCreateVertexArrays(GLsizei n, GLuint* arrays) {
    for (int i = 0; i < n; i++) {
        arrays[i] = vaos.allocate();
        LOG_INFO("CreateVertexArray (DSA) ID: " + std::to_string(arrays[i]));
    }
}

void SoftRenderContext::glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer) {
    VertexArrayObject* vao = vaos.get(vaobj);
    if (vao) {
        vao->elementBufferID = buffer;
        vao->isDirty = true;
    }
}

void SoftRenderContext::glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) {
    if (attribindex >= MAX_ATTRIBS) return;
    VertexArrayObject* vao = vaos.get(vaobj);
    if (vao) {
        VertexAttribFormat& fmt = vao->attributes[attribindex];
        fmt.size = size;
        fmt.type = type;
        fmt.normalized = normalized;
        fmt.relativeOffset = relativeoffset;
        vao->isDirty = true;
    }
}

void SoftRenderContext::glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex) {
    if (attribindex >= MAX_ATTRIBS || bindingindex >= MAX_BINDINGS) return;
    VertexArrayObject* vao = vaos.get(vaobj);
    if (vao) {
        vao->attributes[attribindex].bindingIndex = bindingindex;
        vao->isDirty = true;
    }
}

void SoftRenderContext::glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) {
    if (bindingindex >= MAX_BINDINGS) return;
    VertexArrayObject* vao = vaos.get(vaobj);
    if (vao) {
        VertexBufferBinding& binding = vao->bindings[bindingindex];
        binding.bufferID = buffer;
        binding.offset = offset;
        binding.stride = stride;
        vao->isDirty = true;
    }
}

void SoftRenderContext::glEnableVertexArrayAttrib(GLuint vaobj, GLuint index) {
    if (index >= MAX_ATTRIBS) return;
    VertexArrayObject* vao = vaos.get(vaobj);
    if (vao) {
        vao->attributes[index].enabled = true;
        vao->isDirty = true;
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
    VertexArrayObject& vao = getVAO();
    
    // Legacy mapping: Attribute index X binds to Binding Slot X
    VertexAttribFormat& attr = vao.attributes[index];
    attr.enabled = true; // glVertexAttribPointer implicitly enables? No, usually glEnableVertexAttribArray does.
                         // But in some drivers it might. TinyGL legacy code didn't enable here.
    attr.size = size;
    attr.type = type;
    attr.normalized = norm; 
    attr.relativeOffset = 0; // In legacy, offset is part of VBO binding
    attr.bindingIndex = index;

    VertexBufferBinding& bnd = vao.bindings[index];
    bnd.bufferID = m_boundArrayBuffer;
    bnd.offset = reinterpret_cast<GLintptr>(pointer);
    bnd.stride = stride ? stride : size * ((type == GL_UNSIGNED_BYTE) ? 1 : 4);
    
    vao.isDirty = true;
}

void SoftRenderContext::glEnableVertexAttribArray(GLuint index) {
    if (index < MAX_ATTRIBS) {
        VertexArrayObject& vao = getVAO();
        vao.attributes[index].enabled = true;
        vao.isDirty = true;
    }
}

void SoftRenderContext::glVertexAttribDivisor(GLuint index, GLuint divisor) {
    if (index < MAX_ATTRIBS) {
        VertexArrayObject& vao = getVAO();
        // Divisor is associated with the binding point in modern GL, 
        // but often mapped to attribute in early versions.
        // We'll put it in Binding Slot for now since that's where Instancing logic usually lives.
        GLuint bindingIndex = vao.attributes[index].bindingIndex;
        vao.bindings[bindingIndex].divisor = divisor;
        vao.isDirty = true;
    }
}

void SoftRenderContext::prepareDraw() {
    VertexArrayObject& vao = getVAO();
    if (!vao.isDirty) return;

    for (int i = 0; i < MAX_ATTRIBS; ++i) {
        VertexAttribFormat& fmt = vao.attributes[i];
        ResolvedAttribute& baked = vao.bakedAttributes[i];
        
        baked.enabled = fmt.enabled;
        if (!fmt.enabled) continue;

        // Copy format
        baked.size = fmt.size;
        baked.type = fmt.type;
        baked.normalized = fmt.normalized;

        // Resolve pointer
        if (fmt.bindingIndex < MAX_BINDINGS) {
            VertexBufferBinding& bnd = vao.bindings[fmt.bindingIndex];
            BufferObject* buffer = buffers.get(bnd.bufferID);
            
            if (buffer && !buffer->data.empty()) {
                size_t finalOffset = bnd.offset + fmt.relativeOffset;
                // Basic bounds check
                if (finalOffset < buffer->data.size()) {
                    baked.basePointer = buffer->data.data() + finalOffset;
                    baked.stride = bnd.stride;
                    baked.divisor = bnd.divisor;
                } else {
                    baked.basePointer = nullptr;
                }
            } else {
                baked.basePointer = nullptr;
            }
        } else {
            baked.basePointer = nullptr;
        }
    }
    
    vao.isDirty = false;
}

Vec4 SoftRenderContext::fetchAttribute(const ResolvedAttribute& attr, int vertexIdx, int instanceIdx) {
    if (!attr.enabled || !attr.basePointer) return Vec4(0,0,0,1);

    // Instancing Logic
    // If divisor is 0, use vertexIdx.
    // If divisor > 0, use instanceIdx / divisor.
    int effectiveIdx = (attr.divisor == 0) ? vertexIdx : (instanceIdx / attr.divisor);
    
    size_t stride = attr.stride;
    const uint8_t* src = attr.basePointer + effectiveIdx * stride;
    
    float raw[4] = {0,0,0,1};
    
    switch (attr.type) {
        case GL_FLOAT: {
            std::memcpy(raw, src, attr.size * sizeof(float));
            break;
        }
        case GL_UNSIGNED_BYTE: {
            uint8_t ubyte_raw[4] = {0,0,0,255};
            std::memcpy(ubyte_raw, src, attr.size * sizeof(uint8_t));
            
            if (attr.normalized) {
                for(int i=0; i<4; ++i) raw[i] = ubyte_raw[i] / 255.0f;
            } else {
                for(int i=0; i<4; ++i) raw[i] = (float)ubyte_raw[i];
            }
            break;
        }
        default:
            break;
    }
    
    return Vec4(raw[0], raw[1], raw[2], raw[3]);
}

}