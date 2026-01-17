#include <rhi/soft_device.h>
#include <rhi/shader_registry.h>
#include <tinygl/base/log.h>
#include <cstring>
#include <rhi/command_buffer.h>

namespace rhi {

SoftDevice::SoftDevice(SoftRenderContext& ctx) : m_ctx(ctx) {
    m_uniformData.resize(MAX_UNIFORM_SIZE);
    
    // Initialize Tile-Based Rendering Infrastructure
    m_frameMem.Init(16 * 1024 * 1024); // 16MB
    m_tiler.Init(m_ctx.getWidth(), m_ctx.getHeight(), 64); // 64x64 tiles
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
            m_ctx.glBufferData(target, size, data, GL_STATIC_DRAW); // Todo: subdata
        }
    }
}

TextureHandle SoftDevice::CreateTexture(const void* pixelData, int width, int height, int channels) {
    TextureRes res;
    res.owned = true;
    m_ctx.glGenTextures(1, &res.glId);
    m_ctx.glBindTexture(GL_TEXTURE_2D, res.glId);
    
    GLenum format = GL_RGBA;
    if (channels == 3) format = GL_RGB;
    else if (channels == 1) format = GL_RED;

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

PipelineHandle SoftDevice::CreatePipeline(const PipelineDesc& desc) {
    const ShaderDesc* shaderDesc = ShaderRegistry::GetInstance().GetDesc(desc.shader);
    
    if (!shaderDesc || !shaderDesc->softFactory) {
        LOG_ERROR("Unknown or invalid shader handle in CreatePipeline");
        return {0};
    }
    
    auto pipeline = shaderDesc->softFactory(m_ctx, desc);
    uint32_t id = m_pipelines.Allocate(std::move(pipeline));
    return {id};
}

void SoftDevice::DestroyPipeline(PipelineHandle handle) {
    m_pipelines.Release(handle.id);
}

// --- Execution ---

void SoftDevice::Submit(const CommandBuffer& buffer) {
    // Reset state
    m_activePipelineId = 0;
    std::memset(m_bindings, 0, sizeof(m_bindings));
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
            
            case CommandType::SetVertexStream: {
                const auto* pkt = reinterpret_cast<const PacketSetVertexStream*>(ptr);
                if (pkt->bindingIndex < MAX_BINDINGS) {
                    // Update Binding State
                    m_bindings[pkt->bindingIndex].bufferId = pkt->handle.id;
                    m_bindings[pkt->bindingIndex].offset = pkt->offset;
                    m_bindings[pkt->bindingIndex].stride = pkt->stride;
                }
                break;
            }
            
            case CommandType::SetIndexBuffer: {
                const auto* pkt = reinterpret_cast<const PacketSetIndexBuffer*>(ptr);
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
                    // Resolve Bindings to Arrays
                    uint32_t vboGLIds[MAX_BINDINGS];
                    uint32_t offsets[MAX_BINDINGS];
                    uint32_t strides[MAX_BINDINGS];
                    
                    for(int i=0; i<MAX_BINDINGS; ++i) {
                         uint32_t bufId = m_bindings[i].bufferId;
                         if (bufId != 0) {
                             if (BufferRes* res = m_buffers.Get(bufId)) {
                                 vboGLIds[i] = res->glId;
                                 offsets[i] = m_bindings[i].offset;
                                 strides[i] = m_bindings[i].stride;
                             } else {
                                 vboGLIds[i] = 0;
                             }
                         } else {
                             vboGLIds[i] = 0;
                         }
                    }

                    m_currentPipeline->Draw(m_ctx, m_uniformData, 
                                            pkt->vertexCount, pkt->firstVertex, pkt->instanceCount,
                                            vboGLIds, offsets, strides, MAX_BINDINGS);
                }
                break;
            }

            case CommandType::DrawIndexed: {
                const auto* pkt = reinterpret_cast<const PacketDrawIndexed*>(ptr);

                if (m_currentPipeline) {
                    uint32_t vboGLIds[MAX_BINDINGS];
                    uint32_t offsets[MAX_BINDINGS];
                    uint32_t strides[MAX_BINDINGS];
                    
                    for(int i=0; i<MAX_BINDINGS; ++i) {
                         uint32_t bufId = m_bindings[i].bufferId;
                         if (bufId != 0) {
                             if (BufferRes* res = m_buffers.Get(bufId)) {
                                 vboGLIds[i] = res->glId;
                                 offsets[i] = m_bindings[i].offset;
                                 strides[i] = m_bindings[i].stride;
                             } else {
                                 vboGLIds[i] = 0;
                             }
                         } else {
                             vboGLIds[i] = 0;
                         }
                    }

                    uint32_t iboGLId = 0;
                    if (BufferRes* res = m_buffers.Get(m_activeIBOId)) {
                        iboGLId = res->glId;
                    }
                    m_currentPipeline->DrawIndexed(m_ctx, m_uniformData, 
                                                   pkt->indexCount, pkt->firstIndex, pkt->baseVertex, pkt->instanceCount,
                                                   vboGLIds, offsets, strides, MAX_BINDINGS,
                                                   iboGLId);
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

            case CommandType::BeginPass: {
                 const auto* pkt = reinterpret_cast<const PacketBeginPass*>(ptr);
                 
                 // 1. Invalidate Pipeline State (Stateless requirement)
                 m_activePipelineId = 0;
                 m_currentPipeline = nullptr;

                 // 2. Configure Scissor for LoadAction (Clear)
                 if (pkt->raW >= 0 && pkt->raH >= 0) {
                    m_ctx.glEnable(GL_SCISSOR_TEST);
                    m_ctx.glScissor(pkt->raX, pkt->raY, pkt->raW, pkt->raH);
                 } else {
                    m_ctx.glDisable(GL_SCISSOR_TEST);
                 }

                 // 3. Handle Load Actions (Clear)
                 GLbitfield mask = 0;
                 if (pkt->colorLoadOp == LoadAction::Clear) {
                    m_ctx.glClearColor(pkt->clearColor[0], pkt->clearColor[1], pkt->clearColor[2], pkt->clearColor[3]);
                    mask |= GL_COLOR_BUFFER_BIT;
                 }
                 if (pkt->depthLoadOp == LoadAction::Clear) {
                    m_ctx.glClearDepth(pkt->clearDepth);
                    mask |= GL_DEPTH_BUFFER_BIT;
                    m_ctx.glDepthMask(GL_TRUE); // Ensure we can write to depth buffer
                 }
                 if (mask != 0) {
                    m_ctx.glClear(mask);
                 }

                 // 4. Apply Initial Scissor State for Drawing
                 if (pkt->scW >= 0 && pkt->scH >= 0) {
                    m_ctx.glEnable(GL_SCISSOR_TEST);
                    m_ctx.glScissor(pkt->scX, pkt->scY, pkt->scW, pkt->scH);
                 } else {
                    m_ctx.glDisable(GL_SCISSOR_TEST);
                 }

                 // 5. Reset Viewport State (if specified)
                 if (pkt->vpW >= 0 && pkt->vpH >= 0) {
                    m_ctx.glViewport(pkt->vpX, pkt->vpY, pkt->vpW, pkt->vpH);
                 }
                 break;
            }

            case CommandType::EndPass: {
                 // Store actions would go here (e.g., resolving MSAA, flushing to memory)
                 break;
            }
            
            case CommandType::SetScissor: {
                const auto* pkt = reinterpret_cast<const PacketSetScissor*>(ptr);
                if (pkt->w >= 0 && pkt->h >= 0) {
                    m_ctx.glEnable(GL_SCISSOR_TEST);
                    m_ctx.glScissor(pkt->x, pkt->y, pkt->w, pkt->h);
                } else {
                    m_ctx.glDisable(GL_SCISSOR_TEST);
                }
                break;
            }
                 
            case CommandType::NoOp:
                break;
        }
        ptr += header->size;
    }
}

void SoftDevice::Present() {
    // End of frame, reset allocator and tiler
    m_frameMem.Reset();
    m_tiler.Reset();
}

}