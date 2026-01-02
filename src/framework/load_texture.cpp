#include <tinygl/framework/load_texture.h>
#include <tinygl/third_party/stb_image.h>
#include <cstdio>

namespace tinygl {

bool LoadTextureFromFile(SoftRenderContext& ctx, const char* filename, GLenum textureUnit, GLuint& tex) {
    int width, height, nrChannels;
    
    // Default to flipping vertically as per standard OpenGL conventions for images
    stbi_set_flip_vertically_on_load(true); 

    // Force load as 4 channels (RGBA)
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 4);
    
    if (data) {
        ctx.glGenTextures(1, &tex);
        ctx.glActiveTexture(textureUnit);
        ctx.glBindTexture(GL_TEXTURE_2D, tex);

        // Set default parameters
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        // Upload data
        ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        stbi_image_free(data);
        printf("Texture loaded: %s (%dx%d)\n", filename, width, height);
        return true;
    } else {
        printf("Failed to load texture: %s\n", filename);
        // Create fallback pink texture
        uint32_t pinkPixel = 0xFFFF00FF; // ABGR
        ctx.glGenTextures(1, &tex);
        ctx.glBindTexture(GL_TEXTURE_2D, tex);
        ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &pinkPixel);
        return false;
    }
}

}
