#pragma once

#include <vector>
#include <string>
#include <memory>
#include <tinygl/base/tmath.h>
#include <tinygl/core/tinygl.h>

namespace tinygl {

class IShaderPass; // Forward declaration

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
    
    float shininess = 32.0f;
    float opacity = 1.0f;
    Vec4 diffuse = {1.0f, 1.0f, 1.0f, 1.0f}; 
    Vec4 ambient = {0.1f, 0.1f, 0.1f, 1.0f};
    Vec4 specular = {0.5f, 0.5f, 0.5f, 1.0f};
    Vec4 emissive = {0.0f, 0.0f, 0.0f, 1.0f};

    // Flags
    bool alphaTest = false;
    float alphaCutoff = 0.5f;
    bool doubleSided = false;

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
