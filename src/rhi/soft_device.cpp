#include <rhi/soft_device.h>
#include <tinygl/base/log.h>
#include <cstring>
#include <rhi/command_buffer.h>

namespace rhi {

SoftDevice::SoftDevice(SoftRenderContext& ctx) : m_ctx(ctx) {
    m_uniformData.resize(MAX_UNIFORM_SIZE);
}

SoftDevice::~SoftDevice() {
    // Cleanup Buffers
    for (size_t i = 0; i < m_buffers.pool.size(); ++i) {
        if (m_buffers.pool[i].active) {
            m_ctx.glDeleteBuffers(1, &m_buffers.pool[i].resource.glId);
        }
    }
    // Cleanup Textures
    for (size_t i = 0; i < m_textures.pool.size(); ++i) {
        if (m_textures.pool[i].active) {
            m_ctx.glDeleteTextures(1, &m_textures.pool[i].resource.glId);
        }
    }
    // Pipelines are unique_ptrs, cleaned up by vector destructor
}

// --- Resources ---

BufferHandle SoftDevice::CreateBuffer(const BufferDesc& desc) {
    BufferRes res;
    res.type = desc.type;
    m_ctx.glGenBuffers(1, &res.glId);
    
    GLenum target = (desc.type == BufferType::IndexBuffer) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
    m_ctx.glBindBuffer(target, res.glId);
    
    if (desc.size > 0) {
        m_ctx.glBufferData(target, desc.size, desc.initialData, GL_STATIC_DRAW);
    }
    
    uint32_t id = m_buffers.Allocate(std::move(res));
    return {id};
}

void SoftDevice::DestroyBuffer(BufferHandle handle) {
    BufferRes* res = m_buffers.Get(handle.id);
    if (res) {
        m_ctx.glDeleteBuffers(1, &res->glId);
        m_buffers.Release(handle.id);
    }
}

void SoftDevice::UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
    BufferRes* res = m_buffers.Get(handle.id);
    if (res) {
        GLenum target = (res->type == BufferType::IndexBuffer) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
        m_ctx.glBindBuffer(target, res->glId);
        if (offset == 0) {
            m_ctx.glBufferData(target, size, data, GL_STATIC_DRAW);
        } else {
            // TODO: Add glBufferSubData if needed, but SoftRasterizer usually updates whole buffer or maps it
            // For now, re-upload is fine for small buffers
            m_ctx.glBufferData(target, size, data, GL_STATIC_DRAW);
        }
    }
}

TextureHandle SoftDevice::CreateTexture(const void* pixelData, int width, int height, int channels) {
    TextureRes res;
    res.owned = true;
    m_ctx.glGenTextures(1, &res.glId);
    m_ctx.glBindTexture(GL_TEXTURE_2D, res.glId);
    
    GLenum format = (channels == 4) ? GL_RGBA : GL_RGB;
    m_ctx.glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, pixelData);
    
    uint32_t id = m_textures.Allocate(std::move(res));
    return {id};
}

TextureHandle SoftDevice::CreateTextureFromNative(GLuint glTextureId) {
    TextureRes res;
    res.glId = glTextureId;
    res.owned = false;
    
    uint32_t id = m_textures.Allocate(std::move(res));
    return {id};
}

void SoftDevice::DestroyTexture(TextureHandle handle) {
    TextureRes* res = m_textures.Get(handle.id);
    if (res) {
        if (res->owned) {
            m_ctx.glDeleteTextures(1, &res->glId);
        }
        m_textures.Release(handle.id);
    }
}

// --- Pipelines & Shaders ---

ShaderHandle SoftDevice::CreateShaderHandle() {
    return { m_nextShaderId++ };
}

void SoftDevice::RegisterShaderFactory(ShaderHandle handle, PipelineFactory factory) {
    m_shaderFactories[handle.id] = factory;
}

PipelineHandle SoftDevice::CreatePipeline(const PipelineDesc& desc) {
    auto factoryIt = m_shaderFactories.find(desc.shader.id);
    if (factoryIt == m_shaderFactories.end()) {
        LOG_ERROR("Unknown shader handle in CreatePipeline");
        return {0};
    }
    
    auto pipeline = factoryIt->second(m_ctx, desc);
    uint32_t id = m_pipelines.Allocate(std::move(pipeline));
    return {id};
}

void SoftDevice::DestroyPipeline(PipelineHandle handle) {
    m_pipelines.Release(handle.id);
}

// --- Execution ---

void SoftDevice::Submit(const CommandBuffer& buffer) {
    // Reset runtime state trackers at the beginning of a command batch
    m_activePipelineId = 0;
    m_activeVBOId = 0;
    m_activeIBOId = 0;
    std::memset(m_activeTextureIds, 0, sizeof(m_activeTextureIds));

    m_currentPipeline = nullptr;
    
    const uint8_t* ptr = buffer.GetData();
    const uint8_t* end = ptr + buffer.GetSize();

    while (ptr < end) {
        const auto* header = reinterpret_cast<const CommandPacket*>(ptr);
        
        switch (header->type) {
            case CommandType::SetPipeline: {
                const auto* pkt = reinterpret_cast<const PacketSetPipeline*>(ptr);
                if (pkt->handle.id != m_activePipelineId) {
                    auto* pipelinePtr = m_pipelines.Get(pkt->handle.id);
                    if (pipelinePtr && *pipelinePtr) {
                        m_currentPipeline = pipelinePtr->get();
                        m_activePipelineId = pkt->handle.id;
                    } else {
                        m_currentPipeline = nullptr;
                        m_activePipelineId = 0;
                    }
                }
                break;
            }
            
            case CommandType::SetViewport: {
                const auto* pkt = reinterpret_cast<const PacketSetViewport*>(ptr);
                m_ctx.glViewport(pkt->x, pkt->y, pkt->w, pkt->h);
                break;
            }
            
            case CommandType::SetVertexBuffer: {
                const auto* pkt = reinterpret_cast<const PacketSetBuffer*>(ptr);
                if (pkt->handle.id != m_activeVBOId) {
                    BufferRes* res = m_buffers.Get(pkt->handle.id);
                    if (res) {
                        m_ctx.glBindBuffer(GL_ARRAY_BUFFER, res->glId);
                        m_activeVBOId = pkt->handle.id;
                    } else {
                        m_activeVBOId = 0;
                    }
                }
                m_currentVertexBufferOffset = pkt->offset;
                break;
            }
            
            case CommandType::SetIndexBuffer: {
                const auto* pkt = reinterpret_cast<const PacketSetBuffer*>(ptr);
                if (pkt->handle.id != m_activeIBOId) {
                    BufferRes* res = m_buffers.Get(pkt->handle.id);
                    if (res) {
                        m_ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res->glId);
                        m_activeIBOId = pkt->handle.id;
                    } else {
                        m_activeIBOId = 0;
                    }
                }
                break;
            }

            case CommandType::SetTexture: {
                const auto* pkt = reinterpret_cast<const PacketSetTexture*>(ptr);
                uint8_t slot = pkt->slot;
                if (slot < 8) { 
                    if (m_activeTextureIds[slot] != pkt->handle.id) {
                        TextureRes* res = m_textures.Get(pkt->handle.id);
                        if (res) {
                            m_ctx.glActiveTexture(GL_TEXTURE0 + slot);
                            m_ctx.glBindTexture(GL_TEXTURE_2D, res->glId);
                            m_activeTextureIds[slot] = pkt->handle.id;
                        } else {
                             m_activeTextureIds[slot] = 0;
                        }
                    }
                }
                break;
            }

            case CommandType::UpdateUniform: {
                const auto* pkt = reinterpret_cast<const PacketUpdateUniform*>(ptr);
                // Data follows the struct
                const uint8_t* data = ptr + sizeof(PacketUpdateUniform);
                size_t dataSize = header->size - sizeof(PacketUpdateUniform);
                
                // Copy to internal storage
                // For now, copy to fixed slots
                size_t dstOffset = pkt->slot * 256; 
                if (dstOffset + dataSize <= m_uniformData.size()) {
                    memcpy(m_uniformData.data() + dstOffset, data, dataSize);
                }
                break;
            }

            case CommandType::Draw: {
                const auto* pkt = reinterpret_cast<const PacketDraw*>(ptr);
                
                if (m_currentPipeline) {
                    uint32_t vboGLId = 0;
                    if (BufferRes* res = m_buffers.Get(m_activeVBOId)) {
                        vboGLId = res->glId;
                    }
                    m_currentPipeline->Draw(m_ctx, m_uniformData, 
                                            pkt->vertexCount, pkt->firstVertex, pkt->instanceCount,
                                            vboGLId, m_currentVertexBufferOffset);
                }
                break;
            }

            case CommandType::DrawIndexed: {
                const auto* pkt = reinterpret_cast<const PacketDrawIndexed*>(ptr);

                if (m_currentPipeline) {
                    uint32_t vboGLId = 0;
                    if (BufferRes* res = m_buffers.Get(m_activeVBOId)) {
                        vboGLId = res->glId;
                    }
                    uint32_t iboGLId = 0;
                    if (BufferRes* res = m_buffers.Get(m_activeIBOId)) {
                        iboGLId = res->glId;
                    }
                    m_currentPipeline->DrawIndexed(m_ctx, m_uniformData, 
                                                   pkt->indexCount, pkt->firstIndex, pkt->baseVertex, pkt->instanceCount,
                                                   vboGLId, iboGLId, m_currentVertexBufferOffset);
                }
                break;
            }
            
            case CommandType::Clear: {
                 const auto* pkt = reinterpret_cast<const PacketClear*>(ptr);
                 
                 GLbitfield mask = 0;
                 if (pkt->color) mask |= GL_COLOR_BUFFER_BIT;
                 if (pkt->depth) mask |= GL_DEPTH_BUFFER_BIT;
                 if (pkt->stencil) mask |= GL_STENCIL_BUFFER_BIT;
                 
                 m_ctx.glClearColor(pkt->r, pkt->g, pkt->b, pkt->a);
                 m_ctx.glClear(mask);
                 break;
            }
            
            case CommandType::SetScissor:
                 break;
                 
            case CommandType::NoOp:
                break;
        }

        // Advance to next packet
        ptr += header->size;
    }
}

void SoftDevice::Present() {
    // Present hook
}

}