#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <framework/texture.h>

namespace framework {

class TINYGL_API TextureManager {
public:
    // Thread-safe texture loading/retrieval
    // Uses weak_ptr to allow automatic unloading when no models reference the texture
    static std::shared_ptr<Texture> Load(SoftRenderContext& ctx, const std::string& path);

    // Optional: Manually clear the cache (usually not needed with weak_ptr)
    static void Clear();

private:
    static std::unordered_map<std::string, std::weak_ptr<Texture>> m_cache;
    static std::mutex m_mutex;
};

} // namespace tinygl
