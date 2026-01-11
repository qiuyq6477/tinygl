#pragma once
#include <rhi/types.h>

namespace rhi {

enum class CommandType : uint8_t {
    SetPipeline,
    SetVertexBuffer,
    SetIndexBuffer,
    SetTexture,      // Bind texture to a slot
    UpdateUniform,   // Update uniform data from payload
    Draw,
    DrawIndexed,
    SetViewport,
    SetScissor
};

// A single rendering command. Designed to be POD and relatively small.
struct RenderCommand {
    CommandType type;
    // Explicit padding or flags could go here
    
    union {
        // SET_PIPELINE
        struct { PipelineHandle handle; } pipeline;
        
        // SET_VERTEX_BUFFER / SET_INDEX_BUFFER
        struct { BufferHandle handle; uint32_t offset; } buffer;
        
        // SET_TEXTURE
        struct { TextureHandle handle; uint8_t slot; } texture;

        // UPDATE_UNIFORM
        struct {
            uint32_t dataOffset; // Offset into the command packet's payload buffer
            uint32_t size;       // Size of data to copy
            // For now, soft rasterizer might treat 'slot' as binding point index
            // or we might need named bindings later. Let's stick to slots.
            uint8_t  slot; 
        } uniform;

        // DRAW
        struct { 
            uint32_t vertexCount; 
            uint32_t firstVertex; 
            uint32_t instanceCount; 
        } draw;

        // DRAW_INDEXED
        struct { 
            uint32_t indexCount; 
            uint32_t firstIndex; 
            int32_t  baseVertex; 
            uint32_t instanceCount;
        } drawIndexed;

        // SET_VIEWPORT / SCISSOR
        struct {
            float x, y, w, h;
        } viewport;
    };
};

}
