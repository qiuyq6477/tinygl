#pragma once
#include <tinygl/tinygl.h>
#include <rhi/types.h>
#include <cstring>
#include <vector>

namespace rhi {

// Abstract interface for a pipeline object in the SoftRender backend.
class ISoftPipeline {
public:
    virtual ~ISoftPipeline() = default;

    virtual void Draw(SoftRenderContext& ctx, 
                      const std::vector<uint8_t>& uniformData,
                      uint32_t vertexCount, 
                      uint32_t firstVertex, 
                      uint32_t instanceCount,
                      const uint32_t* vboIds,
                      const uint32_t* offsets,
                      const uint32_t* strides,
                      uint32_t bindingCount) = 0;
                      
    virtual void DrawIndexed(SoftRenderContext& ctx,
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
    SoftRenderContext* m_ctx = nullptr;
    GLuint m_vao = 0;

    SoftPipeline(SoftRenderContext& ctx, const PipelineDesc& d) : desc(d), m_ctx(&ctx) {
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

    void Draw(SoftRenderContext& ctx, 
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
        InjectUniforms(shader, uniformData);
        InjectResources(shader, ctx);
        
        if (instanceCount > 1) {
             ctx.glDrawArraysInstanced(shader, GL_TRIANGLES, firstVertex, vertexCount, instanceCount);
        } else {
             ctx.glDrawArrays(shader, GL_TRIANGLES, firstVertex, vertexCount);
        }
        
        RestoreState(ctx);
    }

    void DrawIndexed(SoftRenderContext& ctx, 
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
        InjectUniforms(shader, uniformData);
        InjectResources(shader, ctx);

        void* offset = (void*)(uintptr_t)(firstIndex * sizeof(uint32_t));
        
        if (instanceCount > 1) {
            ctx.glDrawElementsInstanced(shader, GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, offset, instanceCount);
        } else {
            ctx.glDrawElements(shader, GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, offset);
        }
        
        RestoreState(ctx);
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

    void SetupState(SoftRenderContext& ctx) {
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

    void RestoreState(SoftRenderContext& ctx) {
    }

    void InjectUniforms(ShaderT& shader, const std::vector<uint8_t>& uniformData) {
        if constexpr (requires { shader.BindUniforms(uniformData); }) {
            shader.BindUniforms(uniformData);
        } 
        else if constexpr (requires { shader.materialData; }) {
            if (uniformData.size() >= sizeof(shader.materialData)) {
                std::memcpy(&shader.materialData, uniformData.data(), sizeof(shader.materialData));
            }
        }
    }

    void InjectResources(ShaderT& shader, SoftRenderContext& ctx) {
        if constexpr (requires { shader.BindResources(ctx); }) {
            shader.BindResources(ctx);
        }
    }
};

}