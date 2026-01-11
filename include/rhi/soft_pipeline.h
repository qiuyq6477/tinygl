#pragma once
#include <tinygl/tinygl.h>
#include <rhi/types.h>
#include <cstring>

namespace rhi {

// Abstract interface for a pipeline object in the SoftRender backend.
// Hides the template type of the shader.
class ISoftPipeline {
public:
    virtual ~ISoftPipeline() = default;

    // Called by SoftDevice::Submit when a Draw command is executed.
    // ctx: The underlying SoftRenderContext
    // uniformData: Pointer to the accumulated uniform data (slots)
    // args: Draw arguments
    virtual void Draw(SoftRenderContext& ctx, 
                      const std::vector<uint8_t>& uniformData,
                      const RenderCommand& cmd,
                      uint32_t vboId,
                      uint32_t vertexBufferOffset) = 0;
                      
    virtual void DrawIndexed(SoftRenderContext& ctx,
                             const std::vector<uint8_t>& uniformData,
                             const RenderCommand& cmd,
                             uint32_t vboId,
                             uint32_t iboId,
                             uint32_t vertexBufferOffset) = 0;
};

// Template implementation bridging RHI to SoftRender templates
template <typename ShaderT>
class SoftPipeline : public ISoftPipeline {
public:
    // Store pipeline state that SoftRenderContext needs
    // (e.g. blend mode, depth test, etc.)
    PipelineDesc desc;
    SoftRenderContext* m_ctx = nullptr;
    GLuint m_vao = 0;

    SoftPipeline(SoftRenderContext& ctx, const PipelineDesc& d) : desc(d), m_ctx(&ctx) {
        // [DSA Phase 4] Initialize VAO state once
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
            
            // Format
            m_ctx->glVertexArrayAttribFormat(m_vao, attr.shaderLocation, size, type, normalized, attr.offset);
            
            // Binding (All attributes use Binding 0 for simple VBO)
            m_ctx->glVertexArrayAttribBinding(m_vao, attr.shaderLocation, 0);
            
            // Enable
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
              const RenderCommand& cmd,
              uint32_t vboId,
              uint32_t vertexBufferOffset) override {
        
        SetupState(ctx);
        
        // [DSA Phase 4] Bind VAO and update VBO binding
        ctx.glBindVertexArray(m_vao);
        ctx.glVertexArrayVertexBuffer(m_vao, 0, vboId, vertexBufferOffset, desc.inputLayout.stride);

        // Instantiate Shader
        ShaderT shader;
        
        // Inject Uniforms
        InjectUniforms(shader, uniformData);
        InjectResources(shader, ctx);
        
        // Draw
        if (cmd.draw.instanceCount > 1) {
             ctx.glDrawArraysInstanced(shader, GL_TRIANGLES, cmd.draw.firstVertex, cmd.draw.vertexCount, cmd.draw.instanceCount);
        } else {
             ctx.glDrawArrays(shader, GL_TRIANGLES, cmd.draw.firstVertex, cmd.draw.vertexCount);
        }
        
        RestoreState(ctx);
    }

    void DrawIndexed(SoftRenderContext& ctx, 
                     const std::vector<uint8_t>& uniformData,
                     const RenderCommand& cmd,
                     uint32_t vboId,
                     uint32_t iboId,
                     uint32_t vertexBufferOffset) override {
        SetupState(ctx);
        
        // [DSA Phase 4] Bind VAO and update VBO binding
        ctx.glBindVertexArray(m_vao);
        ctx.glVertexArrayVertexBuffer(m_vao, 0, vboId, vertexBufferOffset, desc.inputLayout.stride);
        ctx.glVertexArrayElementBuffer(m_vao, iboId); // [Fix] Bind IBO to VAO
        
        ShaderT shader;
        InjectUniforms(shader, uniformData);
        InjectResources(shader, ctx);

        // SoftRenderContext::glDrawElements expects indices type. 
        // We assume GL_UNSIGNED_INT for now as per tinygl standard.
        // offset is bytes, so we cast to void*
        void* offset = (void*)(uintptr_t)(cmd.drawIndexed.firstIndex * sizeof(uint32_t));
        
        if (cmd.drawIndexed.instanceCount > 1) {
            ctx.glDrawElementsInstanced(shader, GL_TRIANGLES, cmd.drawIndexed.indexCount, GL_UNSIGNED_INT, offset, cmd.drawIndexed.instanceCount);
        } else {
            ctx.glDrawElements(shader, GL_TRIANGLES, cmd.drawIndexed.indexCount, GL_UNSIGNED_INT, offset);
        }
        
        RestoreState(ctx);
    }

private:
    // SetupInputLayout is removed, replaced by constructor VAO setup

    void SetupState(SoftRenderContext& ctx) {
        if (desc.depthTestEnabled) ctx.glEnable(GL_DEPTH_TEST); else ctx.glDisable(GL_DEPTH_TEST);
        if (desc.cullMode == CullMode::Back) {
            ctx.glEnable(GL_CULL_FACE);
            ctx.glCullFace(GL_BACK);
        } else if (desc.cullMode == CullMode::Front) {
            ctx.glEnable(GL_CULL_FACE);
            ctx.glCullFace(GL_FRONT);
        } else {
            ctx.glDisable(GL_CULL_FACE);
        }
    }

    void RestoreState(SoftRenderContext& ctx) {
        // Reset state logic if needed
    }

    void InjectUniforms(ShaderT& shader, const std::vector<uint8_t>& uniformData) {
        // Prioritize custom binding method if available
        if constexpr (requires { shader.BindUniforms(uniformData); }) {
            shader.BindUniforms(uniformData);
        } 
        // Fallback: Slot 0 -> MaterialData (Legacy Assumption)
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
