#include <framework/asset_manager.h>
#include <framework/asset_formats.h>
#include <tinygl/base/log.h>
#include <fstream>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace framework {

AssetManager& AssetManager::Get() {
    static AssetManager instance;
    return instance;
}

void AssetManager::Init(rhi::IGraphicsDevice* device) {
    m_device = device;
    // Dummy slot 0
    m_texturePool.push_back({});
    m_prefabPool.push_back({});
    m_meshPool.push_back({});
    m_materialPool.push_back({});
}

void AssetManager::Shutdown() {
    // Clear Lookups
    m_textureLookup.clear();
    m_prefabLookup.clear();

    // Clear Pools
    m_texturePool.clear();
    m_meshPool.clear();
    m_materialPool.clear();
    m_prefabPool.clear();

    // Clear Free Lists
    m_freeTextureIndices.clear();
    m_freeMeshIndices.clear();
    m_freeMaterialIndices.clear();
    m_freePrefabIndices.clear();

    m_device = nullptr;
}

// =================================================================================================
// Allocators
// =================================================================================================

uint32_t AssetManager::AllocTextureSlot() {
    if (!m_freeTextureIndices.empty()) {
        uint32_t idx = m_freeTextureIndices.back();
        m_freeTextureIndices.pop_back();
        m_texturePool[idx].generation++;
        return idx;
    }
    m_texturePool.push_back({});
    m_texturePool.back().generation = 1;
    return (uint32_t)(m_texturePool.size() - 1);
}

uint32_t AssetManager::AllocMeshSlot() {
    if (!m_freeMeshIndices.empty()) {
        uint32_t idx = m_freeMeshIndices.back();
        m_freeMeshIndices.pop_back();
        m_meshPool[idx].generation++;
        return idx;
    }
    m_meshPool.push_back({});
    m_meshPool.back().generation = 1;
    return (uint32_t)(m_meshPool.size() - 1);
}

uint32_t AssetManager::AllocMaterialSlot() {
    if (!m_freeMaterialIndices.empty()) {
        uint32_t idx = m_freeMaterialIndices.back();
        m_freeMaterialIndices.pop_back();
        m_materialPool[idx].generation++;
        return idx;
    }
    m_materialPool.push_back({});
    m_materialPool.back().generation = 1;
    return (uint32_t)(m_materialPool.size() - 1);
}

uint32_t AssetManager::AllocPrefabSlot() {
    if (!m_freePrefabIndices.empty()) {
        uint32_t idx = m_freePrefabIndices.back();
        m_freePrefabIndices.pop_back();
        m_prefabPool[idx].generation++;
        return idx;
    }
    m_prefabPool.push_back({});
    m_prefabPool.back().generation = 1;
    return (uint32_t)(m_prefabPool.size() - 1);
}

// =================================================================================================
// AddRef Implementations
// =================================================================================================

template<>
void AssetManager::AddRef(AssetHandle<TextureResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_texturePool.size() && m_texturePool[idx].generation == handle.GetGeneration()) {
        m_texturePool[idx].refCount++;
    }
}

template<>
void AssetManager::AddRef(AssetHandle<MeshResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_meshPool.size() && m_meshPool[idx].generation == handle.GetGeneration()) {
        m_meshPool[idx].refCount++;
    }
}

template<>
void AssetManager::AddRef(AssetHandle<MaterialResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_materialPool.size() && m_materialPool[idx].generation == handle.GetGeneration()) {
        m_materialPool[idx].refCount++;
    }
}

template<>
void AssetManager::AddRef(AssetHandle<Prefab> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_prefabPool.size() && m_prefabPool[idx].generation == handle.GetGeneration()) {
        m_prefabPool[idx].refCount++;
    }
}

// =================================================================================================
// Release Implementations
// =================================================================================================

template<>
void AssetManager::Release(AssetHandle<TextureResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_texturePool.size() && m_texturePool[idx].generation == handle.GetGeneration()) {
        if (m_texturePool[idx].refCount > 0) m_texturePool[idx].refCount--;
    }
}

template<>
void AssetManager::Release(AssetHandle<MeshResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_meshPool.size() && m_meshPool[idx].generation == handle.GetGeneration()) {
        if (m_meshPool[idx].refCount > 0) m_meshPool[idx].refCount--;
    }
}

template<>
void AssetManager::Release(AssetHandle<MaterialResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_materialPool.size() && m_materialPool[idx].generation == handle.GetGeneration()) {
        if (m_materialPool[idx].refCount > 0) m_materialPool[idx].refCount--;
    }
}

template<>
void AssetManager::Release(AssetHandle<Prefab> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx < m_prefabPool.size() && m_prefabPool[idx].generation == handle.GetGeneration()) {
        if (m_prefabPool[idx].refCount > 0) m_prefabPool[idx].refCount--;
    }
}

// =================================================================================================
// Texture Management
// =================================================================================================

template<>
AssetHandle<TextureResource> AssetManager::Load(const std::string& path) {
    if (auto it = m_textureLookup.find(path); it != m_textureLookup.end()) {
        uint32_t idx = it->second;
        m_texturePool[idx].refCount++;
        uint32_t id = (m_texturePool[idx].generation << 20) | idx;
        return AssetHandle<TextureResource>(id);
    }

    uint32_t idx = AllocTextureSlot();
    m_textureLookup[path] = idx;
    m_texturePool[idx].path = path;
    m_texturePool[idx].refCount = 1;
    m_texturePool[idx].loaded = false;

    LoadTextureInternal(idx, path);

    uint32_t id = (m_texturePool[idx].generation << 20) | idx;
    return AssetHandle<TextureResource>(id);
}

TextureResource* AssetManager::GetTexture(AssetHandle<TextureResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx >= m_texturePool.size()) return nullptr;
    if (m_texturePool[idx].generation != handle.GetGeneration()) return nullptr;
    return &m_texturePool[idx].asset;
}

rhi::TextureHandle AssetManager::GetRHI(AssetHandle<TextureResource> handle) {
    auto* res = GetTexture(handle);
    return res ? res->rhiHandle : rhi::TextureHandle{0};
}

void AssetManager::LoadTextureInternal(uint32_t index, const std::string& path) {
    fs::path srcPath(path);
    fs::path binPath = srcPath;
    binPath.replace_extension(".ttex");

    // Strict Offline Check
    if (!fs::exists(binPath)) {
        LOG_ERROR("Asset not found (Need Cook): " + binPath.string());
        // Fallback: Create a 1x1 magenta texture
        static rhi::TextureHandle errorTex = {0};
        if (errorTex.id == 0) {
             uint32_t magenta = 0xFF00FFFF;
             errorTex = m_device->CreateTexture((unsigned char*)&magenta, 1, 1, 4);
        }
        m_texturePool[index].asset.rhiHandle = errorTex;
        m_texturePool[index].asset.width = 1;
        m_texturePool[index].asset.height = 1;
        m_texturePool[index].asset.name = "ErrorTexture";
        m_texturePool[index].loaded = true;
        return;
    }

    if (!LoadTextureBin(index, binPath)) {
        LOG_ERROR("Failed to load binary texture: " + binPath.string());
    }
}

bool AssetManager::LoadTextureBin(uint32_t index, const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    AssetHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != MAGIC_TTEX || header.version != ASSET_VERSION) {
        LOG_ERROR("Invalid .ttex format version: " + path.string());
        return false;
    }

    TextureHeader texHeader;
    in.read(reinterpret_cast<char*>(&texHeader), sizeof(texHeader));

    // Calculate remaining size
    size_t dataSize = (size_t)header.dataSize - sizeof(TextureHeader);
    std::vector<unsigned char> data(dataSize);
    in.read(reinterpret_cast<char*>(data.data()), dataSize);

    // Create Texture via RHI
    rhi::TextureHandle handle = m_device->CreateTexture(data.data(), texHeader.width, texHeader.height, 4);
    
    m_texturePool[index].asset.rhiHandle = handle;
    m_texturePool[index].asset.width = texHeader.width;
    m_texturePool[index].asset.height = texHeader.height;
    m_texturePool[index].asset.name = path.filename().string();
    m_texturePool[index].loaded = true;
    
    return true; 
}

// =================================================================================================
// Prefab Management (replaces Model)
// =================================================================================================

template<>
AssetHandle<Prefab> AssetManager::Load(const std::string& path) {
    if (auto it = m_prefabLookup.find(path); it != m_prefabLookup.end()) {
        uint32_t idx = it->second;
        m_prefabPool[idx].refCount++;
        uint32_t id = (m_prefabPool[idx].generation << 20) | idx;
        return AssetHandle<Prefab>(id);
    }

    uint32_t idx = AllocPrefabSlot();
    m_prefabLookup[path] = idx;
    m_prefabPool[idx].path = path;
    m_prefabPool[idx].refCount = 1;
    m_prefabPool[idx].loaded = false;

    LoadPrefabInternal(idx, path);

    uint32_t id = (m_prefabPool[idx].generation << 20) | idx;
    return AssetHandle<Prefab>(id);
}

Prefab* AssetManager::GetPrefab(AssetHandle<Prefab> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx >= m_prefabPool.size()) return nullptr;
    if (m_prefabPool[idx].generation != handle.GetGeneration()) return nullptr;
    return &m_prefabPool[idx].asset;
}

MeshResource* AssetManager::GetMesh(AssetHandle<MeshResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx >= m_meshPool.size()) return nullptr;
    if (m_meshPool[idx].generation != handle.GetGeneration()) return nullptr;
    return &m_meshPool[idx].asset;
}

MaterialResource* AssetManager::GetMaterial(AssetHandle<MaterialResource> handle) {
    uint32_t idx = handle.GetIndex();
    if (idx >= m_materialPool.size()) return nullptr;
    if (m_materialPool[idx].generation != handle.GetGeneration()) return nullptr;
    return &m_materialPool[idx].asset;
}

void AssetManager::LoadPrefabInternal(uint32_t index, const std::string& path) {
    fs::path srcPath(path);
    fs::path binPath = srcPath;
    binPath.replace_extension(".tmodel"); 

    if (!fs::exists(binPath)) {
        LOG_ERROR("Asset not found (Need Cook): " + binPath.string());
        return;
    }
    
    if (!LoadPrefabBin(index, binPath)) {
         LOG_ERROR("Failed to load binary prefab: " + binPath.string());
    }
}

bool AssetManager::LoadPrefabBin(uint32_t index, const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    AssetHeader assetHeader;
    in.read(reinterpret_cast<char*>(&assetHeader), sizeof(assetHeader));
    if (assetHeader.magic != MAGIC_TMODEL || assetHeader.version != ASSET_VERSION) return false;

    ModelHeader modelHeader;
    in.read(reinterpret_cast<char*>(&modelHeader), sizeof(modelHeader));

    Prefab& prefab = m_prefabPool[index].asset;
    prefab.name = path.filename().string();
    std::string directory = path.parent_path().string();

    // 1. Read Materials
    std::vector<AssetHandle<MaterialResource>> materialHandles;
    for(uint32_t i=0; i<modelHeader.materialCount; i++) {
        MaterialHeader matHeader;
        in.read(reinterpret_cast<char*>(&matHeader), sizeof(matHeader));
        
        // Create Resource
        uint32_t matIdx = AllocMaterialSlot();
        MaterialResource& matRes = m_materialPool[matIdx].asset;
        matRes.name = prefab.name + "_mat_" + std::to_string(i);

        // Copy Data
        matRes.data.diffuse = Vec4(matHeader.data.diffuse[0], matHeader.data.diffuse[1], matHeader.data.diffuse[2], matHeader.data.diffuse[3]);
        matRes.data.ambient = Vec4(matHeader.data.ambient[0], matHeader.data.ambient[1], matHeader.data.ambient[2], matHeader.data.ambient[3]);
        matRes.data.specular = Vec4(matHeader.data.specular[0], matHeader.data.specular[1], matHeader.data.specular[2], matHeader.data.specular[3]);
        matRes.data.emissive = Vec4(matHeader.data.emissive[0], matHeader.data.emissive[1], matHeader.data.emissive[2], matHeader.data.emissive[3]);
        matRes.data.shininess = matHeader.data.shininess;
        matRes.data.opacity = matHeader.data.opacity;
        matRes.data.alphaTest = matHeader.data.alphaTest;
        matRes.data.doubleSided = matHeader.data.doubleSided;

        // Read Texture Paths
        for(int t=0; t<6; t++) {
            uint32_t len = matHeader.texturePathLengths[t];
            if(len > 0) {
                std::string texPath(len, '\0');
                in.read(texPath.data(), len);
                
                std::string fullPath = texPath;
                if (!directory.empty()) fullPath = directory + '/' + texPath;

                // Recursive Load
                AssetHandle<TextureResource> hTex = Load<TextureResource>(fullPath);
                matRes.textures[t] = hTex;
                matRes.rhiTextures[t] = GetRHI(hTex);
            }
        }

        m_materialPool[matIdx].loaded = true;
        m_materialPool[matIdx].refCount = 1; // Held by Prefab logic temporarily
        materialHandles.push_back(AssetHandle<MaterialResource>((m_materialPool[matIdx].generation << 20) | matIdx));
    }

    // 2. Read Meshes
    for(uint32_t i=0; i<modelHeader.meshCount; i++) {
        SubMeshHeader smh;
        in.read(reinterpret_cast<char*>(&smh), sizeof(smh));

        // Create Mesh Resource
        uint32_t meshIdx = AllocMeshSlot();
        MeshResource& meshRes = m_meshPool[meshIdx].asset;
        meshRes.name = prefab.name + "_mesh_" + std::to_string(i);
        meshRes.vertexCount = smh.vertexCount;
        meshRes.indexCount = smh.indexCount;
        meshRes.minBounds = Vec4(smh.minBounds[0], smh.minBounds[1], smh.minBounds[2], 1.0f);
        meshRes.maxBounds = Vec4(smh.maxBounds[0], smh.maxBounds[1], smh.maxBounds[2], 1.0f);

        // Read Vertices
        std::vector<VertexPacked> vertices(smh.vertexCount);
        in.read(reinterpret_cast<char*>(vertices.data()), smh.vertexCount * sizeof(VertexPacked));

        // Create VBO
        rhi::BufferDesc vboDesc;
        vboDesc.type = rhi::BufferType::VertexBuffer;
        vboDesc.size = vertices.size() * sizeof(VertexPacked);
        vboDesc.initialData = vertices.data();
        meshRes.vbo = m_device->CreateBuffer(vboDesc);

        // Read Indices
        std::vector<uint32_t> indices(smh.indexCount);
        in.read(reinterpret_cast<char*>(indices.data()), smh.indexCount * sizeof(uint32_t));
        
        // Create EBO
        rhi::BufferDesc eboDesc;
        eboDesc.type = rhi::BufferType::IndexBuffer;
        eboDesc.size = indices.size() * sizeof(uint32_t);
        eboDesc.initialData = indices.data();
        meshRes.ebo = m_device->CreateBuffer(eboDesc);

        m_meshPool[meshIdx].loaded = true;
        m_meshPool[meshIdx].refCount = 1;
        AssetHandle<MeshResource> hMesh((m_meshPool[meshIdx].generation << 20) | meshIdx);

        // Create Node
        PrefabNode node;
        node.name = meshRes.name;
        node.mesh = hMesh;
        if(smh.materialIndex < materialHandles.size()) {
            node.material = materialHandles[smh.materialIndex];
        }
        node.parentIndex = -1;
        prefab.nodes.push_back(node);
    }
    
    m_prefabPool[index].loaded = true;
    return true; 
}

void AssetManager::GarbageCollect() {
    bool changed = true;
    while(changed) {
        changed = false;
        
        // 1. Prefabs
        for(size_t i=0; i<m_prefabPool.size(); ++i) {
            auto& rec = m_prefabPool[i];
            if(rec.loaded && rec.refCount == 0) {
                // Release dependencies
                for(auto& node : rec.asset.nodes) {
                     Release(node.mesh);
                     Release(node.material);
                }
                rec.loaded = false;
                if(!rec.path.empty()) m_prefabLookup.erase(rec.path);
                m_freePrefabIndices.push_back((uint32_t)i);
                rec.generation++; 
                rec.path = "";
                rec.asset.nodes.clear();
                changed = true;
            }
        }

        // 2. Materials
        for(size_t i=0; i<m_materialPool.size(); ++i) {
             auto& rec = m_materialPool[i];
             if(rec.loaded && rec.refCount == 0) {
                 for(int t=0; t<MaterialResource::MAX_TEXTURES; t++) {
                     if(rec.asset.textures[t].IsValid()) {
                         Release(rec.asset.textures[t]);
                     }
                 }
                 rec.loaded = false;
                 // m_materialLookup.erase(rec.path); // If exists
                 m_freeMaterialIndices.push_back((uint32_t)i);
                 rec.generation++;
                 changed = true;
             }
        }

        // 3. Meshes
        for(size_t i=0; i<m_meshPool.size(); ++i) {
            auto& rec = m_meshPool[i];
            if(rec.loaded && rec.refCount == 0) {
                if (m_device) {
                    m_device->DestroyBuffer(rec.asset.vbo);
                    m_device->DestroyBuffer(rec.asset.ebo);
                }
                rec.loaded = false;
                m_freeMeshIndices.push_back((uint32_t)i);
                rec.generation++;
                changed = true;
            }
        }

        // 4. Textures
        for(size_t i=0; i<m_texturePool.size(); ++i) {
            auto& rec = m_texturePool[i];
            if(rec.loaded && rec.refCount == 0) {
                if (m_device && rec.asset.rhiHandle.id != 0) {
                     m_device->DestroyTexture(rec.asset.rhiHandle);
                }
                
                rec.loaded = false;
                if(!rec.path.empty()) m_textureLookup.erase(rec.path);
                m_freeTextureIndices.push_back((uint32_t)i);
                rec.generation++;
                changed = true;
            }
        }
    }
}


} // namespace framework