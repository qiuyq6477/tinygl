#pragma once

#include <vector>
#include <string>
#include <tinygl/base/tmath.h>
#include <tinygl/core/tinygl.h>

namespace tinygl {

struct Vertex {
    Vec4 Position; // Use Vec4 (w=1)
    Vec4 Normal;   // Use Vec4 (w=0)
    Vec4 TexCoords; // Use Vec4 (xy)
};

struct Material {
    // Texture slots:
    // [0] = Diffuse
    // [1] = Specular
    // [2] = Normal
    // ...
    std::vector<GLuint> textures;
    
    float shininess = 32.0f;
    Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f}; 

    // Helper to add/set texture at specific slot
    void SetTexture(int slot, GLuint textureID) {
        if (slot >= (int)textures.size()) {
            textures.resize(slot + 1, 0);
        }
        textures[slot] = textureID;
    }
};

class TINYGL_API Mesh {
public:
    // Mesh Data
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    Material material;

    Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, Material material, SoftRenderContext& ctx);
    ~Mesh();

    // Disable copying to prevent double-free of GL resources
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    // Enable moving
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    // Template Draw function must be in header
    template <typename ShaderT>
    void Draw(SoftRenderContext& ctx, ShaderT& shader) {
        // Bind textures from slots
        for (size_t i = 0; i < material.textures.size(); ++i) {
            if (material.textures[i] != 0) {
                ctx.glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
                ctx.glBindTexture(GL_TEXTURE_2D, material.textures[i]);
            }
        }

        // Draw mesh
        ctx.glBindVertexArray(VAO);
        ctx.glDrawElements(shader, GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, 0);
        
        // Reset/Cleanup (Optional, but good practice in state machines)
        ctx.glBindVertexArray(0);
    }

private:
    // Render data
    GLuint VAO = 0, VBO = 0, EBO = 0;
    SoftRenderContext* m_ctx = nullptr;

    // Initializes all the buffer objects/arrays
    void setupMesh(SoftRenderContext& ctx);
};

} // namespace tinygl
