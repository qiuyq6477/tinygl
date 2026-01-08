
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

    // --- Resource Storage (Optimized Scheme A: Packed Arrays) ---
    template <typename T>
    struct ResourceSlot {
        T resource;
        bool active = false;
    };

    template <typename T>
    struct ResourcePool {
        std::vector<ResourceSlot<T>> pool;
        std::vector<uint32_t> freeIndices;

        ResourcePool() {
            // Reserve index 0 as invalid/null
            pool.emplace_back(); 
        }

        uint32_t Allocate(T&& res) {
            if (!freeIndices.empty()) {
                uint32_t idx = freeIndices.back();
                freeIndices.pop_back();
                pool[idx] = { std::move(res), true };
                return idx;
            }
            pool.push_back({ std::move(res), true });
            return static_cast<uint32_t>(pool.size() - 1);
        }

        void Release(uint32_t idx) {
            if (idx > 0 && idx < pool.size() && pool[idx].active) {
                pool[idx].active = false;
                // Optional: destruct resource immediately if T holds RAII handles
                pool[idx].resource = T(); 
                freeIndices.push_back(idx);
            }
        }

        T* Get(uint32_t idx) {
            if (idx > 0 && idx < pool.size() && pool[idx].active) {
                return &pool[idx].resource;
            }
            return nullptr;
        }
    };

    struct BufferRes { GLuint glId; BufferType type; };
    struct TextureRes { GLuint glId; bool owned = true; };
    
    // Pools
    ResourcePool<BufferRes> m_buffers;
    ResourcePool<TextureRes> m_textures;
    ResourcePool<std::unique_ptr<ISoftPipeline>> m_pipelines;

    // Factories (Low frequency, can map or vector)
    uint32_t m_nextShaderId = 1;
    std::map<uint32_t, PipelineFactory> m_shaderFactories;

    // --- Runtime Execution State (Optimized Scheme B: State Deduplication) ---
    // Tracks currently bound handles to avoid redundant context calls
    
    uint32_t m_activePipelineId = 0;
    uint32_t m_activeVBOId = 0;
    uint32_t m_activeIBOId = 0;
    uint32_t m_activeTextureIds[8] = {0}; // Track basic slots

    ISoftPipeline* m_currentPipeline = nullptr;
    uint32_t m_currentVertexBufferOffset = 0;
    
    // Uniform Storage
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
