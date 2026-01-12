#pragma once
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <vector>
#include <rhi/soft_pipeline.h>
#include <rhi/types.h>
#include <tinygl/tinygl.h>

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

class TINYGL_API ShaderRegistry {
public:
    static ShaderRegistry& GetInstance();

    // Register a shader with a unique name
    // Returns the handle for the registered shader
    ShaderHandle Register(const std::string& name, const ShaderDesc& desc);

    // Get handle by name (for loading from file)
    ShaderHandle GetShader(const std::string& name) const;

    // Get description by handle (for backend)
    const ShaderDesc* GetDesc(ShaderHandle handle) const;
    
    // Check if registered
    bool IsRegistered(const std::string& name) const;

    // Reset registry (useful for tests)
    void Reset();

    // Helper: Register a SoftRender template shader
    template <typename ShaderT>
    static ShaderHandle RegisterShader(const std::string& name) {
        ShaderDesc desc;
        desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
            return std::make_unique<SoftPipeline<ShaderT>>(ctx, pDesc);
        };
        return GetInstance().Register(name, desc);
    }

    static ShaderHandle RegisterShader(const std::string& name, const std::string& vs, const std::string& fs) {
        ShaderDesc desc;
        desc.glsl.vertex = vs;
        desc.glsl.fragment = fs;
        return GetInstance().Register(name, desc);
    }

private:
    ShaderRegistry();
    ShaderRegistry(const ShaderRegistry&) = delete;
    ShaderRegistry& operator=(const ShaderRegistry&) = delete;

    struct Entry {
        std::string name;
        ShaderDesc desc;
    };
    std::vector<Entry> m_entries; // ID = index
    std::unordered_map<std::string, uint32_t> m_nameToId;
};

}