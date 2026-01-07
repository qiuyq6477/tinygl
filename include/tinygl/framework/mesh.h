#pragma once

#include <vector>
#include <string>
#include <tinygl/base/tmath.h>
#include <tinygl/core/tinygl.h>
#include <tinygl/framework/material.h>
#include <tinygl/framework/render_defs.h>

namespace tinygl {

struct Vertex {
    Vec4 Position; // Use Vec4 (w=1)
    Vec4 Normal;   // Use Vec4 (w=0)
    Vec4 TexCoords; // Use Vec4 (xy)
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

    // New Draw: Delegates to the Material's ShaderPass
    void Draw(const Mat4& modelMatrix, const RenderState& state);

    // Accessor for ShaderPass
    GLuint GetVAO() const { return VAO; }

private:
    // Render data
    GLuint VAO = 0, VBO = 0, EBO = 0;
    SoftRenderContext* m_ctx = nullptr;

    // Initializes all the buffer objects/arrays
    void setupMesh();
};

} // namespace tinygl
