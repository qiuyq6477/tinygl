#pragma once

#include <cstdint>

namespace framework {

// 4-byte Magic Numbers
// 'T' 'M' 'O' 'L' = 0x4C4F4D54 (Little Endian)
constexpr uint32_t MAGIC_TMODEL = 0x4C4F4D54;
// 'T' 'T' 'E' 'X' = 0x58455454
constexpr uint32_t MAGIC_TTEX = 0x58455454;

constexpr uint32_t ASSET_VERSION = 2; // Bump version for new format

enum class CompressionMode : uint32_t {
    None = 0,
    LZ4 = 1
};

struct AssetHeader {
    uint32_t magic;
    uint32_t version;
    CompressionMode compressionMode; 
    uint64_t dataSize;   // Size of the payload following this header
};

// =================================================================================================
// Texture Format
// =================================================================================================

struct TextureHeader {
    uint32_t width;
    uint32_t height;
    uint32_t channels;   // e.g. 4 for RGBA
    uint32_t mipLevels;
    uint32_t format;     // RHI Format Enum (to be defined, for now 0=RGBA8)
    // Payload follows immediately:
    // [Mip 0 Data] [Mip 1 Data] ...
};

// =================================================================================================
// Model / Prefab Format
// =================================================================================================

struct ModelHeader {
    uint32_t meshCount;
    uint32_t materialCount; // New: Explicit material count
    uint32_t nodeCount;     // Future: For hierarchy
};

// Material Data - Uniform Buffer Ready (std140 alignment)
struct alignas(16) MaterialDataUB {
    float diffuse[4];    // 16 bytes
    float ambient[4];    // 16 bytes
    float specular[4];   // 16 bytes
    float emissive[4];   // 16 bytes
    float shininess;     // 4 bytes
    float opacity;       // 4 bytes
    int32_t alphaTest;   // 4 bytes
    int32_t doubleSided; // 4 bytes
};

// Material Header in file
struct MaterialHeader {
    MaterialDataUB data;
    uint32_t texturePathLengths[6]; // Length of paths for 6 slots
    // Followed by:
    // [Path String 0 (no null term)] [Path String 1] ...
};

// Vertex Structure (must match standard layout)
struct VertexPacked {
    float pos[4];
    float norm[4];
    float uv[4];
};

struct SubMeshHeader {
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t materialIndex; // Index into the model's material list
    // AABB
    float minBounds[3];
    float maxBounds[3];
    
    // Payload follows:
    // [VertexPacked * vertexCount]
    // [uint32_t * indexCount]
};

} // namespace framework