#pragma once
#include <rhi/command_buffer.h>
#include <rhi/device.h>

namespace rhi {

class CommandEncoder {
public:
    CommandEncoder() : m_buffer(4096) {}

    void Reset() {
        m_buffer.Reset();
    }

    // --- State Commands ---

    void SetPipeline(PipelineHandle pipeline) {
        PacketSetPipeline pkt;
        pkt.type = CommandType::SetPipeline;
        pkt.size = sizeof(PacketSetPipeline);
        pkt.handle = pipeline;
        m_buffer.Write(pkt);
    }

    void SetViewport(float x, float y, float w, float h) {
        PacketSetViewport pkt;
        pkt.type = CommandType::SetViewport;
        pkt.size = sizeof(PacketSetViewport);
        pkt.x = x; pkt.y = y; pkt.w = w; pkt.h = h;
        m_buffer.Write(pkt);
    }

    void SetScissor(int x, int y, int w, int h) {
        PacketSetScissor pkt;
        pkt.type = CommandType::SetScissor;
        pkt.size = sizeof(PacketSetScissor);
        pkt.x = x; pkt.y = y; pkt.w = w; pkt.h = h;
        m_buffer.Write(pkt);
    }

    // Legacy/Convenience: Set Binding 0 with stride 0 (implied from pipeline)
    void SetVertexBuffer(BufferHandle buffer, uint32_t offset = 0) {
        SetVertexStream(0, buffer, offset, 0);
    }

    // Advanced: Set specific binding slot
    void SetVertexStream(uint32_t bindingIndex, BufferHandle buffer, uint32_t offset, uint32_t stride) {
        PacketSetVertexStream pkt;
        pkt.type = CommandType::SetVertexStream;
        pkt.size = sizeof(PacketSetVertexStream);
        pkt.handle = buffer;
        pkt.offset = offset;
        pkt.stride = stride;
        pkt.bindingIndex = (uint16_t)bindingIndex;
        pkt._padding = 0;
        m_buffer.Write(pkt);
    }

    void SetIndexBuffer(BufferHandle buffer, uint32_t offset = 0) {
        PacketSetIndexBuffer pkt;
        pkt.type = CommandType::SetIndexBuffer;
        pkt.size = sizeof(PacketSetIndexBuffer);
        pkt.handle = buffer;
        pkt.offset = offset;
        m_buffer.Write(pkt);
    }

    void SetTexture(uint8_t slot, TextureHandle texture) {
        PacketSetTexture pkt;
        pkt.type = CommandType::SetTexture;
        pkt.size = sizeof(PacketSetTexture);
        pkt.handle = texture;
        pkt.slot = slot;
        m_buffer.Write(pkt);
    }

    void Clear(float r, float g, float b, float a, bool color = true, bool depth = true, bool stencil = false) {
        PacketClear pkt;
        pkt.type = CommandType::Clear;
        pkt.size = sizeof(PacketClear);
        pkt.r = r; pkt.g = g; pkt.b = b; pkt.a = a;
        pkt.color = color; pkt.depth = depth; pkt.stencil = stencil;
        pkt.depthValue = 1.0f;
        pkt.stencilValue = 0;
        m_buffer.Write(pkt);
    }

    // --- Data Commands ---

    void UpdateUniform(uint8_t slot, const void* data, size_t size) {
        m_buffer.WriteUniform(slot, data, size);
    }

    template <typename T>
    void UpdateUniform(uint8_t slot, const T& data) {
        UpdateUniform(slot, &data, sizeof(T));
    }

    // --- Draw Commands ---

    void Draw(uint32_t vertexCount, uint32_t firstVertex = 0, uint32_t instanceCount = 1) {
        PacketDraw pkt;
        pkt.type = CommandType::Draw;
        pkt.size = sizeof(PacketDraw);
        pkt.vertexCount = vertexCount;
        pkt.firstVertex = firstVertex;
        pkt.instanceCount = instanceCount;
        m_buffer.Write(pkt);
    }

    void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0, int32_t baseVertex = 0, uint32_t instanceCount = 1) {
        PacketDrawIndexed pkt;
        pkt.type = CommandType::DrawIndexed;
        pkt.size = sizeof(PacketDrawIndexed);
        pkt.indexCount = indexCount;
        pkt.firstIndex = firstIndex;
        pkt.baseVertex = baseVertex;
        pkt.instanceCount = instanceCount;
        m_buffer.Write(pkt);
    }

    // --- Submission ---

    void SubmitTo(IGraphicsDevice& device) {
        // Ensure pass is ended before submission
        if (m_isInsidePass) {
            // In a real engine, this might auto-end or error. 
            // For now, let's just log or assume user error.
            // Ideally: assert(!m_isInsidePass);
        }
        device.Submit(m_buffer);
    }

    const CommandBuffer& GetBuffer() const { return m_buffer; }

    // --- Render Pass (Stateless) ---

    void BeginRenderPass(const RenderPassDesc& desc) {
        // Validation: No nested passes
        // assert(!m_isInsidePass); 
        m_isInsidePass = true;

        PacketBeginPass pkt;
        pkt.type = CommandType::BeginPass;
        pkt.size = sizeof(PacketBeginPass);
        pkt.colorLoadOp = desc.colorLoadOp;
        pkt.depthLoadOp = desc.depthLoadOp;
        pkt.clearDepth = desc.clearDepth;
        // Copy color array
        for(int i=0; i<4; ++i) pkt.clearColor[i] = desc.clearColor[i];
        
        // Initial State
        pkt.scX = desc.initialScissor.x;
        pkt.scY = desc.initialScissor.y;
        pkt.scW = desc.initialScissor.w;
        pkt.scH = desc.initialScissor.h;
        
        pkt.vpX = desc.initialViewport.x;
        pkt.vpY = desc.initialViewport.y;
        pkt.vpW = desc.initialViewport.w;
        pkt.vpH = desc.initialViewport.h;

        pkt.raX = desc.renderArea.x;
        pkt.raY = desc.renderArea.y;
        pkt.raW = desc.renderArea.w;
        pkt.raH = desc.renderArea.h;

        m_buffer.Write(pkt);
    }

    void EndRenderPass() {
        // Validation
        // assert(m_isInsidePass);
        m_isInsidePass = false;

        PacketEndPass pkt;
        pkt.type = CommandType::EndPass;
        pkt.size = sizeof(PacketEndPass);
        m_buffer.Write(pkt);
    }

private:
    CommandBuffer m_buffer;
    bool m_isInsidePass = false;
};

}