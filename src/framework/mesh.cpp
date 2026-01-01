#include "tinygl/framework/mesh.h"

namespace tinygl {

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, Material material, SoftRenderContext& ctx)
    : vertices(std::move(vertices)), indices(std::move(indices)), material(std::move(material)) 
{
    setupMesh(ctx);
}

void Mesh::setupMesh(SoftRenderContext& ctx) {
    // Generate Buffers
    GLuint buffers[2];
    ctx.glGenBuffers(2, buffers);
    VBO = buffers[0];
    EBO = buffers[1];

    ctx.glGenVertexArrays(1, &VAO);
    ctx.glBindVertexArray(VAO);

    // Load data into vertex buffers
    ctx.glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // A struct's memory layout is sequential for its members (padding aside), 
    // so we can pass &vertices[0] as pointer to raw bytes.
    ctx.glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &indices[0], GL_STATIC_DRAW);

    // Set the vertex attribute pointers
    // Layout:
    // 0: Position (Vec4)
    // 1: Normal (Vec4)
    // 2: TexCoords (Vec4)
    
    // Position
    ctx.glEnableVertexAttribArray(0);
    ctx.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));

    // Normal
    ctx.glEnableVertexAttribArray(1);
    ctx.glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    // TexCoords
    ctx.glEnableVertexAttribArray(2);
    ctx.glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    ctx.glBindVertexArray(0);
}

} // namespace tinygl
