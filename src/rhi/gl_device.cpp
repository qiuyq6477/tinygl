#include <third_party/glad/glad.h>
#include <rhi/gl_device.h>
#include <rhi/shader_registry.h>
#include <iostream>
#include <cstring>
#include <algorithm>

namespace rhi {

namespace {
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
        switch (format) {
            case VertexFormat::Float1: 
            case VertexFormat::Float2: 
            case VertexFormat::Float3: 
            case VertexFormat::Float4: return GL_FLOAT;
            case VertexFormat::UByte4: 
            case VertexFormat::UByte4N: return GL_UNSIGNED_BYTE;
            default: return GL_FLOAT;
        }
    }

    GLboolean IsNormalized(VertexFormat format) {
        return (format == VertexFormat::UByte4N) ? GL_TRUE : GL_FALSE;
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

    GLenum ToGLBlendFactor(BlendFactor factor) {
        switch (factor) {
            case BlendFactor::Zero: return GL_ZERO;
            case BlendFactor::One: return GL_ONE;
            case BlendFactor::SrcColor: return GL_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
            case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstColor: return GL_DST_COLOR;
            case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
            case BlendFactor::DstAlpha: return GL_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
            default: return GL_ONE;
        }
    }

    GLenum ToGLBlendOp(BlendOp op) {
        switch (op) {
            case BlendOp::Add: return GL_FUNC_ADD;
            case BlendOp::Subtract: return GL_FUNC_SUBTRACT;
            case BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
            case BlendOp::Min: return GL_MIN;
            case BlendOp::Max: return GL_MAX;
            default: return GL_FUNC_ADD;
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

GLDevice::GLDevice() {
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        std::cerr << "GLDevice: GLAD not initialized!" << std::endl;
    }
    std::memset(m_bindings, 0, sizeof(m_bindings));

    // Initialize Global UBO for Uniform Slots
    glGenBuffers(1, &m_globalUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_globalUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(m_uniformStaging), nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

GLDevice::~GLDevice() {
    if (m_globalUBO) glDeleteBuffers(1, &m_globalUBO);
    for (auto& pair : m_buffers) glDeleteBuffers(1, &pair.second.id);
    for (auto& pair : m_textures) glDeleteTextures(1, &pair.second);
    for (auto& pair : m_pipelines) {
        glDeleteVertexArrays(1, &pair.second.vao);
    }
    for (auto& pair : m_shaderPrograms) glDeleteProgram(pair.second);
    
    m_buffers.clear();
    m_textures.clear();
    m_pipelines.clear();
    m_shaderPrograms.clear();
}

void GLDevice::BindBuffer(GLenum target, GLuint id) {
    if (target == GL_ARRAY_BUFFER) {
        glBindBuffer(target, id);
    } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
        glBindBuffer(target, id);
    } else {
        glBindBuffer(target, id);
    }
}



void GLDevice::BindVertexArray(GLuint id) {
    glBindVertexArray(id);
}

void GLDevice::UseProgram(GLuint id) {
    glUseProgram(id);
}

BufferHandle GLDevice::CreateBuffer(const BufferDesc& desc) {
    GLuint id;
    glGenBuffers(1, &id);
    GLenum target = ToGLBufferTarget(desc.type);
    BindBuffer(target, id);
    if (desc.size > 0) {
        glBufferData(target, desc.size, desc.initialData, ToGLUsage(desc.usage));
    }
    uint32_t handle = m_nextBufferHandle++;
    m_buffers[handle] = {id, target};
    return {handle};
}

void GLDevice::DestroyBuffer(BufferHandle handle) {
    if (m_buffers.count(handle.id)) {
        GLuint id = m_buffers[handle.id].id;
        glDeleteBuffers(1, &id);
        m_buffers.erase(handle.id);
    }
}

void GLDevice::UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
    if (m_buffers.count(handle.id)) {
        const auto& meta = m_buffers[handle.id];
        BindBuffer(meta.target, meta.id);
        glBufferSubData(meta.target, offset, size, data);
    }
}

TextureHandle GLDevice::CreateTexture(const void* pixelData, int width, int height, int channels) {
    GLuint id;
    glGenTextures(1, &id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, id);

    GLenum format = GL_RGBA;
    if (channels == 3) format = GL_RGB;
    else if (channels == 1) format = GL_RED;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixelData);
    if (pixelData) glGenerateMipmap(GL_TEXTURE_2D);

    uint32_t handle = m_nextTextureHandle++;
    m_textures[handle] = id;
    return {handle};
}

void GLDevice::DestroyTexture(TextureHandle handle) {
    if (m_textures.count(handle.id)) {
        GLuint id = m_textures[handle.id];
        glDeleteTextures(1, &id);
        m_textures.erase(handle.id);
    }
}

PipelineHandle GLDevice::CreatePipeline(const PipelineDesc& desc) {
    GLuint program = 0;
    if (desc.shader.IsValid()) {
        if (m_shaderPrograms.count(desc.shader.id)) {
            program = m_shaderPrograms[desc.shader.id];
        } else {
            const auto* shaderDesc = ShaderRegistry::GetInstance().GetDesc(desc.shader);
            if (shaderDesc) {
                GLuint vs = CompileShader(GL_VERTEX_SHADER, shaderDesc->glsl.vertex);
                GLuint fs = CompileShader(GL_FRAGMENT_SHADER, shaderDesc->glsl.fragment);
                if (vs && fs) {
                    program = LinkProgram(vs, fs);
                    m_shaderPrograms[desc.shader.id] = program;
                }
                if (vs) glDeleteShader(vs);
                if (fs) glDeleteShader(fs);
            }
        }
    }

    GLuint vao;
    glGenVertexArrays(1, &vao);
    BindVertexArray(vao);
    
    // Initial setup with enabled arrays. Pointers are set at Draw time.
    for (const auto& attr : desc.inputLayout.attributes) {
        glEnableVertexAttribArray(attr.shaderLocation);
    }
    BindVertexArray(0);

    uint32_t handle = m_nextPipelineHandle++;
    m_pipelines[handle] = {program, vao, desc};
    return {handle};
}

void GLDevice::DestroyPipeline(PipelineHandle handle) {
    if (m_pipelines.count(handle.id)) {
        glDeleteVertexArrays(1, &m_pipelines[handle.id].vao);
        m_pipelines.erase(handle.id);
    }
}

void GLDevice::Submit(const CommandBuffer& buffer) {
    if (buffer.IsEmpty()) return;

    uint32_t currentPipelineId = 0;
    PipelineMeta* currentPipelineMeta = nullptr;
    
    // Reset frame state
    std::memset(m_bindings, 0, sizeof(m_bindings));
    m_activeIBO = 0;

    const uint8_t* ptr = buffer.GetData();
    const uint8_t* end = ptr + buffer.GetSize();

    while (ptr < end) {
        const auto* header = reinterpret_cast<const CommandPacket*>(ptr);
        
        switch (header->type) {
            case CommandType::SetPipeline: {
                const auto* pkt = reinterpret_cast<const PacketSetPipeline*>(ptr);
                if (currentPipelineId != pkt->handle.id) {
                    currentPipelineId = pkt->handle.id;
                    if (m_pipelines.count(currentPipelineId)) {
                        currentPipelineMeta = &m_pipelines[currentPipelineId];
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

                        // Apply Pipeline State
                        const auto& desc = currentPipelineMeta->desc;
                        
                        // Cull Mode
                        if (desc.cullMode == CullMode::None) {
                            glDisable(GL_CULL_FACE);
                        } else {
                            glEnable(GL_CULL_FACE);
                            glCullFace(desc.cullMode == CullMode::Back ? GL_BACK : GL_FRONT);
                        }

                        // Depth State
                        if (desc.depthTestEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
                        glDepthMask(desc.depthWriteEnabled ? GL_TRUE : GL_FALSE);

                        // Blend State
                        if (desc.blend.enabled) {
                            glEnable(GL_BLEND);
                            glBlendFuncSeparate(
                                ToGLBlendFactor(desc.blend.srcRGB), ToGLBlendFactor(desc.blend.dstRGB),
                                ToGLBlendFactor(desc.blend.srcAlpha), ToGLBlendFactor(desc.blend.dstAlpha)
                            );
                            glBlendEquationSeparate(ToGLBlendOp(desc.blend.opRGB), ToGLBlendOp(desc.blend.opAlpha));
                        } else {
                            glDisable(GL_BLEND);
                        }
                    }
                }
                break;
            }
            case CommandType::SetVertexStream: {
                const auto* pkt = reinterpret_cast<const PacketSetVertexStream*>(ptr);
                if (pkt->bindingIndex < MAX_BINDINGS) {
                    m_bindings[pkt->bindingIndex].bufferId = pkt->handle.id;
                    m_bindings[pkt->bindingIndex].offset = pkt->offset;
                    m_bindings[pkt->bindingIndex].stride = pkt->stride;
                }
                break;
            }
            case CommandType::SetIndexBuffer: {
                const auto* pkt = reinterpret_cast<const PacketSetIndexBuffer*>(ptr);
                if (m_activeIBO != pkt->handle.id) {
                    m_activeIBO = pkt->handle.id;
                    if (m_buffers.count(m_activeIBO)) {
                        BindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_buffers[m_activeIBO].id);
                    }
                }
                break;
            }
            case CommandType::SetTexture: {
                const auto* pkt = reinterpret_cast<const PacketSetTexture*>(ptr);
                glActiveTexture(GL_TEXTURE0 + pkt->slot);
                if (m_textures.count(pkt->handle.id)) {
                    glBindTexture(GL_TEXTURE_2D, m_textures[pkt->handle.id]);
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
                    memcpy(m_uniformStaging + pkt->slot * 256, data, copySize);
                    m_uniformsDirty = true;
                }
                break;
            }
            case CommandType::Draw: {
                const auto* pkt = reinterpret_cast<const PacketDraw*>(ptr);
                if (currentPipelineMeta) {
                    if (m_uniformsDirty) {
                        glBindBuffer(GL_UNIFORM_BUFFER, m_globalUBO);
                        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(m_uniformStaging), m_uniformStaging);
                        m_uniformsDirty = false;
                        
                        // Bind slots
                        for(int i=0; i<16; ++i) {
                            glBindBufferRange(GL_UNIFORM_BUFFER, i, m_globalUBO, i*256, 256);
                        }
                    }

                    // Apply Bindings to Attrib Pointers
                    const auto& layout = currentPipelineMeta->desc.inputLayout;
                    bool interleaved = currentPipelineMeta->desc.useInterleavedAttributes;

                    for (const auto& attr : layout.attributes) {
                        uint32_t bindingIdx = interleaved ? 0 : attr.shaderLocation;
                        // For safety, fallback to 0 if we requested planar but didn't bind anything at N
                        if (m_bindings[bindingIdx].bufferId == 0 && !interleaved) bindingIdx = 0;

                        const auto& binding = m_bindings[bindingIdx];
                        if (binding.bufferId != 0 && m_buffers.count(binding.bufferId)) {
                             BindBuffer(GL_ARRAY_BUFFER, m_buffers[binding.bufferId].id);
                             // Use provided stride, fallback to binding stride, fallback to pipeline stride
                             // If interleaved, usually binding.stride is 0 (set by legacy SetVertexBuffer), 
                             // so we use layout.stride.
                             GLsizei stride = binding.stride > 0 ? binding.stride : layout.stride;
                             
                             glVertexAttribPointer(
                                attr.shaderLocation,
                                ToGLSize(attr.format),
                                ToGLType(attr.format),
                                IsNormalized(attr.format),
                                stride,
                                (const void*)(uintptr_t)(binding.offset + attr.offset)
                            );
                        }
                    }
                    glDrawArrays(ToGLPrimitive(currentPipelineMeta->desc.primitiveType), pkt->firstVertex, pkt->vertexCount);
                }
                break;
            }
            case CommandType::DrawIndexed: {
                const auto* pkt = reinterpret_cast<const PacketDrawIndexed*>(ptr);
                if (currentPipelineMeta && m_activeIBO) {
                    if (m_uniformsDirty) {
                        glBindBuffer(GL_UNIFORM_BUFFER, m_globalUBO);
                        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(m_uniformStaging), m_uniformStaging);
                        m_uniformsDirty = false;
                        for(int i=0; i<16; ++i) {
                            glBindBufferRange(GL_UNIFORM_BUFFER, i, m_globalUBO, i*256, 256);
                        }
                    }

                    const auto& layout = currentPipelineMeta->desc.inputLayout;
                    bool interleaved = currentPipelineMeta->desc.useInterleavedAttributes;
                    
                    for (const auto& attr : layout.attributes) {
                        uint32_t bindingIdx = interleaved ? 0 : attr.shaderLocation;
                        if (m_bindings[bindingIdx].bufferId == 0 && !interleaved) bindingIdx = 0;

                        const auto& binding = m_bindings[bindingIdx];
                        if (binding.bufferId != 0 && m_buffers.count(binding.bufferId)) {
                             BindBuffer(GL_ARRAY_BUFFER, m_buffers[binding.bufferId].id);
                             GLsizei stride = binding.stride > 0 ? binding.stride : layout.stride;
                             glVertexAttribPointer(
                                attr.shaderLocation,
                                ToGLSize(attr.format),
                                ToGLType(attr.format),
                                IsNormalized(attr.format),
                                stride,
                                (const void*)(uintptr_t)(binding.offset + attr.offset)
                            );
                        }
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
            case CommandType::BeginPass: {
                 const auto* pkt = reinterpret_cast<const PacketBeginPass*>(ptr);

                 // 1. Invalidate Pipeline State (Stateless requirement)
                 currentPipelineId = 0;
                 currentPipelineMeta = nullptr;

                 // 2. Handle Load Actions (Clear)
                 // Ensure Scissor is DISABLED for Clear (Standard RenderPass behavior clears full attachment)
                 glDisable(GL_SCISSOR_TEST);

                 GLbitfield mask = 0;
                 if (pkt->colorLoadOp == LoadAction::Clear) {
                    glClearColor(pkt->clearColor[0], pkt->clearColor[1], pkt->clearColor[2], pkt->clearColor[3]);
                    mask |= GL_COLOR_BUFFER_BIT;
                 }
                 if (pkt->depthLoadOp == LoadAction::Clear) {
                    glClearDepth(pkt->clearDepth);
                    mask |= GL_DEPTH_BUFFER_BIT;
                    glDepthMask(GL_TRUE); // Ensure we can write to depth buffer
                 }
                 if (mask != 0) {
                    glClear(mask);
                 }

                 // 3. Apply Scissor State
                 if (pkt->scW >= 0 && pkt->scH >= 0) {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(pkt->scX, pkt->scY, pkt->scW, pkt->scH);
                 }
                 // If invalid scissor, it remains disabled from step 2.

                 // 4. Reset Viewport State (if specified)
                 if (pkt->vpW >= 0 && pkt->vpH >= 0) {
                    glViewport(pkt->vpX, pkt->vpY, pkt->vpW, pkt->vpH);
                 }
                 break;
            }
            case CommandType::EndPass: {
                 // Store actions (No-op for Default Framebuffer)
                 break;
            }
            case CommandType::SetViewport: {
                const auto* pkt = reinterpret_cast<const PacketSetViewport*>(ptr);
                glViewport((GLint)pkt->x, (GLint)pkt->y, (GLsizei)pkt->w, (GLsizei)pkt->h);
                break;
            }
            case CommandType::SetScissor: {
                const auto* pkt = reinterpret_cast<const PacketSetScissor*>(ptr);
                if (pkt->w >= 0 && pkt->h >= 0) {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(pkt->x, pkt->y, pkt->w, pkt->h);
                } else {
                    glDisable(GL_SCISSOR_TEST);
                }
                break;
            }
            case CommandType::NoOp:
                break;
        }

        ptr += header->size;
    }
}

void GLDevice::Present() {
}

} // namespace rhi
