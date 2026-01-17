#pragma once

#pragma once

#include "asset_handle.h"
#include <rhi/device.h>
#include <framework/resources.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

namespace framework {

class TINYGL_API AssetManager {
public:
    static AssetManager& Get();

    void Init(rhi::IGraphicsDevice* device);
    void Shutdown();

    // Generic Load Interface
    template<typename T>
    AssetHandle<T> Load(const std::string& path);

    template<typename T>
    void AddRef(AssetHandle<T> handle);

    template<typename T>
    void Release(AssetHandle<T> handle);
    
    // Resource Access API
    TextureResource* GetTexture(AssetHandle<TextureResource> handle);
    MeshResource*    GetMesh(AssetHandle<MeshResource> handle);
    MaterialResource* GetMaterial(AssetHandle<MaterialResource> handle);
    Prefab*          GetPrefab(AssetHandle<Prefab> handle);

    // Helpers
    rhi::TextureHandle GetRHI(AssetHandle<TextureResource> handle);

    // Garbage Collection
    void GarbageCollect();

private:
    AssetManager() = default;
    ~AssetManager() = default;

    rhi::IGraphicsDevice* m_device = nullptr;

    // Internal Structures for Pools
    
    template<typename T>
    struct AssetRecord {
        std::string path; // or name
        T asset;
        uint32_t refCount = 0;
        uint32_t generation = 1;
        bool loaded = false;
    };

    std::vector<AssetRecord<TextureResource>> m_texturePool;
    std::vector<uint32_t> m_freeTextureIndices;

    std::vector<AssetRecord<MeshResource>> m_meshPool;
    std::vector<uint32_t> m_freeMeshIndices;

    std::vector<AssetRecord<MaterialResource>> m_materialPool;
    std::vector<uint32_t> m_freeMaterialIndices;

    std::vector<AssetRecord<Prefab>> m_prefabPool;
    std::vector<uint32_t> m_freePrefabIndices;

    // --- Lookups ---
    std::unordered_map<std::string, uint32_t> m_textureLookup;
    std::unordered_map<std::string, uint32_t> m_prefabLookup;
    // Meshes and Materials are typically sub-assets loaded via Prefab, 
    // so they might not have a direct file path lookup unless externally referenced.

    // --- Internal Allocators ---
    uint32_t AllocTextureSlot();
    uint32_t AllocMeshSlot();
    uint32_t AllocMaterialSlot();
    uint32_t AllocPrefabSlot();

    // --- Loading Logic ---
    void LoadTextureInternal(uint32_t index, const std::string& path);
    void LoadPrefabInternal(uint32_t index, const std::string& path);

    // Binary Loaders
    bool LoadTextureBin(uint32_t index, const std::filesystem::path& path);
    bool LoadPrefabBin(uint32_t index, const std::filesystem::path& path);
};

// Template Specializations
template<>
TINYGL_API AssetHandle<TextureResource> AssetManager::Load(const std::string& path);

template<>
TINYGL_API AssetHandle<Prefab> AssetManager::Load(const std::string& path);

template<>
TINYGL_API void AssetManager::AddRef(AssetHandle<TextureResource> handle);

template<>
TINYGL_API void AssetManager::AddRef(AssetHandle<MeshResource> handle);

template<>
TINYGL_API void AssetManager::AddRef(AssetHandle<MaterialResource> handle);

template<>
TINYGL_API void AssetManager::AddRef(AssetHandle<Prefab> handle);

template<>
TINYGL_API void AssetManager::Release(AssetHandle<TextureResource> handle);

template<>
TINYGL_API void AssetManager::Release(AssetHandle<MeshResource> handle);

template<>
TINYGL_API void AssetManager::Release(AssetHandle<MaterialResource> handle);

template<>
TINYGL_API void AssetManager::Release(AssetHandle<Prefab> handle);

} // namespace framework
