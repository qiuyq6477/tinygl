#pragma once
#include <tinygl/rhi/types.h>
#include <tinygl/rhi/commands.h>

namespace tinygl::rhi {

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
    // Update buffer data immediately (useful for staging/initialization)
    virtual void UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset = 0) = 0;

    virtual TextureHandle CreateTexture(const void* pixelData, int width, int height, int channels) = 0;
    virtual void DestroyTexture(TextureHandle handle) = 0;
    
    // Pipeline Creation: Requires a shader and state configuration
    virtual PipelineHandle CreatePipeline(const PipelineDesc& desc) = 0;
    virtual void DestroyPipeline(PipelineHandle handle) = 0;

    // --- Execution ---

    /**
     * @brief Submits a list of commands for execution.
     * 
     * @param commands Pointer to the array of RenderCommand structures.
     * @param commandCount Number of commands in the array.
     * @param payload Pointer to the transient data buffer (payload) referenced by commands (e.g., UpdateUniform).
     * @param payloadSize Size of the payload buffer in bytes.
     */
    virtual void Submit(const RenderCommand* commands, size_t commandCount, const uint8_t* payload, size_t payloadSize) = 0;

    // --- Frame Control ---
    // Signal the end of a frame, swap buffers, etc.
    virtual void Present() = 0;
};

}
