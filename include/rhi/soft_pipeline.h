#pragma once
#include <tinygl/tinygl.h>
#include <rhi/types.h>
#include <tinygl/core/linear_allocator.h>
#include <tinygl/core/tiler.h>
#include <cstring>
#include <vector>

namespace rhi {

// Abstract interface for a pipeline object in the SoftRender backend.
class ISoftPipeline {
public:
    virtual ~ISoftPipeline() = default;

    // Frontend: VS + Clipping + Binning
    virtual void ProcessGeometry(tinygl::SoftRenderContext& ctx,
                                 tinygl::LinearAllocator& frameMem,
                                 tinygl::TileBinningSystem& tiler,
                                 uint16_t pipelineId,
                                 const std::vector<uint8_t>& uniformData,
                                 uint32_t vertexCount, 
                                 uint32_t firstVertex, 
                                 uint32_t instanceCount,
                                 const uint32_t* vboIds,
                                 const uint32_t* offsets,
                                 const uint32_t* strides,
                                 uint32_t bindingCount) = 0;
                      
    virtual void ProcessGeometryIndexed(tinygl::SoftRenderContext& ctx,
                                        tinygl::LinearAllocator& frameMem,
                                        tinygl::TileBinningSystem& tiler,
                                        uint16_t pipelineId,
                                        const std::vector<uint8_t>& uniformData,
                                        uint32_t indexCount, 
                                        uint32_t firstIndex, 
                                        int32_t baseVertex, 
                                        uint32_t instanceCount,
                                        const uint32_t* vboIds,
                                        const uint32_t* offsets,
                                        const uint32_t* strides,
                                        uint32_t bindingCount,
                                        uint32_t iboId) = 0;

    // Backend: FS (Rasterization)
    virtual void RasterizeTriangle(tinygl::SoftRenderContext& ctx,
                                   const uint8_t* uniformData,
                                   const tinygl::TriangleData& tri,
                                   const tinygl::Rect& tileRect) = 0;

    // Legacy Draw for compatibility or non-TBR paths
    virtual void Draw(tinygl::SoftRenderContext& ctx, 
                      const std::vector<uint8_t>& uniformData,
                      uint32_t vertexCount, 
                      uint32_t firstVertex, 
                      uint32_t instanceCount,
                      const uint32_t* vboIds,
                      const uint32_t* offsets,
                      const uint32_t* strides,
                      uint32_t bindingCount) = 0;
                      
    virtual void DrawIndexed(tinygl::SoftRenderContext& ctx,
                             const std::vector<uint8_t>& uniformData,
                             uint32_t indexCount, 
                             uint32_t firstIndex, 
                             int32_t baseVertex, 
                             uint32_t instanceCount,
                             const uint32_t* vboIds,
                             const uint32_t* offsets,
                             const uint32_t* strides,
                             uint32_t bindingCount,
                             uint32_t iboId) = 0;
};

// Template implementation bridging RHI to SoftRender templates
template <typename ShaderT>
class SoftPipeline : public ISoftPipeline {
public:
    PipelineDesc desc;
    tinygl::SoftRenderContext* m_ctx = nullptr;
    GLuint m_vao = 0;

    SoftPipeline(tinygl::SoftRenderContext& ctx, const PipelineDesc& d) : desc(d), m_ctx(&ctx) {
        m_ctx->glCreateVertexArrays(1, &m_vao);
        
        for (const auto& attr : desc.inputLayout.attributes) {
            GLint size = 4;
            GLenum type = GL_FLOAT;
            GLboolean normalized = GL_FALSE;
            
            switch (attr.format) {
                case VertexFormat::Float1: size = 1; type = GL_FLOAT; break;
                case VertexFormat::Float2: size = 2; type = GL_FLOAT; break;
                case VertexFormat::Float3: size = 3; type = GL_FLOAT; break;
                case VertexFormat::Float4: size = 4; type = GL_FLOAT; break;
                case VertexFormat::UByte4: size = 4; type = GL_UNSIGNED_BYTE; break;
                case VertexFormat::UByte4N: size = 4; type = GL_UNSIGNED_BYTE; normalized = GL_TRUE; break;
            }
            
            m_ctx->glVertexArrayAttribFormat(m_vao, attr.shaderLocation, size, type, normalized, attr.offset);
            uint32_t bindingIndex = desc.useInterleavedAttributes ? 0 : attr.shaderLocation;
            m_ctx->glVertexArrayAttribBinding(m_vao, attr.shaderLocation, bindingIndex);
            m_ctx->glEnableVertexArrayAttrib(m_vao, attr.shaderLocation);
        }
    }

    ~SoftPipeline() {
        if (m_ctx && m_vao != 0) {
            m_ctx->glDeleteVertexArrays(1, &m_vao);
        }
    }

    void ProcessGeometry(tinygl::SoftRenderContext& ctx,
                         tinygl::LinearAllocator& frameMem,
                         tinygl::TileBinningSystem& tiler,
                         uint16_t pipelineId,
                         const std::vector<uint8_t>& uniformData,
                         uint32_t vertexCount, 
                         uint32_t firstVertex, 
                         uint32_t instanceCount,
                         const uint32_t* vboIds,
                         const uint32_t* offsets,
                         const uint32_t* strides,
                         uint32_t bindingCount) override {
        
        SetupState(ctx);
        ctx.glBindVertexArray(m_vao);
        for (uint32_t i = 0; i < bindingCount; ++i) {
             if (vboIds[i] != 0) {
                 uint32_t effStride = strides[i] > 0 ? strides[i] : desc.inputLayout.stride;
                 ctx.glVertexArrayVertexBuffer(m_vao, i, vboIds[i], offsets[i], effStride);
             }
        }

        ShaderT shader;
        InjectUniforms(shader, uniformData.data(), uniformData.size());
        InjectResources(shader, ctx);

        // Snapshot uniforms for this draw call
        uint8_t* savedUniforms = frameMem.New<uint8_t>(uniformData.size());
        if (savedUniforms) std::memcpy(savedUniforms, uniformData.data(), uniformData.size());
        uint32_t uniformOffset = (uint32_t)(savedUniforms - frameMem.GetBasePtr());

        ctx.setBinningMode(true, [&](const tinygl::VOut& v0, const tinygl::VOut& v1, const tinygl::VOut& v2) {
            tinygl::TriangleData* tri = frameMem.New<tinygl::TriangleData>();
            if (!tri) return;
            tri->p[0] = v0.scn;
            tri->p[1] = v1.scn;
            tri->p[2] = v2.scn;
            std::memcpy(tri->varyings[0], v0.ctx.varyings, sizeof(tri->varyings[0]));
            std::memcpy(tri->varyings[1], v1.ctx.varyings, sizeof(tri->varyings[1]));
            std::memcpy(tri->varyings[2], v2.ctx.varyings, sizeof(tri->varyings[2]));
            uint32_t dataOffset = (uint32_t)((uint8_t*)tri - frameMem.GetBasePtr());
            tiler.BinTriangle(*tri, pipelineId, dataOffset, uniformOffset);
        });

        if (instanceCount > 1) ctx.glDrawArraysInstanced(shader, GL_TRIANGLES, firstVertex, vertexCount, instanceCount);
        else ctx.glDrawArrays(shader, GL_TRIANGLES, firstVertex, vertexCount);

        ctx.setBinningMode(false);
    }

    void ProcessGeometryIndexed(tinygl::SoftRenderContext& ctx,
                                tinygl::LinearAllocator& frameMem,
                                tinygl::TileBinningSystem& tiler,
                                uint16_t pipelineId,
                                const std::vector<uint8_t>& uniformData,
                                uint32_t indexCount, 
                                uint32_t firstIndex, 
                                int32_t baseVertex, 
                                uint32_t instanceCount,
                                const uint32_t* vboIds,
                                const uint32_t* offsets,
                                const uint32_t* strides,
                                uint32_t bindingCount,
                                uint32_t iboId) override {
        SetupState(ctx);
        ctx.glBindVertexArray(m_vao);
        for (uint32_t i = 0; i < bindingCount; ++i) {
             if (vboIds[i] != 0) {
                 uint32_t effStride = strides[i] > 0 ? strides[i] : desc.inputLayout.stride;
                 ctx.glVertexArrayVertexBuffer(m_vao, i, vboIds[i], offsets[i], effStride);
             }
        }
        ctx.glVertexArrayElementBuffer(m_vao, iboId);

        ShaderT shader;
        InjectUniforms(shader, uniformData.data(), uniformData.size());
        InjectResources(shader, ctx);

        uint8_t* savedUniforms = frameMem.New<uint8_t>(uniformData.size());
        if (savedUniforms) std::memcpy(savedUniforms, uniformData.data(), uniformData.size());
        uint32_t uniformOffset = (uint32_t)(savedUniforms - frameMem.GetBasePtr());

        ctx.setBinningMode(true, [&](const tinygl::VOut& v0, const tinygl::VOut& v1, const tinygl::VOut& v2) {
            tinygl::TriangleData* tri = frameMem.New<tinygl::TriangleData>();
            if (!tri) return;
            tri->p[0] = v0.scn;
            tri->p[1] = v1.scn;
            tri->p[2] = v2.scn;
            std::memcpy(tri->varyings[0], v0.ctx.varyings, sizeof(tri->varyings[0]));
            std::memcpy(tri->varyings[1], v1.ctx.varyings, sizeof(tri->varyings[1]));
            std::memcpy(tri->varyings[2], v2.ctx.varyings, sizeof(tri->varyings[2]));
            uint32_t dataOffset = (uint32_t)((uint8_t*)tri - frameMem.GetBasePtr());
            tiler.BinTriangle(*tri, pipelineId, dataOffset, uniformOffset);
        });

        void* offset = (void*)(uintptr_t)(firstIndex * sizeof(uint32_t));
        if (instanceCount > 1) ctx.glDrawElementsInstanced(shader, GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, offset, instanceCount);
        else ctx.glDrawElements(shader, GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, offset);

        ctx.setBinningMode(false);
    }

    void RasterizeTriangle(tinygl::SoftRenderContext& ctx,
                           const uint8_t* uniformData,
                           const tinygl::TriangleData& tri,
                           const tinygl::Rect& tileRect) override {
        tinygl::VOut v0, v1, v2;
        v0.scn = tri.p[0];
        v1.scn = tri.p[1];
        v2.scn = tri.p[2];
        std::memcpy(v0.ctx.varyings, tri.varyings[0], sizeof(v0.ctx.varyings));
        std::memcpy(v1.ctx.varyings, tri.varyings[1], sizeof(v1.ctx.varyings));
        std::memcpy(v2.ctx.varyings, tri.varyings[2], sizeof(v2.ctx.varyings));

        SetupState(ctx);
        ctx.glEnable(GL_SCISSOR_TEST);
        ctx.glScissor(tileRect.x, tileRect.y, tileRect.w, tileRect.h);

        ShaderT shader;
        InjectUniforms(shader, uniformData, 1024); 
        InjectResources(shader, ctx);
        ctx.rasterizeTriangleTemplate(shader, v0, v1, v2);
    }

    void Draw(tinygl::SoftRenderContext& ctx, 
              const std::vector<uint8_t>& uniformData,
              uint32_t vertexCount, 
              uint32_t firstVertex, 
              uint32_t instanceCount,
              const uint32_t* vboIds,
              const uint32_t* offsets,
              const uint32_t* strides,
              uint32_t bindingCount) override {
        SetupState(ctx);
        ctx.glBindVertexArray(m_vao);
        for (uint32_t i = 0; i < bindingCount; ++i) {
             if (vboIds[i] != 0) {
                 uint32_t effStride = strides[i] > 0 ? strides[i] : desc.inputLayout.stride;
                 ctx.glVertexArrayVertexBuffer(m_vao, i, vboIds[i], offsets[i], effStride);
             }
        }
        ShaderT shader;
        InjectUniforms(shader, uniformData.data(), uniformData.size());
        InjectResources(shader, ctx);
        if (instanceCount > 1) ctx.glDrawArraysInstanced(shader, GL_TRIANGLES, firstVertex, vertexCount, instanceCount);
        else ctx.glDrawArrays(shader, GL_TRIANGLES, firstVertex, vertexCount);
    }

    void DrawIndexed(tinygl::SoftRenderContext& ctx, 
                     const std::vector<uint8_t>& uniformData,
                     uint32_t indexCount, 
                     uint32_t firstIndex, 
                     int32_t baseVertex, 
                     uint32_t instanceCount,
                     const uint32_t* vboIds,
                     const uint32_t* offsets,
                     const uint32_t* strides,
                     uint32_t bindingCount,
                     uint32_t iboId) override {
        SetupState(ctx);
        ctx.glBindVertexArray(m_vao);
        for (uint32_t i = 0; i < bindingCount; ++i) {
             if (vboIds[i] != 0) {
                 uint32_t effStride = strides[i] > 0 ? strides[i] : desc.inputLayout.stride;
                 ctx.glVertexArrayVertexBuffer(m_vao, i, vboIds[i], offsets[i], effStride);
             }
        }
        ctx.glVertexArrayElementBuffer(m_vao, iboId);
        ShaderT shader;
        InjectUniforms(shader, uniformData.data(), uniformData.size());
        InjectResources(shader, ctx);
        void* offset = (void*)(uintptr_t)(firstIndex * sizeof(uint32_t));
        if (instanceCount > 1) ctx.glDrawElementsInstanced(shader, GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, offset, instanceCount);
        else ctx.glDrawElements(shader, GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, offset);
    }

private:
    static GLenum MapFactor(BlendFactor f) {
        switch(f) {
            case BlendFactor::Zero: return GL_ZERO;
            case BlendFactor::One: return GL_ONE;
            case BlendFactor::SrcColor: return GL_SRC_COLOR;
            case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
            case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
            case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
            case BlendFactor::DstColor: return GL_DST_COLOR;
            case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
            case BlendFactor::DstAlpha: return GL_DST_ALPHA;
            case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
            default: return GL_ONE;
        }
    }

    static GLenum MapOp(BlendOp op) {
        switch(op) {
            case BlendOp::Add: return GL_FUNC_ADD;
            case BlendOp::Subtract: return GL_FUNC_SUBTRACT;
            case BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
            case BlendOp::Min: return GL_MIN;
            case BlendOp::Max: return GL_MAX;
            default: return GL_FUNC_ADD;
        }
    }

    void SetupState(tinygl::SoftRenderContext& ctx) {
        if (desc.depthTestEnabled) ctx.glEnable(GL_DEPTH_TEST); else ctx.glDisable(GL_DEPTH_TEST);
        ctx.glDepthMask(desc.depthWriteEnabled ? GL_TRUE : GL_FALSE);

        if (desc.cullMode == CullMode::Back) {
            ctx.glEnable(GL_CULL_FACE);
            ctx.glCullFace(GL_BACK);
        } else if (desc.cullMode == CullMode::Front) {
            ctx.glEnable(GL_CULL_FACE);
            ctx.glCullFace(GL_FRONT);
        } else {
            ctx.glDisable(GL_CULL_FACE);
        }

        if (desc.blend.enabled) {
            ctx.glEnable(GL_BLEND);
            ctx.glBlendFuncSeparate(
                MapFactor(desc.blend.srcRGB), MapFactor(desc.blend.dstRGB),
                MapFactor(desc.blend.srcAlpha), MapFactor(desc.blend.dstAlpha)
            );
            ctx.glBlendEquationSeparate(MapOp(desc.blend.opRGB), MapOp(desc.blend.opAlpha));
        } else {
            ctx.glDisable(GL_BLEND);
        }
    }

    void InjectUniforms(ShaderT& shader, const uint8_t* uniformData, size_t size) {
        if constexpr (requires { shader.BindUniforms(uniformData, size); }) {
            shader.BindUniforms(uniformData, size);
        } 
        else if constexpr (requires { shader.materialData; }) {
            if (size >= sizeof(shader.materialData)) {
                std::memcpy(&shader.materialData, uniformData, sizeof(shader.materialData));
            }
        }
    }

    void InjectResources(ShaderT& shader, tinygl::SoftRenderContext& ctx) {
        if constexpr (requires { shader.BindResources(ctx); }) {
            shader.BindResources(ctx);
        }
    }
};

}