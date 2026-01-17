#pragma once
#include <rhi/device.h>
#include <rhi/command_buffer.h>
#include <tinygl/tinygl.h>
#include <rhi/soft_pipeline.h>
#include <tinygl/core/linear_allocator.h>
#include <tinygl/core/tiler.h>
#include <tinygl/core/job_system.h>
#include <vector>
#include <memory>
#include <map>
#include <functional>

namespace rhi {

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

    void Submit(const CommandBuffer& buffer) override;
    void Present() override;

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

    // --- Runtime Execution State ---
    
    uint32_t m_activePipelineId = 0;
    
    // Binding Slots
    static constexpr int MAX_BINDINGS = 8;
    struct BindingState {
        uint32_t bufferId = 0;
        uint32_t offset = 0;
        uint32_t stride = 0;
    } m_bindings[MAX_BINDINGS];

    uint32_t m_activeIBOId = 0;
    uint32_t m_activeTextureIds[8] = {0}; // Track basic slots

    ISoftPipeline* m_currentPipeline = nullptr;
    
    // Uniform Storage
    // Use a flat buffer to accumulate uniform updates.
    // For now, let's keep it simple and just write directly to this buffer
    // when we see UpdateUniform, and pass pointers into it during Draw.
    static constexpr size_t MAX_UNIFORM_SIZE = 1024 * 64; // Increase to 64KB
    std::vector<uint8_t> m_uniformData;

    // --- Phase 1: Tile-Based Rendering Infrastructure ---
    tinygl::LinearAllocator m_frameMem;
    tinygl::TileBinningSystem m_tiler;
    tinygl::JobSystem m_jobSystem;
};

}