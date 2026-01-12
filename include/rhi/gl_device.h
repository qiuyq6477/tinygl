#pragma once
#include <rhi/device.h>
#include <rhi/command_buffer.h>
#include <SDL2/SDL.h>
#include <tinygl/core/gl_defs.h>
#include <unordered_map>
#include <array>
#include <vector>

namespace rhi {

class TINYGL_API GLDevice : public IGraphicsDevice {
public:
    explicit GLDevice();
    ~GLDevice() override;

    BufferHandle CreateBuffer(const BufferDesc& desc) override;
    void DestroyBuffer(BufferHandle handle) override;
    void UpdateBuffer(BufferHandle handle, const void* data, size_t size, size_t offset) override;

    TextureHandle CreateTexture(const void* pixelData, int width, int height, int channels) override;
    void DestroyTexture(TextureHandle handle) override;

    PipelineHandle CreatePipeline(const PipelineDesc& desc) override;
    void DestroyPipeline(PipelineHandle handle) override;

    void Submit(const CommandBuffer& buffer) override;
    void Present() override;

private:
    using GLuint = tinygl::GLuint;
    using GLenum = tinygl::GLenum;

    struct BufferMeta {
        GLuint id;
        GLenum target;
    };

    struct PipelineMeta {
        GLuint program;
        GLuint vao;
        PipelineDesc desc;
    };

    struct BindingState {
        uint32_t bufferId = 0;
        uint32_t offset = 0;
        uint32_t stride = 0;
    };

    struct GLState {
        GLuint boundVBO = 0;
        GLuint boundIBO = 0;
        GLuint boundVAO = 0;
        GLuint boundProgram = 0;
        GLuint boundTexture = 0;
    };

    void BindBuffer(GLenum target, GLuint id);
    void BindVertexArray(GLuint id);
    void UseProgram(GLuint id);

    std::unordered_map<uint32_t, BufferMeta> m_buffers;
    std::unordered_map<uint32_t, GLuint> m_textures;
    std::unordered_map<uint32_t, PipelineMeta> m_pipelines;
    std::unordered_map<uint32_t, GLuint> m_shaderPrograms;

    uint32_t m_nextBufferHandle = 1;
    uint32_t m_nextTextureHandle = 1;
    uint32_t m_nextPipelineHandle = 1;

    GLuint m_globalUBO = 0;
    static constexpr size_t UNIFORM_STAGING_SIZE = 16 * 256;
    uint8_t m_uniformStaging[UNIFORM_STAGING_SIZE];
    bool m_uniformsDirty = false;

    static constexpr int MAX_BINDINGS = 8;
    BindingState m_bindings[MAX_BINDINGS];
    uint32_t m_activeIBO = 0;

    GLState m_state;
};

}