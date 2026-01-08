
#pragma once
#include <tinygl/rhi/device.h>
#include <tinygl/core/tinygl.h>
#include <tinygl/rhi/soft_pipeline.h>
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace tinygl::rhi {

class TINYGL_API SoftDevice : public IGraphicsDevice {
public:
    explicit SoftDevice(SoftRenderContext& ctx);
    ~SoftDevice() override;

    BufferHandle CreateBuffer(const BufferDesc& desc) override;
    void DestroyBuffer(BufferHandle handle) override;
    void UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) override;

    TextureHandle CreateTexture(const void* pixelData, int width, int height, int channels) override;
    
    // Create a handle from an existing GL texture ID.
    // The device will NOT take ownership of this texture (will not delete it).
    TextureHandle CreateTextureFromNative(GLuint glTextureId);
    
    void DestroyTexture(TextureHandle handle) override;

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override;
    void DestroyPipeline(PipelineHandle handle) override;

    void Submit(const RenderCommand* commands, size_t commandCount, const uint8_t* payload, size_t payloadSize) override;
    void Present() override;

    // --- Shader Registration System ---
    // Since SoftRender uses C++ templates for shaders, we need a way to link a runtime Handle 
    // to a compile-time type instantiation.
    
    using PipelineFactory = std::function<std::unique_ptr<ISoftPipeline>(const PipelineDesc&)>;
    
    // Returns a new handle that represents the Shader Type T
    ShaderHandle CreateShaderHandle(); 
    
    void RegisterShaderFactory(ShaderHandle handle, PipelineFactory factory);

private:
    SoftRenderContext& m_ctx;

    // --- Resource Storage ---
    // Using maps for simplicity in Phase 1. 
    // Real implementation should use handle-maps or packed arrays.
    
    struct BufferRes { GLuint glId; BufferType type; };
    struct TextureRes { GLuint glId; bool owned = true; };

    uint32_t m_nextBufferId = 1;
    std::map<uint32_t, BufferRes> m_buffers;

    uint32_t m_nextTextureId = 1;
    std::map<uint32_t, TextureRes> m_textures;

    uint32_t m_nextPipelineId = 1;
    std::map<uint32_t, std::unique_ptr<ISoftPipeline>> m_pipelines;

    uint32_t m_nextShaderId = 1;
    std::map<uint32_t, PipelineFactory> m_shaderFactories;

    // --- Runtime Execution State ---
    // This state is reset at the beginning of Submit
    
    ISoftPipeline* m_currentPipeline = nullptr;
    uint32_t m_currentVertexBufferOffset = 0;
    
    // Uniform Storage
    // We simply keep a linear buffer that mimics the layout of slots.
    // For simplicity: Slot 0 = bytes [0..255], Slot 1 = [256..511], etc.
    static constexpr size_t MAX_UNIFORM_SIZE = 1024; 
    std::vector<uint8_t> m_uniformData;
};

// Helper to register a shader type
template <typename ShaderT>
ShaderHandle RegisterShader(SoftDevice& device) {
    ShaderHandle handle = device.CreateShaderHandle();
    device.RegisterShaderFactory(handle, [](const PipelineDesc& desc) {
        return std::make_unique<SoftPipeline<ShaderT>>(desc);
    });
    return handle;
}

}
