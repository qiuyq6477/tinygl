#pragma once
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <rhi/soft_pipeline.h>
#include <rhi/types.h>

namespace rhi {

// Unified Shader Definition
struct ShaderDesc {
    // 1. Backend: SoftRender (C++ Factory)
    using SoftFactory = std::function<std::unique_ptr<ISoftPipeline>(SoftRenderContext&, const PipelineDesc&)>;
    SoftFactory softFactory;

    // 2. Backend: OpenGL (GLSL Source)
    struct GLSL {
        std::string vertex;
        std::string fragment;
    } glsl;
};

// Global Registry
class ShaderRegistry {
public:
    static ShaderRegistry& Get() {
        static ShaderRegistry instance;
        return instance;
    }

    void Register(uint32_t id, const ShaderDesc& desc) {
        m_shaders[id] = desc;
    }

    const ShaderDesc* GetDesc(uint32_t id) const {
        auto it = m_shaders.find(id);
        if (it != m_shaders.end()) return &it->second;
        return nullptr;
    }

private:
    std::unordered_map<uint32_t, ShaderDesc> m_shaders;
};

}
