#include <framework/asset_manager.h>
#include <framework/asset_formats.h>
#include <stb_image.h>
#include <tinygl/base/log.h>
#include <fstream>
#include <filesystem>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

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

    bool needCook = true;
    if (fs::exists(binPath)) {
        if (fs::last_write_time(binPath) >= fs::last_write_time(srcPath)) {
            needCook = false;
        }
    }

    if (needCook) {
        LOG_INFO("Cooking Texture: " + path);
        if (!CookTexture(srcPath, binPath)) {
            LOG_ERROR("Failed to cook texture: " + path);
            return; 
        }
        LOG_INFO("Cook Texture Done: " + path);
    }

    if (!LoadTextureBin(index, binPath)) {
        LOG_ERROR("Failed to load binary texture: " + binPath.string());
    }
    LOG_INFO("Loaded Texture: " + path);
}

bool AssetManager::CookTexture(const fs::path& source, const fs::path& dest) {
    int w, h, c;
    stbi_set_flip_vertically_on_load(true); 
    unsigned char* data = stbi_load(source.string().c_str(), &w, &h, &c, 4); // Force RGBA
    if (!data) return false;

    TextureMetadata header;
    header.magic = MAGIC_TTEX;
    header.version = ASSET_VERSION;
    header.width = w;
    header.height = h;
    header.mipLevels = 1;
    header.format = 0; // RGBA8
    header.dataSize = w * h * 4;

    std::ofstream out(dest, std::ios::binary);
    out.write(reinterpret_cast<char*>(&header), sizeof(header));
    out.write(reinterpret_cast<char*>(data), header.dataSize);
    
    stbi_image_free(data);
    return true;
}

bool AssetManager::LoadTextureBin(uint32_t index, const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    TextureMetadata header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));

    if (header.magic != MAGIC_TTEX || header.version != ASSET_VERSION) {
        LOG_ERROR("Invalid .ttex format version");
        return false;
    }

    std::vector<unsigned char> data(header.dataSize);
    in.read(reinterpret_cast<char*>(data.data()), header.dataSize);

    // Create Texture via RHI
    rhi::TextureHandle handle = m_device->CreateTexture(data.data(), header.width, header.height, 4);
    
    m_texturePool[index].asset.rhiHandle = handle;
    m_texturePool[index].asset.width = header.width;
    m_texturePool[index].asset.height = header.height;
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
    binPath.replace_extension(".tmodel"); // Keep using .tmodel extension

    bool needCook = true;
    if (fs::exists(binPath)) {
         if (fs::last_write_time(binPath) >= fs::last_write_time(srcPath)) {
            needCook = false;
        }
    }
    LOG_INFO("Loading Prefab: " + path);
    if (needCook) {
        LOG_INFO("Cooking Prefab: " + path);
        if (!CookModel(srcPath, binPath)) {
            LOG_ERROR("Failed to cook prefab");
            return;
        }
        LOG_INFO("Cook Prefab Done: " + path);
    }
    
    if (!LoadPrefabBin(index, binPath)) {
         LOG_ERROR("Failed to load binary prefab");
    }
    LOG_INFO("Loaded Prefab: " + path);
}

// COOKER LOGIC (Mostly same as before, just creates .tmodel)
bool AssetManager::CookModel(const fs::path& source, const fs::path& dest) {
    Assimp::Importer importer;
    // Use FlipUVs (standard OpenGL convention)
    // Use PreTransformVertices to bake node transforms into meshes, ensuring multi-part models (like backpack)
    // are assembled correctly even if we flatten the hierarchy.
    const aiScene* scene = importer.ReadFile(source.string(), aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_PreTransformVertices | aiProcess_FlipUVs);
    if (!scene || !scene->mRootNode) 
    {
        LOG_ERROR("ERROR::ASSIMP:: " + std::string(importer.GetErrorString()));
        return false;
    }

    // Simplification: We flatten the mesh list into the file.
    // In a real prefab cooker, we would traverse nodes and write hierarchy.
    // Here we just write meshes.
    
    std::vector<aiMesh*> meshes;
    std::function<void(aiNode*)> process = [&](aiNode* node) {
        for(unsigned int i = 0; i < node->mNumMeshes; i++) {
            meshes.push_back(scene->mMeshes[node->mMeshes[i]]);
        }
        for(unsigned int i = 0; i < node->mNumChildren; i++) {
            process(node->mChildren[i]);
        }
    };
    process(scene->mRootNode);

    std::ofstream out(dest, std::ios::binary);
    
    ModelHeader header;
    header.magic = MAGIC_TMODEL;
    header.version = ASSET_VERSION;
    header.meshCount = (uint32_t)meshes.size();
    header.dataSize = 0;
    
    out.write(reinterpret_cast<char*>(&header), sizeof(header));
    
    for (aiMesh* mesh : meshes) {
        SubMeshHeader smh;
        smh.vertexCount = mesh->mNumVertices;
        smh.indexCount = mesh->mNumFaces * 3;
        smh.vertexAttributes = 0; 
        
        // Count textures
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        std::vector<std::string> texPaths;
        
        auto getTexturePath = [&](aiTextureType type) -> std::string {
            if (mat->GetTextureCount(type) > 0) {
                aiString str;
                mat->GetTexture(type, 0, &str);
                return str.C_Str();
            }
            return "";
        };

        // Slot 0: Diffuse
        texPaths.push_back(getTexturePath(aiTextureType_DIFFUSE));
        // Slot 1: Specular
        texPaths.push_back(getTexturePath(aiTextureType_SPECULAR));
        // Slot 2: Normal / Height
        std::string normalPath = getTexturePath(aiTextureType_NORMALS);
        if (normalPath.empty()) normalPath = getTexturePath(aiTextureType_HEIGHT);
        texPaths.push_back(normalPath);
        // Slot 3: Ambient
        texPaths.push_back(getTexturePath(aiTextureType_AMBIENT));
        // Slot 4: Emissive
        texPaths.push_back(getTexturePath(aiTextureType_EMISSIVE));
        // Slot 5: Opacity
        texPaths.push_back(getTexturePath(aiTextureType_OPACITY));

        smh.textureCount = (uint32_t)texPaths.size();

        out.write(reinterpret_cast<char*>(&smh), sizeof(smh));
        
        // Fill Material Data
        MaterialDataPOD matData = {}; 
        aiColor3D color(1.f, 1.f, 1.f);
        if (mat->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            matData.diffuse[0] = color.r; matData.diffuse[1] = color.g; matData.diffuse[2] = color.b; matData.diffuse[3] = 1.0f;
        } else {
            matData.diffuse[0] = 1.0f; matData.diffuse[1] = 1.0f; matData.diffuse[2] = 1.0f; matData.diffuse[3] = 1.0f;
        }

        if (mat->Get(AI_MATKEY_COLOR_AMBIENT, color) == AI_SUCCESS) {
            matData.ambient[0] = color.r; matData.ambient[1] = color.g; matData.ambient[2] = color.b; matData.ambient[3] = 1.0f;
        }
        if (mat->Get(AI_MATKEY_COLOR_SPECULAR, color) == AI_SUCCESS) {
            matData.specular[0] = color.r; matData.specular[1] = color.g; matData.specular[2] = color.b; matData.specular[3] = 1.0f;
        }
        if (mat->Get(AI_MATKEY_COLOR_EMISSIVE, color) == AI_SUCCESS) {
            matData.emissive[0] = color.r; matData.emissive[1] = color.g; matData.emissive[2] = color.b; matData.emissive[3] = 1.0f;
        }
        
        float val = 0.0f;
        if (mat->Get(AI_MATKEY_SHININESS, val) == AI_SUCCESS) matData.shininess = val; else matData.shininess = 32.0f;
        if (mat->Get(AI_MATKEY_OPACITY, val) == AI_SUCCESS) matData.opacity = val; else matData.opacity = 1.0f;
        
        int bVal = 0;
        if (mat->Get(AI_MATKEY_TWOSIDED, bVal) == AI_SUCCESS) matData.doubleSided = bVal;

        out.write(reinterpret_cast<char*>(&matData), sizeof(matData));

        for(const auto& p : texPaths) {
            uint32_t len = (uint32_t)p.length();
            out.write(reinterpret_cast<char*>(&len), sizeof(len));
            if(len > 0) out.write(p.c_str(), len);
        }
        
        // Write Vertices
        struct VertexPacked {
            float pos[4];
            float norm[4];
            float uv[4];
        };
        std::vector<VertexPacked> vertices(mesh->mNumVertices);
        for(unsigned int i=0; i<mesh->mNumVertices; i++) {
            vertices[i].pos[0] = mesh->mVertices[i].x; vertices[i].pos[1] = mesh->mVertices[i].y; vertices[i].pos[2] = mesh->mVertices[i].z; vertices[i].pos[3] = 1.0f;
            if(mesh->HasNormals()) {
                vertices[i].norm[0] = mesh->mNormals[i].x; vertices[i].norm[1] = mesh->mNormals[i].y; vertices[i].norm[2] = mesh->mNormals[i].z; vertices[i].norm[3] = 0.0f;
            }
            else {
                vertices[i].norm[0] = 0.0f; vertices[i].norm[1] = 0.0f; vertices[i].norm[2] = 0.0f; vertices[i].norm[3] = 0.0f;
            }
            if(mesh->mTextureCoords[0]) {
                vertices[i].uv[0] = mesh->mTextureCoords[0][i].x; vertices[i].uv[1] = mesh->mTextureCoords[0][i].y;
                vertices[i].uv[2] = 0.0f; vertices[i].uv[3] = 0.0f;
            }
            else {
                vertices[i].uv[0] = 0.0f; vertices[i].uv[1] = 0.0f; vertices[i].uv[2] = 0.0f; vertices[i].uv[3] = 0.0f;
            }
        }
        out.write(reinterpret_cast<char*>(vertices.data()), vertices.size() * sizeof(VertexPacked));

        // Write Indices
        std::vector<uint32_t> indices;
        for(unsigned int i=0; i<mesh->mNumFaces; i++) {
            indices.push_back(mesh->mFaces[i].mIndices[0]);
            indices.push_back(mesh->mFaces[i].mIndices[1]);
            indices.push_back(mesh->mFaces[i].mIndices[2]);
        }
        out.write(reinterpret_cast<char*>(indices.data()), indices.size() * sizeof(uint32_t));
    }
    
    return true;
}

bool AssetManager::LoadPrefabBin(uint32_t index, const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    ModelHeader header;
    in.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.magic != MAGIC_TMODEL) return false;

    Prefab& prefab = m_prefabPool[index].asset;
    prefab.name = path.filename().string();
    
    std::string directory = path.parent_path().string();

    for(uint32_t i=0; i<header.meshCount; i++) {
        SubMeshHeader smh;
        in.read(reinterpret_cast<char*>(&smh), sizeof(smh));

        MaterialDataPOD matPod;
        in.read(reinterpret_cast<char*>(&matPod), sizeof(matPod));
        
        // 1. Create Material Resource
        uint32_t matIdx = AllocMaterialSlot();
        MaterialResource& matRes = m_materialPool[matIdx].asset;
        matRes.name = prefab.name + "_mat_" + std::to_string(i);
        
        matRes.data.diffuse = Vec4(matPod.diffuse[0], matPod.diffuse[1], matPod.diffuse[2], matPod.diffuse[3]);
        matRes.data.ambient = Vec4(matPod.ambient[0], matPod.ambient[1], matPod.ambient[2], matPod.ambient[3]);
        matRes.data.specular = Vec4(matPod.specular[0], matPod.specular[1], matPod.specular[2], matPod.specular[3]);
        matRes.data.emissive = Vec4(matPod.emissive[0], matPod.emissive[1], matPod.emissive[2], matPod.emissive[3]);
        matRes.data.shininess = matPod.shininess;
        matRes.data.opacity = matPod.opacity;
        matRes.data.alphaTest = matPod.alphaTest;
        matRes.data.doubleSided = matPod.doubleSided;
        
        // Read Textures
        for(uint32_t t=0; t<smh.textureCount; t++) {
            uint32_t len = 0;
            in.read(reinterpret_cast<char*>(&len), sizeof(len));
            if(len > 0) {
                std::vector<char> buf(len + 1);
                in.read(buf.data(), len);
                buf[len] = '\0';
                std::string texPath = std::string(buf.data());
                
                std::string fullPath = texPath;
                if (!directory.empty()) fullPath = directory + '/' + texPath;

                AssetHandle<TextureResource> hTex = Load<TextureResource>(fullPath);
                if (t < MaterialResource::MAX_TEXTURES) {
                    matRes.textures[t] = hTex;
                    matRes.rhiTextures[t] = GetRHI(hTex);
                }
            }
        }
        
        m_materialPool[matIdx].loaded = true;
        m_materialPool[matIdx].refCount = 1;
        AssetHandle<MaterialResource> hMat((m_materialPool[matIdx].generation << 20) | matIdx);

        // 2. Create Mesh Resource
        uint32_t meshIdx = AllocMeshSlot();
        MeshResource& meshRes = m_meshPool[meshIdx].asset;
        meshRes.name = prefab.name + "_mesh_" + std::to_string(i);
        meshRes.vertexCount = smh.vertexCount;
        meshRes.indexCount = smh.indexCount;

        // Read Vertices & Create VBO
        struct VertexPacked {
            float pos[4];
            float norm[4];
            float uv[4];
        };
        std::vector<VertexPacked> vertices(smh.vertexCount);
        in.read(reinterpret_cast<char*>(vertices.data()), smh.vertexCount * sizeof(VertexPacked));

        // Create VBO via RHI
        rhi::BufferDesc vboDesc;
        vboDesc.type = rhi::BufferType::VertexBuffer;
        vboDesc.size = vertices.size() * sizeof(VertexPacked);
        vboDesc.initialData = vertices.data();
        meshRes.vbo = m_device->CreateBuffer(vboDesc);

        // Read Indices & Create EBO
        std::vector<uint32_t> indices(smh.indexCount);
        in.read(reinterpret_cast<char*>(indices.data()), smh.indexCount * sizeof(uint32_t));
        
        rhi::BufferDesc eboDesc;
        eboDesc.type = rhi::BufferType::IndexBuffer;
        eboDesc.size = indices.size() * sizeof(uint32_t);
        eboDesc.initialData = indices.data();
        meshRes.ebo = m_device->CreateBuffer(eboDesc);
        
        m_meshPool[meshIdx].loaded = true;
        m_meshPool[meshIdx].refCount = 1;
        AssetHandle<MeshResource> hMesh((m_meshPool[meshIdx].generation << 20) | meshIdx);

        // 3. Add Node to Prefab
        PrefabNode node;
        node.name = meshRes.name;
        node.mesh = hMesh;
        node.material = hMat;
        node.parentIndex = -1; // All flat for now
        prefab.nodes.push_back(node);
    }
    
    m_prefabPool[index].loaded = true;
    return true; 
}

void AssetManager::GarbageCollect() {
    // TODO
}

} // namespace framework