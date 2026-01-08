#include <tinygl/rhi/soft_device.h>
#include <tinygl/base/log.h>
#include <cstring>

namespace tinygl::rhi {

SoftDevice::SoftDevice(SoftRenderContext& ctx) : m_ctx(ctx) {
    m_uniformData.resize(MAX_UNIFORM_SIZE);
}

SoftDevice::~SoftDevice() {
    for (auto& [id, res] : m_buffers) m_ctx.glDeleteBuffers(1, &res.glId);
    for (auto& [id, res] : m_textures) m_ctx.glDeleteTextures(1, &res.glId);
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
    
    uint32_t id = m_nextBufferId++;
    m_buffers[id] = res;
    return {id};
}

void SoftDevice::DestroyBuffer(BufferHandle handle) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end()) {
        m_ctx.glDeleteBuffers(1, &it->second.glId);
        m_buffers.erase(it);
    }
}

void SoftDevice::UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) {
    auto it = m_buffers.find(handle.id);
    if (it != m_buffers.end()) {
        GLenum target = (it->second.type == BufferType::IndexBuffer) ? GL_ELEMENT_ARRAY_BUFFER : GL_ARRAY_BUFFER;
        m_ctx.glBindBuffer(target, it->second.glId);
        // SoftRenderContext currently doesn't expose glBufferSubData, so we re-upload or need to add it.
        // For now, assume full re-upload or implement SubData in core.
        // Fallback: full upload if offset is 0
        if (offset == 0) {
            m_ctx.glBufferData(target, size, data, GL_STATIC_DRAW);
        } else {
            // TODO: Add glBufferSubData to SoftRenderContext
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
    
    uint32_t id = m_nextTextureId++;
    m_textures[id] = res;
    return {id};
}

TextureHandle SoftDevice::CreateTextureFromNative(GLuint glTextureId) {
    TextureRes res;
    res.glId = glTextureId;
    res.owned = false; // External ownership
    
    uint32_t id = m_nextTextureId++;
    m_textures[id] = res;
    return {id};
}

void SoftDevice::DestroyTexture(TextureHandle handle) {
    auto it = m_textures.find(handle.id);
    if (it != m_textures.end()) {
        if (it->second.owned) {
            m_ctx.glDeleteTextures(1, &it->second.glId);
        }
        m_textures.erase(it);
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
    uint32_t id = m_nextPipelineId++;
    m_pipelines[id] = std::move(pipeline);
    return {id};
}

void SoftDevice::DestroyPipeline(PipelineHandle handle) {
    m_pipelines.erase(handle.id);
}

// --- Execution ---

void SoftDevice::Submit(const RenderCommand* commands, size_t commandCount, const uint8_t* payload, size_t payloadSize) {
    m_currentPipeline = nullptr;
    
    for (size_t i = 0; i < commandCount; ++i) {
        const auto& cmd = commands[i];
        
        switch (cmd.type) {
            case CommandType::SetPipeline: {
                auto it = m_pipelines.find(cmd.pipeline.handle.id);
                if (it != m_pipelines.end()) {
                    m_currentPipeline = it->second.get();
                }
                break;
            }
            
            case CommandType::SetViewport: {
                m_ctx.glViewport(cmd.viewport.x, cmd.viewport.y, cmd.viewport.w, cmd.viewport.h);
                break;
            }
            
            case CommandType::SetVertexBuffer: {
                auto it = m_buffers.find(cmd.buffer.handle.id);
                if (it != m_buffers.end()) {
                    m_ctx.glBindBuffer(GL_ARRAY_BUFFER, it->second.glId);
                    // Store offset, but don't set attrib pointers yet.
                    // This is done by Pipeline::Draw using the InputLayout.
                    m_currentVertexBufferOffset = cmd.buffer.offset;
                }
                break;
            }
            
            case CommandType::SetIndexBuffer: {
                auto it = m_buffers.find(cmd.buffer.handle.id);
                if (it != m_buffers.end()) {
                    m_ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, it->second.glId);
                }
                break;
            }

            case CommandType::SetTexture: {
                auto it = m_textures.find(cmd.texture.handle.id);
                if (it != m_textures.end()) {
                    m_ctx.glActiveTexture(GL_TEXTURE0 + cmd.texture.slot);
                    m_ctx.glBindTexture(GL_TEXTURE_2D, it->second.glId);
                }
                break;
            }

            case CommandType::UpdateUniform: {
                if (cmd.uniform.dataOffset + cmd.uniform.size <= payloadSize) {
                    // Mapping Slot -> Linear Offset
                    // Slot 0: 0
                    // Slot 1: 256
                    // Slot 2: 512
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
    // SoftRenderContext::SwapBuffers or similar is handled by Application loop usually.
    // But we can add a hook here.
}

}
