#pragma once
#include <tinygl/core/tinygl.h>

namespace tinygl {
    // Loads a texture from a file and creates a GL texture object.
    // Returns true on success, false on failure (creates a 1x1 pink fallback texture).
    TINYGL_API bool LoadTextureFromFile(SoftRenderContext& ctx, const char* filename, GLenum textureUnit, GLuint& tex);

    // Loads raw image data. Returns pointer to data, sets width, height, channels.
    // Must be freed with FreeImageRaw.
    TINYGL_API unsigned char* LoadImageRaw(const char* filename, int* width, int* height, int* channels, int desired_channels);
    
    // Frees image data loaded with LoadImageRaw.
    TINYGL_API void FreeImageRaw(void* data);
}
