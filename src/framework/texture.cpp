#include <framework/texture.h>
#include <tinygl/base/log.h>
#include <stb_image.h>
#include <utility>

namespace framework {

Texture::Texture(SoftRenderContext& ctx, const std::string& relPath, const std::string& directory) 
    : m_ctx(&ctx) 
{
    std::string filename = relPath;
    if (!directory.empty()) {
        filename = directory + '/' + filename;
    }
    this->path = filename;

    ctx.glGenTextures(1, &id);
    // Default to flipping vertically as per standard OpenGL conventions for images
    stbi_set_flip_vertically_on_load(true); 
    // Force loading as 4 components (RGBA)
    unsigned char *data = stbi_load(filename.c_str(), &width, &height, &channels, 4);
    if (data) {
        GLenum format = GL_RGBA;

        ctx.glBindTexture(GL_TEXTURE_2D, id);
        ctx.glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        
        // Set default parameters
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // Or GL_LINEAR_MIPMAP_LINEAR
        ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
        LOG_INFO("Texture: Loaded " + filename);
    } else {
        LOG_ERROR("Texture failed to load at path: " + filename);
        stbi_image_free(data); // Safe to call on null? stbi says yes usually, but good to check. 
                               // Actually stbi_image_free checks for null.
    }
}

Texture::~Texture() {
    if (m_ctx && id != 0) {
        m_ctx->glDeleteTextures(1, &id);
        LOG_INFO("Texture: UnLoaded " + path);
    }
}

Texture::Texture(Texture&& other) noexcept 
    : id(other.id), width(other.width), height(other.height), 
      channels(other.channels), path(std::move(other.path)), m_ctx(other.m_ctx) 
{
    other.id = 0;
    other.m_ctx = nullptr;
}

Texture& Texture::operator=(Texture&& other) noexcept {
    if (this != &other) {
        if (m_ctx && id != 0) {
            m_ctx->glDeleteTextures(1, &id);
        }

        id = other.id;
        width = other.width;
        height = other.height;
        channels = other.channels;
        path = std::move(other.path);
        m_ctx = other.m_ctx;

        other.id = 0;
        other.m_ctx = nullptr;
    }
    return *this;
}

void Texture::Bind(SoftRenderContext& ctx, int unit) const {
    ctx.glActiveTexture(GL_TEXTURE0 + unit);
    ctx.glBindTexture(GL_TEXTURE_2D, id);
}

} // namespace tinygl
