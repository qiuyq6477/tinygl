#pragma once
#include <vector>
#include <cstring>
#include <tinygl/rhi/commands.h>
#include <tinygl/rhi/device.h>

namespace tinygl::rhi {

/**
 * @brief Helper class to record RenderCommands.
 * Handles the serialization of commands and transient data (payload) into linear buffers.
 */
class CommandEncoder {
public:
    CommandEncoder() {
        // Pre-allocate some reasonable defaults to avoid early reallocations
        commands.reserve(128);
        payload.reserve(4096); 
    }

    void Reset() {
        commands.clear();
        payload.clear();
    }

    // --- State Commands ---

    void SetPipeline(PipelineHandle pipeline) {
        RenderCommand cmd;
        cmd.type = CommandType::SetPipeline;
        cmd.pipeline.handle = pipeline;
        commands.push_back(cmd);
    }

    void SetViewport(float x, float y, float w, float h) {
        RenderCommand cmd;
        cmd.type = CommandType::SetViewport;
        cmd.viewport = {x, y, w, h};
        commands.push_back(cmd);
    }

    void SetVertexBuffer(BufferHandle buffer, uint32_t offset = 0) {
        RenderCommand cmd;
        cmd.type = CommandType::SetVertexBuffer;
        cmd.buffer.handle = buffer;
        cmd.buffer.offset = offset;
        commands.push_back(cmd);
    }

    void SetIndexBuffer(BufferHandle buffer, uint32_t offset = 0) {
        RenderCommand cmd;
        cmd.type = CommandType::SetIndexBuffer;
        cmd.buffer.handle = buffer;
        cmd.buffer.offset = offset;
        commands.push_back(cmd);
    }

    void SetTexture(uint8_t slot, TextureHandle texture) {
        RenderCommand cmd;
        cmd.type = CommandType::SetTexture;
        cmd.texture.handle = texture;
        cmd.texture.slot = slot;
        commands.push_back(cmd);
    }

    // --- Data Commands ---

    /**
     * @brief Updates uniform data for the next draw call.
     * Copies the data into the internal payload buffer.
     */
    void UpdateUniform(uint8_t slot, const void* data, size_t size) {
        RenderCommand cmd;
        cmd.type = CommandType::UpdateUniform;
        
        // Record offset
        cmd.uniform.dataOffset = static_cast<uint32_t>(payload.size());
        cmd.uniform.size = static_cast<uint32_t>(size);
        cmd.uniform.slot = slot;
        
        // Copy data to payload
        const uint8_t* bytes = static_cast<const uint8_t*>(data);
        payload.insert(payload.end(), bytes, bytes + size);
        
        // TODO: Handle alignment requirements if necessary (e.g., ensure 4-byte or 16-byte alignment)
        
        commands.push_back(cmd);
    }

    template <typename T>
    void UpdateUniform(uint8_t slot, const T& data) {
        UpdateUniform(slot, &data, sizeof(T));
    }

    // --- Draw Commands ---

    void Draw(uint32_t vertexCount, uint32_t firstVertex = 0, uint32_t instanceCount = 1) {
        RenderCommand cmd;
        cmd.type = CommandType::Draw;
        cmd.draw.vertexCount = vertexCount;
        cmd.draw.firstVertex = firstVertex;
        cmd.draw.instanceCount = instanceCount;
        commands.push_back(cmd);
    }

    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t baseVertex = 0, uint32_t instanceCount = 1) {
        RenderCommand cmd;
        cmd.type = CommandType::DrawIndexed;
        cmd.drawIndexed.indexCount = indexCount;
        cmd.drawIndexed.firstIndex = firstIndex;
        cmd.drawIndexed.baseVertex = baseVertex;
        cmd.drawIndexed.instanceCount = instanceCount;
        commands.push_back(cmd);
    }

    // --- Submission ---

    void SubmitTo(IGraphicsDevice& device) {
        device.Submit(commands.data(), commands.size(), payload.data(), payload.size());
    }

private:
    std::vector<RenderCommand> commands;
    std::vector<uint8_t> payload; 
};

}
