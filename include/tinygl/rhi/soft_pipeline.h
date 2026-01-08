#pragma once
#include <tinygl/core/tinygl.h>
#include <tinygl/rhi/types.h>
#include <cstring>

namespace tinygl::rhi {

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
                      uint32_t vertexBufferOffset) = 0;
                      
    virtual void DrawIndexed(SoftRenderContext& ctx,
                             const std::vector<uint8_t>& uniformData,
                             const RenderCommand& cmd,
                             uint32_t vertexBufferOffset) = 0;
};

// Template implementation bridging RHI to SoftRender templates
template <typename ShaderT>
class SoftPipeline : public ISoftPipeline {
public:
    // Store pipeline state that SoftRenderContext needs
    // (e.g. blend mode, depth test, etc.)
    PipelineDesc desc;

    SoftPipeline(const PipelineDesc& d) : desc(d) {}

    void Draw(SoftRenderContext& ctx, 
              const std::vector<uint8_t>& uniformData,
              const RenderCommand& cmd,
              uint32_t vertexBufferOffset) override {
        
        SetupState(ctx);
        SetupInputLayout(ctx, vertexBufferOffset);

        // Instantiate Shader
        ShaderT shader;
        
        // Inject Uniforms
        InjectUniforms(shader, uniformData);
        InjectResources(shader, ctx);
        
        // Draw
        ctx.glDrawArrays(shader, GL_TRIANGLES, cmd.draw.firstVertex, cmd.draw.vertexCount);
        
        RestoreState(ctx);
    }

    void DrawIndexed(SoftRenderContext& ctx, 
                     const std::vector<uint8_t>& uniformData,
                     const RenderCommand& cmd,
                     uint32_t vertexBufferOffset) override {
        SetupState(ctx);
        SetupInputLayout(ctx, vertexBufferOffset);
        
        ShaderT shader;
        InjectUniforms(shader, uniformData);
        InjectResources(shader, ctx);

        // SoftRenderContext::glDrawElements expects indices type. 
        // We assume GL_UNSIGNED_INT for now as per tinygl standard.
        // offset is bytes, so we cast to void*
        void* offset = (void*)(uintptr_t)(cmd.drawIndexed.firstIndex * sizeof(uint32_t));
        
        ctx.glDrawElements(shader, GL_TRIANGLES, cmd.drawIndexed.indexCount, GL_UNSIGNED_INT, offset);
        
        RestoreState(ctx);
    }

private:
    void SetupInputLayout(SoftRenderContext& ctx, uint32_t baseOffset) {
        // If no attributes defined, do nothing (or fallback)
        if (desc.inputLayout.attributes.empty()) return;

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
            
            // Calculate absolute offset
            // NOTE: SoftRender expects void* for offset
            void* pointer = (void*)(uintptr_t)(baseOffset + attr.offset);
            
            ctx.glVertexAttribPointer(attr.shaderLocation, size, type, normalized, desc.inputLayout.stride, pointer);
            ctx.glEnableVertexAttribArray(attr.shaderLocation);
        }
    }

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
