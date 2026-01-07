#pragma once

#include <tinygl/core/tinygl.h>
#include <tinygl/framework/render_defs.h>
#include <tinygl/framework/mesh.h> 
#include <iostream>
#include <concepts>

namespace tinygl {

// Concept: 定义一个合法的 TinyGL Shader 必须满足的条件
template <typename T>
concept TinyGLShader = std::derived_from<T, ShaderBuiltins> && requires(T shader, const Vec4* attribs, ShaderContext& ctx) {
    { shader.vertex(attribs, ctx) } -> std::same_as<void>;
    { shader.fragment(ctx) } -> std::same_as<void>;
};

// 抽象基类：允许 Mesh 在不知道具体 Shader 类型的情况下发起绘制
class IShaderPass {
public:
    virtual ~IShaderPass() = default;
    
    // Draw 接口：接收上下文、网格数据、变换矩阵和全局状态
    virtual void Draw(SoftRenderContext& ctx, const Mesh& mesh, const Mat4& modelMatrix, const RenderState& state) = 0;
};

// 模板实现类：将运行时调用桥接到编译时模板
// 使用 Concept 约束 ShaderT
template <TinyGLShader ShaderT>
class ShaderPass : public IShaderPass {
public:
    void Draw(SoftRenderContext& ctx, const Mesh& mesh, const Mat4& modelMatrix, const RenderState& state) override {
        // 1. 在栈上实例化具体的 Shader (零开销)
        ShaderT shader;
        
        // 2. 绑定全局 Uniforms (需要 ShaderT 具有对应的成员变量)
        if constexpr (requires { shader.model = modelMatrix; }) shader.model = modelMatrix;
        if constexpr (requires { shader.view = state.view; }) shader.view = state.view;
        if constexpr (requires { shader.projection = state.projection; }) shader.projection = state.projection;
        if constexpr (requires { shader.viewPos = state.viewPos; }) shader.viewPos = state.viewPos;
        if constexpr (requires { shader.lightPos = state.lightPos; }) shader.lightPos = state.lightPos;
        if constexpr (requires { shader.lightColor = state.lightColor; }) shader.lightColor = state.lightColor;
        
        // 注入 Context (Hack for Texture Sampling in Fragment Shader)
        if constexpr (requires { shader.renderCtx = &ctx; }) {
            shader.renderCtx = &ctx;
        }

        // 3. 绑定 Material 数据 (Material -> Shader)
        // 利用 mesh.material 将数据传递给 shader.material
        if constexpr (requires { shader.material = mesh.material; }) {
            shader.material = mesh.material;
        }

        // 4. 处理双面渲染状态
        bool wasCullEnabled = ctx.glIsEnabled(GL_CULL_FACE);
        if (mesh.material.doubleSided && wasCullEnabled) {
            ctx.glDisable(GL_CULL_FACE);
        }

        // 5. 绑定纹理到 Context (Mesh::Draw 逻辑下沉到这里)
        const auto& mat = mesh.material;
        for (size_t i = 0; i < mat.textures.size(); ++i) {
            if (mat.textures[i] != 0) {
                ctx.glActiveTexture(GL_TEXTURE0 + static_cast<GLenum>(i));
                ctx.glBindTexture(GL_TEXTURE_2D, mat.textures[i]);
            }
        }

        // 6. 执行绘制核心 (完全内联)
        ctx.glBindVertexArray(mesh.GetVAO());
        ctx.glDrawElements(shader, GL_TRIANGLES, static_cast<GLsizei>(mesh.indices.size()), GL_UNSIGNED_INT, 0);
        
        // 7. 恢复状态
        ctx.glBindVertexArray(0);
        if (mesh.material.doubleSided && wasCullEnabled) {
            ctx.glEnable(GL_CULL_FACE);
        }
    }
};

}
