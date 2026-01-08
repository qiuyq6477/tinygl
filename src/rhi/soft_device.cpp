#include <tinygl/rhi/soft_device.h>
#include <tinygl/base/log.h>
#include <cstring>

namespace tinygl::rhi {

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
            // TODO: Add glBufferSubData
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
    
    auto pipeline = factoryIt->second(desc);
    uint32_t id = m_pipelines.Allocate(std::move(pipeline));
    return {id};
}

void SoftDevice::DestroyPipeline(PipelineHandle handle) {
    m_pipelines.Release(handle.id);
}

// --- Execution ---

void SoftDevice::Submit(const RenderCommand* commands, size_t commandCount, const uint8_t* payload, size_t payloadSize) {
    // Reset runtime state trackers at the beginning of a command batch
    // This ensures we don't assume state persists from previous Submit calls 
    // (unless we want to implement robust cross-frame state tracking).
    m_activePipelineId = 0;
    m_activeVBOId = 0;
    m_activeIBOId = 0;
    std::memset(m_activeTextureIds, 0, sizeof(m_activeTextureIds));

    m_currentPipeline = nullptr;
    
    for (size_t i = 0; i < commandCount; ++i) {
        const auto& cmd = commands[i];
        
        switch (cmd.type) {
            case CommandType::SetPipeline: {
                if (cmd.pipeline.handle.id != m_activePipelineId) {
                    auto* pipelinePtr = m_pipelines.Get(cmd.pipeline.handle.id);
                    if (pipelinePtr && *pipelinePtr) {
                        m_currentPipeline = pipelinePtr->get();
                        m_activePipelineId = cmd.pipeline.handle.id;
                    } else {
                        m_currentPipeline = nullptr;
                        m_activePipelineId = 0;
                    }
                }
                break;
            }
            
            case CommandType::SetViewport: {
                // Viewport is rarely redundant, just set it
                m_ctx.glViewport(cmd.viewport.x, cmd.viewport.y, cmd.viewport.w, cmd.viewport.h);
                break;
            }
            
            case CommandType::SetVertexBuffer: {
                // Check redundancy for BindBuffer
                if (cmd.buffer.handle.id != m_activeVBOId) {
                    BufferRes* res = m_buffers.Get(cmd.buffer.handle.id);
                    if (res) {
                        m_ctx.glBindBuffer(GL_ARRAY_BUFFER, res->glId);
                        m_activeVBOId = cmd.buffer.handle.id;
                    } else {
                        m_activeVBOId = 0;
                    }
                }
                // Offset can change even if buffer is same
                m_currentVertexBufferOffset = cmd.buffer.offset;
                break;
            }
            
            case CommandType::SetIndexBuffer: {
                if (cmd.buffer.handle.id != m_activeIBOId) {
                    BufferRes* res = m_buffers.Get(cmd.buffer.handle.id);
                    if (res) {
                        m_ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, res->glId);
                        m_activeIBOId = cmd.buffer.handle.id;
                    } else {
                        m_activeIBOId = 0;
                    }
                }
                break;
            }

            case CommandType::SetTexture: {
                uint8_t slot = cmd.texture.slot;
                if (slot < 8) { // Bound check for tracker
                    if (m_activeTextureIds[slot] != cmd.texture.handle.id) {
                        TextureRes* res = m_textures.Get(cmd.texture.handle.id);
                        if (res) {
                            m_ctx.glActiveTexture(GL_TEXTURE0 + slot);
                            m_ctx.glBindTexture(GL_TEXTURE_2D, res->glId);
                            m_activeTextureIds[slot] = cmd.texture.handle.id;
                        } else {
                             m_activeTextureIds[slot] = 0;
                        }
                    }
                }
                break;
            }

            case CommandType::UpdateUniform: {
                if (cmd.uniform.dataOffset + cmd.uniform.size <= payloadSize) {
                    // Mapping Slot -> Linear Offset
                    size_t dstOffset = cmd.uniform.slot * 256; 
                    if (dstOffset + cmd.uniform.size <= m_uniformData.size()) {
                        memcpy(m_uniformData.data() + dstOffset, 
                               payload + cmd.uniform.dataOffset, 
                               cmd.uniform.size);
                    }
                }
                break;
            }

            case CommandType::Draw: {
                if (m_currentPipeline) {
                    m_currentPipeline->Draw(m_ctx, m_uniformData, cmd, m_currentVertexBufferOffset);
                }
                break;
            }

            case CommandType::DrawIndexed: {
                if (m_currentPipeline) {
                    m_currentPipeline->DrawIndexed(m_ctx, m_uniformData, cmd, m_currentVertexBufferOffset);
                }
                break;
            }
            
            case CommandType::SetScissor:
                 // TODO
                 break;
        }
    }
}

void SoftDevice::Present() {
    // Present hook
}

}