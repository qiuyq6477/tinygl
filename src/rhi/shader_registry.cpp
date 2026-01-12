#include <rhi/shader_registry.h>

namespace rhi {

ShaderRegistry& ShaderRegistry::GetInstance() {
    static ShaderRegistry instance;
    return instance;
}

ShaderRegistry::ShaderRegistry() {
    // Reserve ID 0 as invalid
    m_entries.emplace_back(); 
}

ShaderHandle ShaderRegistry::Register(const std::string& name, const ShaderDesc& desc) {
    if (auto it = m_nameToId.find(name); it != m_nameToId.end()) {
        // Warning: Re-registering shader? For now just return existing handle.
        return { it->second };
    }
    
    uint32_t id = static_cast<uint32_t>(m_entries.size());
    m_entries.push_back({name, desc});
    m_nameToId[name] = id;
    return {id};
}

ShaderHandle ShaderRegistry::GetShader(const std::string& name) const {
    auto it = m_nameToId.find(name);
    if (it != m_nameToId.end()) {
        return { it->second };
    }
    return {0}; // Invalid
}

const ShaderDesc* ShaderRegistry::GetDesc(ShaderHandle handle) const {
    if (handle.id > 0 && handle.id < m_entries.size()) {
        return &m_entries[handle.id].desc;
    }
    return nullptr;
}

bool ShaderRegistry::IsRegistered(const std::string& name) const {
    return m_nameToId.find(name) != m_nameToId.end();
}

void ShaderRegistry::Reset() {
    m_entries.clear();
    m_nameToId.clear();
    m_entries.emplace_back(); // Reserve ID 0
}

}
