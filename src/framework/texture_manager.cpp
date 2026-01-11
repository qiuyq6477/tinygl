#include <framework/texture_manager.h>
#include <iostream>

namespace framework {

std::unordered_map<std::string, std::weak_ptr<Texture>> TextureManager::m_cache;
std::mutex TextureManager::m_mutex;

std::shared_ptr<Texture> TextureManager::Load(SoftRenderContext& ctx, const std::string& path) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // 1. Check if texture exists and is valid
    auto it = m_cache.find(path);
    if (it != m_cache.end()) {
        std::shared_ptr<Texture> tex = it->second.lock();
        if (tex) {
            // Cache hit
            return tex;
        } else {
            // Expired pointer, remove from cache
            m_cache.erase(it);
        }
    }

    // 2. Not in cache or expired, load new
    // Note: We use make_shared with a constructor, but Texture ctor might assume access.
    // Since Texture ctor is public, make_shared is fine.
    // However, Texture needs `directory` argument usually, but here we pass full path as `path`
    // and empty directory, or handle it before calling Load.
    // The previous Model code constructed path = dir + '/' + filename.
    // We assume `path` passed here is the full resolvable path.
    
    auto tex = std::make_shared<Texture>(ctx, path, "");
    
    // Only cache if load was successful (id != 0)
    if (tex->id != 0) {
        m_cache[path] = tex;
    }

    return tex;
}

void TextureManager::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cache.clear();
}

} // namespace tinygl
