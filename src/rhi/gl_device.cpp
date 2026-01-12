#include <third_party/glad/glad.h>
#include <rhi/gl_device.h>
#include <rhi/shader_registry.h>
#include <iostream>
#include <unordered_map>
#include <vector>
#include <rhi/command_buffer.h>

namespace rhi {

namespace {
    // --- State Cache (Internal) ---
    struct GLState {
        GLuint boundVBO = 0;
        GLuint boundIBO = 0;
        GLuint boundVAO = 0;
        GLuint boundProgram = 0;
        GLuint boundTexture = 0;
    } g_state;

    // --- Enum Conversions ---
    GLenum ToGLBufferTarget(BufferType type) {
        switch (type) {
            case BufferType::VertexBuffer: return GL_ARRAY_BUFFER;
            case BufferType::IndexBuffer: return GL_ELEMENT_ARRAY_BUFFER;
            case BufferType::UniformBuffer: return GL_UNIFORM_BUFFER;
            default: return GL_ARRAY_BUFFER;
        }
    }

    GLenum ToGLUsage(BufferUsage usage) {
        switch (usage) {
            case BufferUsage::Immutable: return GL_STATIC_DRAW;
            case BufferUsage::Dynamic:   return GL_DYNAMIC_DRAW;
            case BufferUsage::Stream:    return GL_STREAM_DRAW;
            default: return GL_STATIC_DRAW;
        }
    }

    GLenum ToGLPrimitive(PrimitiveType type) {
        switch (type) {
            case PrimitiveType::Triangles: return GL_TRIANGLES;
            case PrimitiveType::Lines:     return GL_LINES;
            case PrimitiveType::Points:    return GL_POINTS;
            default: return GL_TRIANGLES;
        }
    }

    GLenum ToGLType(VertexFormat format) {
        return GL_FLOAT; 
    }

    GLint ToGLSize(VertexFormat format) {
        switch (format) {
            case VertexFormat::Float1: return 1;
            case VertexFormat::Float2: return 2;
            case VertexFormat::Float3: return 3;
            case VertexFormat::Float4: return 4;
            case VertexFormat::UByte4: return 4;
            case VertexFormat::UByte4N: return 4;
            default: return 4;
        }
    }

    // --- State Helpers ---
    void BindBuffer(GLenum target, GLuint id) {
        if (target == GL_ARRAY_BUFFER) {
            if (g_state.boundVBO != id) {
                glBindBuffer(target, id);
                g_state.boundVBO = id;
            }
        } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
            glBindBuffer(target, id); 
        } else {
            glBindBuffer(target, id);
        }
    }

    void BindVertexArray(GLuint id) {
        if (g_state.boundVAO != id) {
            glBindVertexArray(id);
            g_state.boundVAO = id;
        }
    }

    void UseProgram(GLuint id) {
        if (g_state.boundProgram != id) {
            glUseProgram(id);
            g_state.boundProgram = id;
        }
    }

    // --- Shader Compiler ---
    GLuint CompileShader(GLenum type, const std::string& source) {
        if (source.empty()) return 0;
        
        GLuint shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint success;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetShaderInfoLog(shader, 512, nullptr, infoLog);
            std::cerr << "Shader Compilation Error (" << (type == GL_VERTEX_SHADER ? "VS" : "FS") << "):\n" << infoLog << std::endl;
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    GLuint LinkProgram(GLuint vs, GLuint fs) {
        GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        GLint success;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            std::cerr << "Program Linking Error:\n" << infoLog << std::endl;
            glDeleteProgram(program);
            return 0;
        }
        return program;
    }
} // namespace

// --- Resource Metadata ---
struct BufferMeta {
    GLuint id;
    GLenum target;
};

struct PipelineMeta {
    GLuint program;
    GLuint vao;
    PipelineDesc desc;
};

static std::unordered_map<uint32_t, BufferMeta> s_buffers;
static std::unordered_map<uint32_t, GLuint> s_textures;
static std::unordered_map<uint32_t, PipelineMeta> s_pipelines;
static std::unordered_map<uint32_t, GLuint> s_shaderPrograms;

static uint32_t s_nextBufferHandle = 1;
static uint32_t s_nextTextureHandle = 1;
static uint32_t s_nextPipelineHandle = 1;

static GLuint s_globalUBO = 0;
static uint8_t s_uniformStaging[16 * 256];
static bool s_uniformsDirty = false;

GLDevice::GLDevice() {
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "GLDevice: GLAD not initialized!" << std::endl;
    }
    g_state = {};

    // Initialize Global UBO for Uniform Slots
    glGenBuffers(1, &s_globalUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, s_globalUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(s_uniformStaging), nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

GLDevice::~GLDevice() {
    if (s_globalUBO) glDeleteBuffers(1, &s_globalUBO);
    for (auto& pair : s_buffers) glDeleteBuffers(1, &pair.second.id);
    for (auto& pair : s_textures) glDeleteTextures(1, &pair.second);
    for (auto& pair : s_pipelines) {
        glDeleteVertexArrays(1, &pair.second.vao);
    }
    for (auto& pair : s_shaderPrograms) glDeleteProgram(pair.second);
    
    s_buffers.clear();
    s_textures.clear();
    s_pipelines.clear();
    s_shaderPrograms.clear();
}

BufferHandle GLDevice::CreateBuffer(const BufferDesc& desc) {
    GLuint id;
    glGenBuffers(1, &id);
    GLenum target = ToGLBufferTarget(desc.type);
    BindBuffer(target, id);
    if (desc.size > 0) {
        glBufferData(target, desc.size, desc.initialData, ToGLUsage(desc.usage));
    }
    uint32_t handle = s_nextBufferHandle++;
    s_buffers[handle] = {id, target};
    return {handle};
}

void GLDevice::DestroyBuffer(BufferHandle handle) {
    if (s_buffers.count(handle.id)) {
        GLuint id = s_buffers[handle.id].id;
        glDeleteBuffers(1, &id);
        s_buffers.erase(handle.id);
        if (g_state.boundVBO == id) g_state.boundVBO = 0;
        if (g_state.boundIBO == id) g_state.boundIBO = 0;
    }
}

void GLDevice::UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
    if (s_buffers.count(handle.id)) {
        const auto& meta = s_buffers[handle.id];
        BindBuffer(meta.target, meta.id);
        glBufferSubData(meta.target, offset, size, data);
    }
}

TextureHandle GLDevice::CreateTexture(const void* pixelData, int width, int height, int channels) {
    GLuint id;
    glGenTextures(1, &id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);
    g_state.boundTexture = id;

    GLenum format = GL_RGBA;
    if (channels == 3) format = GL_RGB;
    else if (channels == 1) format = GL_RED;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixelData);
    if (pixelData) glGenerateMipmap(GL_TEXTURE_2D);

    uint32_t handle = s_nextTextureHandle++;
    s_textures[handle] = id;
    return {handle};
}

void GLDevice::DestroyTexture(TextureHandle handle) {
    if (s_textures.count(handle.id)) {
        GLuint id = s_textures[handle.id];
        glDeleteTextures(1, &id);
        s_textures.erase(handle.id);
        if (g_state.boundTexture == id) g_state.boundTexture = 0;
    }
}

PipelineHandle GLDevice::CreatePipeline(const PipelineDesc& desc) {
    GLuint program = 0;
    if (desc.shader.IsValid()) {
        if (s_shaderPrograms.count(desc.shader.id)) {
            program = s_shaderPrograms[desc.shader.id];
        } else {
            const auto* shaderDesc = ShaderRegistry::Get().GetDesc(desc.shader.id);
            if (shaderDesc) {
                GLuint vs = CompileShader(GL_VERTEX_SHADER, shaderDesc->glsl.vertex);
                GLuint fs = CompileShader(GL_FRAGMENT_SHADER, shaderDesc->glsl.fragment);
                if (vs && fs) {
                    program = LinkProgram(vs, fs);
                    s_shaderPrograms[desc.shader.id] = program;
                }
                if (vs) glDeleteShader(vs);
                if (fs) glDeleteShader(fs);
            }
        }
    }

    GLuint vao;
    glGenVertexArrays(1, &vao);
    BindVertexArray(vao);
    for (const auto& attr : desc.inputLayout.attributes) {
        glEnableVertexAttribArray(attr.shaderLocation);
    }
    BindVertexArray(0);

    uint32_t handle = s_nextPipelineHandle++;
    s_pipelines[handle] = {program, vao, desc};
    return {handle};
}

void GLDevice::DestroyPipeline(PipelineHandle handle) {
    if (s_pipelines.count(handle.id)) {
        glDeleteVertexArrays(1, &s_pipelines[handle.id].vao);
        s_pipelines.erase(handle.id);
    }
}

void GLDevice::Submit(const CommandBuffer& buffer) {
    if (buffer.IsEmpty()) {
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        return;
    }

    uint32_t currentPipelineId = 0;
    uint32_t currentVBO = 0;
    uint32_t currentIBO = 0;
    PipelineMeta* currentPipelineMeta = nullptr;

    const uint8_t* ptr = buffer.GetData();
    const uint8_t* end = ptr + buffer.GetSize();

    while (ptr < end) {
        const auto* header = reinterpret_cast<const CommandPacket*>(ptr);
        
        switch (header->type) {
            case CommandType::SetPipeline: {
                const auto* pkt = reinterpret_cast<const PacketSetPipeline*>(ptr);
                if (currentPipelineId != pkt->handle.id) {
                    currentPipelineId = pkt->handle.id;
                    if (s_pipelines.count(currentPipelineId)) {
                        currentPipelineMeta = &s_pipelines[currentPipelineId];
                        UseProgram(currentPipelineMeta->program);
                        BindVertexArray(currentPipelineMeta->vao);

                        // Bind Uniform Blocks to Slots (if program changed)
                        GLuint prog = currentPipelineMeta->program;
                        // For simplicity, we assume blocks are named Slot0, Slot1, etc. or just bind the first block to 0.
                        // Better: Iterate active uniform blocks
                        GLint numBlocks;
                        glGetProgramiv(prog, GL_ACTIVE_UNIFORM_BLOCKS, &numBlocks);
                        for(int i=0; i<numBlocks; ++i) {
                            char name[64];
                            glGetActiveUniformBlockName(prog, i, 64, nullptr, name);
                            if(strncmp(name, "Slot", 4) == 0) {
                                int slot = atoi(name + 4);
                                glUniformBlockBinding(prog, i, slot);
                            } else {
                                glUniformBlockBinding(prog, i, i);
                            }
                        }
                    }
                }
                break;
            }
            case CommandType::SetVertexBuffer: {
                const auto* pkt = reinterpret_cast<const PacketSetBuffer*>(ptr);
                if (currentVBO != pkt->handle.id) {
                    currentVBO = pkt->handle.id;
                    if (s_buffers.count(currentVBO)) {
                        BindBuffer(GL_ARRAY_BUFFER, s_buffers[currentVBO].id);
                    }
                }
                break;
            }
            case CommandType::SetIndexBuffer: {
                const auto* pkt = reinterpret_cast<const PacketSetBuffer*>(ptr);
                if (currentIBO != pkt->handle.id) {
                    currentIBO = pkt->handle.id;
                    if (s_buffers.count(currentIBO)) {
                        BindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_buffers[currentIBO].id);
                    }
                }
                break;
            }
            case CommandType::SetTexture: {
                const auto* pkt = reinterpret_cast<const PacketSetTexture*>(ptr);
                glActiveTexture(GL_TEXTURE0 + pkt->slot);
                if (s_textures.count(pkt->handle.id)) {
                    glBindTexture(GL_TEXTURE_2D, s_textures[pkt->handle.id]);
                } else {
                    glBindTexture(GL_TEXTURE_2D, 0);
                }
                break;
            }
            case CommandType::UpdateUniform: {
                const auto* pkt = reinterpret_cast<const PacketUpdateUniform*>(ptr);
                const uint8_t* data = ptr + sizeof(PacketUpdateUniform);
                size_t dataSize = header->size - sizeof(PacketUpdateUniform);

                if (dataSize > 0 && pkt->slot < 16) {
                    // Current GLDevice assumes fixed 256 byte slots for now, need to be careful not to overflow
                    size_t copySize = std::min(dataSize, (size_t)256); 
                    memcpy(s_uniformStaging + pkt->slot * 256, data, copySize);
                    s_uniformsDirty = true;
                }
                break;
            }
            case CommandType::Draw: {
                const auto* pkt = reinterpret_cast<const PacketDraw*>(ptr);
                if (currentPipelineMeta && currentVBO) {
                    if (s_uniformsDirty) {
                        glBindBuffer(GL_UNIFORM_BUFFER, s_globalUBO);
                        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(s_uniformStaging), s_uniformStaging);
                        s_uniformsDirty = false;
                        
                        // Bind slots
                        for(int i=0; i<16; ++i) {
                            glBindBufferRange(GL_UNIFORM_BUFFER, i, s_globalUBO, i*256, 256);
                        }
                    }

                    const auto& layout = currentPipelineMeta->desc.inputLayout;
                    GLsizei stride = layout.stride;
                    for (const auto& attr : layout.attributes) {
                        glVertexAttribPointer(
                            attr.shaderLocation,
                            ToGLSize(attr.format),
                            ToGLType(attr.format),
                            GL_FALSE,
                            stride,
                            (const void*)(uintptr_t)attr.offset
                        );
                    }
                    glDrawArrays(ToGLPrimitive(currentPipelineMeta->desc.primitiveType), pkt->firstVertex, pkt->vertexCount);
                }
                break;
            }
            case CommandType::DrawIndexed: {
                const auto* pkt = reinterpret_cast<const PacketDrawIndexed*>(ptr);
                if (currentPipelineMeta && currentVBO && currentIBO) {
                    if (s_uniformsDirty) {
                        glBindBuffer(GL_UNIFORM_BUFFER, s_globalUBO);
                        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(s_uniformStaging), s_uniformStaging);
                        s_uniformsDirty = false;

                        for(int i=0; i<16; ++i) {
                            glBindBufferRange(GL_UNIFORM_BUFFER, i, s_globalUBO, i*256, 256);
                        }
                    }

                    const auto& layout = currentPipelineMeta->desc.inputLayout;
                    GLsizei stride = layout.stride;
                    for (const auto& attr : layout.attributes) {
                        glVertexAttribPointer(
                            attr.shaderLocation,
                            ToGLSize(attr.format),
                            ToGLType(attr.format),
                            GL_FALSE,
                            stride,
                            (const void*)(uintptr_t)attr.offset
                        );
                    }
                    glDrawElements(ToGLPrimitive(currentPipelineMeta->desc.primitiveType), pkt->indexCount, GL_UNSIGNED_INT, (const void*)(uintptr_t)(pkt->firstIndex * 4));
                }
                break;
            }
            case CommandType::Clear: {
                const auto* pkt = reinterpret_cast<const PacketClear*>(ptr);
                GLbitfield mask = 0;
                if (pkt->color) mask |= GL_COLOR_BUFFER_BIT;
                if (pkt->depth) mask |= GL_DEPTH_BUFFER_BIT;
                if (pkt->stencil) mask |= GL_STENCIL_BUFFER_BIT;
                glClearColor(pkt->r, pkt->g, pkt->b, pkt->a);
                glClear(mask);
                break;
            }
            case CommandType::SetViewport: {
                const auto* pkt = reinterpret_cast<const PacketSetViewport*>(ptr);
                glViewport((GLint)pkt->x, (GLint)pkt->y, (GLsizei)pkt->w, (GLsizei)pkt->h);
                break;
            }
            case CommandType::SetScissor:
                // TODO
                break;
            case CommandType::NoOp:
                break;
        }

        ptr += header->size;
    }
}

void GLDevice::Present() {
}

} // namespace rhi
