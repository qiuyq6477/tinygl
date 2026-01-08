#pragma once

#include <vector>
#include <string>
#include <memory>
#include <tinygl/base/tmath.h>
#include <tinygl/core/tinygl.h>

namespace tinygl {

class IShaderPass; // Forward declaration

/**
 * @brief POD structure for material data that can be directly uploaded to a GPU Uniform Buffer (UBO).
 * Uses std140-like layout principles (16-byte alignment for vectors).
 */
struct MaterialData {
    Vec4 diffuse = {1.0f, 1.0f, 1.0f, 1.0f}; 
    Vec4 ambient = {0.1f, 0.1f, 0.1f, 1.0f};
    Vec4 specular = {0.5f, 0.5f, 0.5f, 1.0f};
    Vec4 emissive = {0.0f, 0.0f, 0.0f, 1.0f};

    float shininess = 32.0f;
    float opacity = 1.0f;
    float alphaCutoff = 0.5f;
    int alphaTest = 0;      // 0 = false, 1 = true

    int doubleSided = 0;    // 0 = false, 1 = true
    float padding[3];       // Pad to 16-byte boundary (Total size: 4*16 + 4*4 + 4*4 = 96 bytes)
};

struct Material {
    std::string name;

    // Shader Pass: Defines how this material is rendered
    std::shared_ptr<IShaderPass> shader;

    // Texture slots:
    // [0] = Diffuse
    // [1] = Specular
    // [2] = Normal / Height
    // [3] = Ambient / AO
    // [4] = Emissive
    // [5] = Opacity / Alpha
    std::vector<GLuint> textures;

    // Render Data (POD)
    MaterialData data;
    
    // Convenience accessors for commonly used data (optional, but helps keep code clean)
    // Note: We use references to allow direct modification of the underlying data.
    float& shininess() { return data.shininess; }
    float& opacity() { return data.opacity; }
    Vec4& diffuse() { return data.diffuse; }
    Vec4& ambient() { return data.ambient; }
    Vec4& specular() { return data.specular; }
    Vec4& emissive() { return data.emissive; }

    // Helper to add/set texture at specific slot
    void SetTexture(int slot, GLuint textureID) {
        if (slot >= (int)textures.size()) {
            textures.resize(slot + 1, 0);
        }
        textures[slot] = textureID;
    }

    bool HasTexture(int slot) const {
        return slot >= 0 && slot < (int)textures.size() && textures[slot] != 0;
    }
};

}
