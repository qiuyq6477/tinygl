#pragma once
#include <rhi/types.h>
#include <rhi/commands.h>

namespace rhi {

class CommandBuffer; // Forward declaration

/**
 * @brief Abstract Render Hardware Interface (RHI).
 * Provides a unified interface for creating resources and submitting draw commands.
 */
class IGraphicsDevice {
public:
    virtual ~IGraphicsDevice() = default;

    // --- Resource Management ---
    
    virtual BufferHandle CreateBuffer(const BufferDesc& desc) = 0;
    virtual void DestroyBuffer(BufferHandle handle) = 0;
    virtual void UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset = 0) = 0;

    virtual TextureHandle CreateTexture(const void* pixelData, int width, int height, int channels) = 0;
    virtual void DestroyTexture(TextureHandle handle) = 0;
    
    virtual PipelineHandle CreatePipeline(const PipelineDesc& desc) = 0;
    virtual void DestroyPipeline(PipelineHandle handle) = 0;

    // --- Execution ---

    /**
     * @brief Submits a command buffer for execution.
     * 
     * @param buffer The command buffer containing linear command stream.
     */
    virtual void Submit(const CommandBuffer& buffer) = 0;

    // --- Frame Control ---
    virtual void Present() = 0;
};

}