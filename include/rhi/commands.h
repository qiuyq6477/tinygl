#pragma once
#include <rhi/types.h>
#include <cstdint>

namespace rhi {

enum class CommandType : uint16_t {
    SetPipeline,
    SetVertexStream,
    SetIndexBuffer,
    SetTexture,
    UpdateUniform,
    Draw,
    DrawIndexed,
    SetViewport,
    SetScissor,
    Clear,
    BeginPass,
    EndPass,
    // Marker for end of buffer or no-op
    NoOp
};

// Base header for all commands in the linear stream
struct CommandPacket {
    CommandType type;
    uint16_t size; // Total size of the packet including this header
};

// --- Packet Definitions ---

struct PacketBeginPass : CommandPacket {
    LoadAction colorLoadOp;
    float clearColor[4];
    LoadAction depthLoadOp;
    float clearDepth;
    // Initial State
    int scX, scY, scW, scH; // Scissor
    int vpX, vpY, vpW, vpH; // Viewport
};

struct PacketEndPass : CommandPacket {
    // Empty for now, acts as a barrier/marker
};

struct PacketSetPipeline : CommandPacket {
    PipelineHandle handle;
};

// Used for Vertex Buffers (Planar or Interleaved)
struct PacketSetVertexStream : CommandPacket {
    BufferHandle handle;
    uint32_t offset;
    uint32_t stride; 
    uint16_t bindingIndex;
    uint16_t _padding;
};

// Used for Index Buffers
struct PacketSetIndexBuffer : CommandPacket {
    BufferHandle handle;
    uint32_t offset;
};

struct PacketSetTexture : CommandPacket {
    TextureHandle handle;
    uint8_t slot;
    uint8_t _padding[3]; // Align to 4 bytes
};

struct PacketUpdateUniform : CommandPacket {
    uint8_t slot;
    uint8_t _padding[3]; // Align to 4 bytes
    // Data follows immediately after this struct
};

struct PacketDraw : CommandPacket {
    uint32_t vertexCount;
    uint32_t firstVertex;
    uint32_t instanceCount;
};

struct PacketDrawIndexed : CommandPacket {
    uint32_t indexCount;
    uint32_t firstIndex;
    int32_t  baseVertex;
    uint32_t instanceCount;
};

struct PacketSetViewport : CommandPacket {
    int x, y, w, h;
};

struct PacketSetScissor : CommandPacket {
    int x, y, w, h;
};

struct PacketClear : CommandPacket {
    bool color;
    bool depth;
    bool stencil;
    uint8_t _padding; // Align
    float r, g, b, a;
    float depthValue;
    int stencilValue;
};

}