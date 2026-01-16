#pragma once

#include <string>
#include <tinygl/tinygl.h>

namespace tests {

class Texture {
public:
    GLuint id = 0;
    int width = 0;
    int height = 0;
    int channels = 0;
    std::string path;

    // Constructor loads the texture immediately
    Texture(SoftRenderContext& ctx, const std::string& path, const std::string& directory = "");
    
    // Destructor frees the GL texture
    ~Texture();

    // Prevent copying to avoid double-free of GL resource
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // Allow moving
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Helper to bind
    void Bind(SoftRenderContext& ctx, int unit) const;

private:
    SoftRenderContext* m_ctx = nullptr;
};

} // namespace tests
