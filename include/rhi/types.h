#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

namespace rhi {

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

enum class BlendFactor {
    Zero,
    One,
    SrcColor,
    OneMinusSrcColor,
    SrcAlpha,
    OneMinusSrcAlpha,
    DstColor,
    OneMinusDstColor,
    DstAlpha,
    OneMinusDstAlpha
};

enum class BlendOp {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max
};

struct BlendState {
    bool enabled = false;
    BlendFactor srcRGB = BlendFactor::One;
    BlendFactor dstRGB = BlendFactor::Zero;
    BlendFactor srcAlpha = BlendFactor::One;
    BlendFactor dstAlpha = BlendFactor::Zero;
    BlendOp opRGB = BlendOp::Add;
    BlendOp opAlpha = BlendOp::Add;
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
    
    // [Advanced] Hint for SoA optimizations (optional)
    bool useSoALayout = false; 
    uint32_t stride = 0; 
};

// Pipeline State Description
// Defines the complete state of the GPU pipeline
struct PipelineDesc {
    ShaderHandle shader; // For SoftRender, we usually have one combined shader object
    
    // Vertex Input Layout
    VertexInputState inputLayout;
    
    // [Advanced] Attribute Binding Mode
    // true: All attributes sourced from Binding 0 (Interleaved).
    // false: Attribute 'i' sourced from Binding 'i' (Planar/Multi-stream).
    bool useInterleavedAttributes = true;

    CullMode cullMode = CullMode::Back;
    PrimitiveType primitiveType = PrimitiveType::Triangles;
    
    // Depth State
    bool depthTestEnabled = true;
    bool depthWriteEnabled = true;
    
    // Blend State
    BlendState blend;
    
    const char* label = nullptr;
};

// --- Render Pass ---

enum class LoadAction {
    Load,     // Preserve existing content
    Clear,    // Clear to a specific value
    DontCare  // Content is undefined (performance optimization)
};

enum class StoreAction {
    Store,    // Save content to memory
    DontCare  // Discard content
};

struct RenderRect {
    int x = 0, y = 0, w = -1, h = -1; // w < 0 implies "Invalid/Disabled" or "Full Screen" depending on context
};

struct RenderPassDesc {
    LoadAction colorLoadOp = LoadAction::Clear;
    StoreAction colorStoreOp = StoreAction::Store;
    float clearColor[4] = {0.0f, 0.0f, 0.0f, 1.0f};

    LoadAction depthLoadOp = LoadAction::Clear;
    StoreAction depthStoreOp = StoreAction::Store;
    float clearDepth = 1.0f;

    // Initial State to apply at start of Pass
    // If w < 0, Scissor Test is DISABLED.
    RenderRect initialScissor = {0, 0, -1, -1}; 
    
    // Explicit Render Area (Scissor) for LoadAction::Clear
    // If w < 0, defaults to clearing the entire attachment (Standard behavior)
    // If specified, Scissor Test is enabled JUST for the Clear operation, then reset to initialScissor.
    RenderRect renderArea = {0, 0, -1, -1};

    // If w < 0, Viewport is reset to full framebuffer size (if supported by backend)
    // or kept as is. For safety in "Stateless" mode, backend should likely reset to target size.
    RenderRect initialViewport = {0, 0, -1, -1}; 
};

}
