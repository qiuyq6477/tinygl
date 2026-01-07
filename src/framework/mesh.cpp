#include <tinygl/framework/mesh.h>
#include <tinygl/framework/shader_pass.h>

namespace tinygl {

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, Material material, SoftRenderContext& ctx)
    : vertices(std::move(vertices)), indices(std::move(indices)), material(std::move(material)), m_ctx(&ctx)
{
    setupMesh();
}

void Mesh::Draw(const Mat4& modelMatrix, const RenderState& state) {
    if (material.shader && m_ctx) {
        material.shader->Draw(*m_ctx, *this, modelMatrix, state);
    } else {
        // Fallback or warning log
    }
}

Mesh::~Mesh() {
    if (m_ctx) {
        // Clean up buffers
        GLuint buffers[2] = {VBO, EBO};
        // Check if valid handles before delete (0 is ignored by GL but good to be explicit)
        if (VBO != 0 || EBO != 0) {
            m_ctx->glDeleteBuffers(2, buffers);
        }
        if (VAO != 0) {
            m_ctx->glDeleteVertexArrays(1, &VAO);
        }
    }
}

Mesh::Mesh(Mesh&& other) noexcept 
    : vertices(std::move(other.vertices)),
      indices(std::move(other.indices)),
      material(std::move(other.material)),
      VAO(other.VAO), VBO(other.VBO), EBO(other.EBO),
      m_ctx(other.m_ctx)
{
    // Steal ownership
    other.VAO = 0;
    other.VBO = 0;
    other.EBO = 0;
    other.m_ctx = nullptr;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept {
    if (this != &other) {
        // Free existing resources
        if (m_ctx) {
             GLuint buffers[2] = {VBO, EBO};
             if (VBO != 0 || EBO != 0) m_ctx->glDeleteBuffers(2, buffers);
             if (VAO != 0) m_ctx->glDeleteVertexArrays(1, &VAO);
        }

        // Move data
        vertices = std::move(other.vertices);
        indices = std::move(other.indices);
        material = std::move(other.material);
        VAO = other.VAO;
        VBO = other.VBO;
        EBO = other.EBO;
        m_ctx = other.m_ctx;

        // Reset source
        other.VAO = 0;
        other.VBO = 0;
        other.EBO = 0;
        other.m_ctx = nullptr;
    }
    return *this;
}

void Mesh::setupMesh() {
    if (!m_ctx) return;

    // Generate Buffers
    GLuint buffers[2];
    m_ctx->glGenBuffers(2, buffers);
    VBO = buffers[0];
    EBO = buffers[1];

    m_ctx->glGenVertexArrays(1, &VAO);
    m_ctx->glBindVertexArray(VAO);

    // Load data into vertex buffers
    m_ctx->glBindBuffer(GL_ARRAY_BUFFER, VBO);
    // A struct's memory layout is sequential for its members (padding aside), 
    // so we can pass &vertices[0] as pointer to raw bytes.
    m_ctx->glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

    m_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    m_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &indices[0], GL_STATIC_DRAW);

    // Set the vertex attribute pointers
    // Layout:
    // 0: Position (Vec4)
    // 1: Normal (Vec4)
    // 2: TexCoords (Vec4)
    
    // Position
    m_ctx->glEnableVertexAttribArray(0);
    m_ctx->glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Position));

    // Normal
    m_ctx->glEnableVertexAttribArray(1);
    m_ctx->glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, Normal));

    // TexCoords
    m_ctx->glEnableVertexAttribArray(2);
    m_ctx->glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, TexCoords));

    m_ctx->glBindVertexArray(0);
}

} // namespace tinygl