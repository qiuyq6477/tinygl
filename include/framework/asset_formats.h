#pragma once

#include <cstdint>

namespace framework {

// 4-byte Magic Numbers
// 'T' 'M' 'D' 'L' = 0x4C444D54 (Little Endian)
constexpr uint32_t MAGIC_TMODEL = 0x4C444D54;
// 'T' 'T' 'E' 'X' = 0x58455454
constexpr uint32_t MAGIC_TTEX = 0x58455454;

constexpr uint32_t ASSET_VERSION = 1;

struct BinaryAssetHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t dataSize; // Size of the following data payload
};

// Texture Metadata
// Payload follows: [Level 0 Data] [Level 1 Data] ...
struct TextureMetadata : BinaryAssetHeader {
    uint32_t width;
    uint32_t height;
    uint32_t mipLevels;
    uint32_t format; // 0 = RGBA8, 1 = R8, etc. (To be defined in an enum)
};

// Model Metadata (Container for multiple submeshes)
struct ModelHeader : BinaryAssetHeader {
    uint32_t meshCount;
    // Followed by meshCount instances of SubMeshBlock
};

// Material Data (POD version of Material::MaterialData)
struct MaterialDataPOD {
    float diffuse[4];
    float ambient[4];
    float specular[4];
    float emissive[4];
    float shininess;
    float opacity;
    int32_t alphaTest;
    int32_t doubleSided;
};

// SubMesh Header
// Structure in file:
// [SubMeshHeader]
// [MaterialDataPOD]
// [Texture Path Strings... (Count defined in header?)]
// [Vertices...]
// [Indices...]
struct SubMeshHeader {
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t vertexAttributes; // Bitmask
    uint32_t textureCount;     // How many texture paths follow
    // AABB bounds could go here
};

} // namespace framework
