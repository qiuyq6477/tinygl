#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace tinygl::rhi {

// --- Handles ---
// Simple handle wrapper. ID 0 is reserved for invalid/null.
struct BufferHandle { uint32_t id; bool IsValid() const { return id != 0; } };
struct TextureHandle { uint32_t id; bool IsValid() const { return id != 0; } };
struct ShaderHandle { uint32_t id; bool IsValid() const { return id != 0; } };
struct PipelineHandle { uint32_t id; bool IsValid() const { return id != 0; } };

static constexpr uint32_t INVALID_ID = 0;

// --- Enums ---
enum class BufferType {
    VertexBuffer,
    IndexBuffer,
    UniformBuffer
};

enum class BufferUsage {
    Immutable, // Data uploaded once, never changed
    Dynamic,   // Data updated frequently (per frame)
    Stream     // Data updated every frame (discarded)
};

enum class PrimitiveType {
    Triangles,
    Lines,
    Points
};

enum class CullMode {
    None,
    Front,
    Back
};

enum class IndexFormat {
    Uint16,
    Uint32
};

// --- Vertex Layout ---

enum class VertexFormat {
    Float1, Float2, Float3, Float4,
    UByte4, UByte4N
};

struct VertexAttribute {
    VertexFormat format = VertexFormat::Float4;
    uint32_t offset = 0;
    uint32_t shaderLocation = 0; // The attribute index (0, 1, 2...)
};

struct VertexInputState {
    uint32_t stride = 0;
    // Simple fixed-size array or vector. Vector is easier but not POD.
    // For a header-only definition, let's use vector and hope for the best, 
    // or use a fixed max size to keep it POD-ish.
    // Given std::vector is used elsewhere, we'll use it.
    std::vector<VertexAttribute> attributes;
};

// --- Descriptors ---

struct BufferDesc {
    BufferType type;
    BufferUsage usage = BufferUsage::Immutable;
    size_t size = 0;
    const void* initialData = nullptr; // Optional
    const char* label = nullptr;       // For debugging
};

// Pipeline State Description
// Defines the complete state of the GPU pipeline
struct PipelineDesc {
    ShaderHandle shader; // For SoftRender, we usually have one combined shader object
    
    // Vertex Input Layout
    VertexInputState inputLayout;

    // Rasterization State
    CullMode cullMode = CullMode::Back;
    PrimitiveType primitiveType = PrimitiveType::Triangles;
    
    // Depth State
    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    
    // Blend State (Simplified)
    bool blendEnabled = false;
    
    const char* label = nullptr;
};

}
