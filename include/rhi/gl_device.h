#pragma once
#include <rhi/device.h>
#include <rhi/command_buffer.h>
#include <SDL2/SDL.h>
#include <tinygl/core/gl_defs.h>

namespace rhi {

class TINYGL_API GLDevice : public IGraphicsDevice {
public:
    explicit GLDevice();
    ~GLDevice() override;

    BufferHandle CreateBuffer(const BufferDesc& desc) override;
    void DestroyBuffer(BufferHandle handle) override;
    void UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) override;

    TextureHandle CreateTexture(const void* pixelData, int width, int height, int channels) override;
    void DestroyTexture(TextureHandle handle) override;

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override;
    void DestroyPipeline(PipelineHandle handle) override;

    void Submit(const CommandBuffer& buffer) override;
    void Present() override;
};

}