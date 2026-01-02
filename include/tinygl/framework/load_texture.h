#pragma once
#include <tinygl/core/tinygl.h>

namespace tinygl {
    // Loads a texture from a file and creates a GL texture object.
    // Returns true on success, false on failure (creates a 1x1 pink fallback texture).
    bool LoadTextureFromFile(SoftRenderContext& ctx, const char* filename, GLenum textureUnit, GLuint& tex);
}
