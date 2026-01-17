#pragma once

#include <tinygl/tinygl.h>
#include <rhi/types.h>
#include <framework/asset_handle.h>
#include <framework/shared_asset.h>
#include <vector>
#include <string>

namespace framework {

// --- 1. Texture Resource ---
struct TextureResource {
    rhi::TextureHandle rhiHandle;
    uint32_t width;
    uint32_t height;
    std::string name;
};

// --- 2. Material Resource ---
// Decoupled material data. No dependency on Mesh.
struct MaterialResource {
    // Pipeline / Shader State
    // For now we assume a standard shader, so we just store parameters.
    // In future: AssetHandle<Shader> shader;

    struct Data {
        Vec4 diffuse = {1,1,1,1};
        Vec4 ambient = {0.1f,0.1f,0.1f,1};
        Vec4 specular = {0.5f,0.5f,0.5f,1};
        Vec4 emissive = {0,0,0,0};
        float shininess = 32.0f;
        float opacity = 1.0f;
        int alphaTest = 0;
        int doubleSided = 0;
    } data;

    // Texture Slots (Shared Handles for auto-refcount)
    // Slot 0: Diffuse, 1: Specular, 2: Normal
    static constexpr int MAX_TEXTURES = 6;
    SharedAsset<TextureResource> textures[MAX_TEXTURES]; 
    
    // Cached RHI handles for fast binding (updated by RenderSystem if dirty)
    rhi::TextureHandle rhiTextures[MAX_TEXTURES];

    std::string name;
};

// --- 3. Mesh Resource ---
struct MeshResource {
    rhi::BufferHandle vbo;
    rhi::BufferHandle ebo;
    rhi::VertexArrayHandle vao; // Optional, if RHI supports it
    
    uint32_t vertexCount;
    uint32_t indexCount;
    
    // Bounding Box for Culling
    Vec4 minBounds;
    Vec4 maxBounds;

    std::string name;
};

// --- 4. Prefab (The "Model") ---
// A blueprint for creating entities.
struct PrefabNode {
    // Transform relative to parent
    Vec4 position = {0,0,0,1};
    Vec4 rotation = {0,0,0,0}; // Euler
    Vec4 scale = {1,1,1,0};
    
    // Components to attach
    SharedAsset<MeshResource> mesh;
    SharedAsset<MaterialResource> material;
    
    // Hierarchy
    int parentIndex = -1; // -1 means root
    std::string name;
};

struct Prefab {
    // Flattened hierarchy
    std::vector<PrefabNode> nodes;
    std::string name;
};

} // namespace framework