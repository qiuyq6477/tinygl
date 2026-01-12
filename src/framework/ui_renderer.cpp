#include <vector>
#include <tinygl/base/log.h>
#include <framework/ui_renderer.h>
#include <framework/atlas.inl> 
#include <rhi/shader_registry.h>
#include <tinygl/base/tmath.h>
#include <tinygl/core/gl_texture.h>

#include <iostream>
#include <algorithm>
#include <cstring>
#include <array>

namespace framework {

using namespace rhi;
using namespace tinygl;

// --- Internal Data Structures ---

struct UIVertex {
    float pos[2];
    float uv[2];
    uint32_t color; // Packed RGBA
};

struct UIState {
    IGraphicsDevice* device = nullptr;
    PipelineHandle pipeline;
    BufferHandle vbo;
    BufferHandle ibo;
    TextureHandle texture;
    ShaderHandle shader;
    
    // Transient build data
    std::vector<UIVertex> vertices;
    std::vector<uint32_t> indices;
    
    // Batch tracking
    struct Batch {
        uint32_t indexOffset;
        uint32_t indexCount;
        mu_Rect clip;
    };
    std::vector<Batch> batches;
};

static UIState s_ui;

// --- Helpers ---

static uint32_t PackColor(mu_Color color) {
    return (color.a << 24) | (color.b << 16) | (color.g << 8) | color.r;
}

static void PushQuad(mu_Rect dst, mu_Rect src, mu_Color color) {
    // UV Coordinates (Normalize)
    float u0 = src.x / (float)ATLAS_WIDTH;
    float v0 = src.y / (float)ATLAS_HEIGHT;
    float u1 = (src.x + src.w) / (float)ATLAS_WIDTH;
    float v1 = (src.y + src.h) / (float)ATLAS_HEIGHT;

    // Vertices
    uint32_t col = PackColor(color);
    uint32_t baseIdx = (uint32_t)s_ui.vertices.size();

    // Top-Left
    s_ui.vertices.push_back({(float)dst.x, (float)dst.y, u0, v0, col});
    // Top-Right
    s_ui.vertices.push_back({(float)(dst.x + dst.w), (float)dst.y, u1, v0, col});
    // Bottom-Right
    s_ui.vertices.push_back({(float)(dst.x + dst.w), (float)(dst.y + dst.h), u1, v1, col});
    // Bottom-Left
    s_ui.vertices.push_back({(float)dst.x, (float)(dst.y + dst.h), u0, v1, col});

    // Indices (Two Triangles: 0-1-2, 0-2-3)
    // Vertices: 0:TL, 1:TR, 2:BR, 3:BL
    s_ui.indices.push_back(baseIdx + 0);
    s_ui.indices.push_back(baseIdx + 3); // BL
    s_ui.indices.push_back(baseIdx + 1); // TR
    
    s_ui.indices.push_back(baseIdx + 1); // TR
    s_ui.indices.push_back(baseIdx + 3); // BL
    s_ui.indices.push_back(baseIdx + 2); // BR
}

// --- Shader Definition ---

struct UIShader : public tinygl::ShaderBuiltins {
    // Uniforms
    Mat4 projection;
    
    // Resources
    const TextureObject* texture = nullptr;

    void BindUniforms(const std::vector<uint8_t>& data) {
        if (data.size() >= sizeof(Mat4)) {
            std::memcpy(&projection, data.data(), sizeof(Mat4));
        }
    }
    
    void BindResources(SoftRenderContext& ctx) {
        texture = ctx.getTexture(0);
    }
    
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
        // attribs[0]: pos (xy)
        // attribs[1]: uv (xy)
        // attribs[2]: color (normalized float4)

        Vec4 pos = attribs[0];
        pos.z = 0.0f;
        pos.w = 1.0f;

        gl_Position = projection * pos;
        
        ctx.varyings[0] = attribs[2]; // Color
        ctx.varyings[1] = attribs[1]; // UV
    }

    void fragment(const ShaderContext& ctx) {
        Vec4 color = ctx.varyings[0];
        Vec4 uv = ctx.varyings[1];
        
        // Sample texture 0 (Atlas is R8)
        float alpha = 1.0f;
        if (texture) {
            Vec4 texColor = texture->sample(uv.x, uv.y, ctx.rho);
            // Alpha is in Red channel for R8 texture
            alpha = texColor.x;
        }
        
        gl_FragColor = color;
        gl_FragColor.w *= alpha;
    }
};

static const std::string s_vsGLSL = R"(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aUV;
layout(location = 2) in vec4 aColor;

layout(std140) uniform Globals {
    mat4 projection;
};

out vec4 vColor;
out vec2 vUV;

void main() {
    gl_Position = projection * vec4(aPos, 0.0, 1.0);
    vColor = aColor;
    vUV = aUV;
}
)";

static const std::string s_fsGLSL = R"(
#version 330 core
in vec4 vColor;
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uTexture;

void main() {
    float alpha = texture(uTexture, vUV).r;
    FragColor = vColor * vec4(1, 1, 1, alpha);
}
)";

// --- Implementation ---

int UIRenderer::textWidth(mu_Font font, const char *text, int len) {
    int res = 0;
    for (const char *p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        int chr = mu_min((unsigned char)*p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}

int UIRenderer::textHeight(mu_Font font) {
    return 18;
}

void UIRenderer::init(mu_Context* ctx, IGraphicsDevice* device) {
    s_ui.device = device;
    
    // 1. Init MicroUI
    mu_init(ctx);
    ctx->text_width = UIRenderer::textWidth;
    ctx->text_height = UIRenderer::textHeight;

    // 2. Create Atlas Texture (R8)
    // ATLAS_WIDTH/HEIGHT are 128x128
    s_ui.texture = device->CreateTexture(atlas_texture, ATLAS_WIDTH, ATLAS_HEIGHT, 1);

    // 3. Register Shader
    ShaderDesc desc;
    desc.softFactory = [](SoftRenderContext& ctx, const PipelineDesc& pDesc) {
        return std::make_unique<SoftPipeline<UIShader>>(ctx, pDesc);
    };
    desc.glsl.vertex = s_vsGLSL;
    desc.glsl.fragment = s_fsGLSL;
    
    // Check if already registered
    if (!ShaderRegistry::GetInstance().IsRegistered("UIShader")) {
        s_ui.shader = ShaderRegistry::GetInstance().Register("UIShader", desc);
    } else {
        s_ui.shader = ShaderRegistry::GetInstance().GetShader("UIShader");
    }

    // 4. Create Pipeline
    PipelineDesc pDesc;
    pDesc.shader = s_ui.shader;
    pDesc.primitiveType = PrimitiveType::Triangles;
    pDesc.cullMode = CullMode::None;
    pDesc.depthTestEnabled = false;
    pDesc.depthWriteEnabled = false;
    
    // Blending
    pDesc.blend.enabled = true;
    pDesc.blend.srcRGB = BlendFactor::SrcAlpha;
    pDesc.blend.dstRGB = BlendFactor::OneMinusSrcAlpha;
    pDesc.blend.srcAlpha = BlendFactor::SrcAlpha;
    pDesc.blend.dstAlpha = BlendFactor::OneMinusSrcAlpha;
    
    // Input Layout
    pDesc.useInterleavedAttributes = true;
    pDesc.inputLayout.stride = sizeof(UIVertex);
    pDesc.inputLayout.attributes = {
        {VertexFormat::Float2, offsetof(UIVertex, pos), 0},
        {VertexFormat::Float2, offsetof(UIVertex, uv), 1},
        {VertexFormat::UByte4N, offsetof(UIVertex, color), 2}
    };
    
    s_ui.pipeline = device->CreatePipeline(pDesc);

    // 5. Create Buffers (Dynamic)
    BufferDesc vboDesc;
    vboDesc.type = BufferType::VertexBuffer;
    vboDesc.usage = BufferUsage::Stream; // Changed often
    vboDesc.size = 128 * 1024; // 128KB initial
    s_ui.vbo = device->CreateBuffer(vboDesc);

    BufferDesc iboDesc;
    iboDesc.type = BufferType::IndexBuffer;
    iboDesc.usage = BufferUsage::Stream;
    iboDesc.size = 128 * 1024;
    s_ui.ibo = device->CreateBuffer(iboDesc);
}

void UIRenderer::shutdown() {
    if (s_ui.device) {
        s_ui.device->DestroyBuffer(s_ui.vbo);
        s_ui.device->DestroyBuffer(s_ui.ibo);
        s_ui.device->DestroyTexture(s_ui.texture);
        s_ui.device->DestroyPipeline(s_ui.pipeline);
        s_ui.device = nullptr;
    }
}

void UIRenderer::processInput(mu_Context* ctx, const SDL_Event& e) {
    switch (e.type) {
        case SDL_MOUSEMOTION:
            mu_input_mousemove(ctx, e.motion.x, e.motion.y);
            break;
        case SDL_MOUSEWHEEL:
            mu_input_scroll(ctx, 0, e.wheel.y * -30);
            break;
        case SDL_TEXTINPUT:
            mu_input_text(ctx, e.text.text);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int b = 0;
            switch (e.button.button) {
                case SDL_BUTTON_LEFT:   b = MU_MOUSE_LEFT; break;
                case SDL_BUTTON_RIGHT:  b = MU_MOUSE_RIGHT; break;
                case SDL_BUTTON_MIDDLE: b = MU_MOUSE_MIDDLE; break;
            }
            if (b) {
                if (e.type == SDL_MOUSEBUTTONDOWN) mu_input_mousedown(ctx, e.button.x, e.button.y, b);
                else mu_input_mouseup(ctx, e.button.x, e.button.y, b);
            }
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            int key = e.key.keysym.sym;
            int mu_key = 0;
            
            switch (key) {
                case SDLK_LSHIFT:    
                case SDLK_RSHIFT:    mu_key = MU_KEY_SHIFT; break;
                case SDLK_LCTRL:     
                case SDLK_RCTRL:     mu_key = MU_KEY_CTRL; break;
                case SDLK_LALT:      
                case SDLK_RALT:      mu_key = MU_KEY_ALT; break;
                case SDLK_RETURN:    mu_key = MU_KEY_RETURN; break;
                case SDLK_BACKSPACE: mu_key = MU_KEY_BACKSPACE; break;
            }
            
            if (mu_key) {
                if (e.type == SDL_KEYDOWN) mu_input_keydown(ctx, mu_key);
                else mu_input_keyup(ctx, mu_key);
            }
            break;
        }
    }
}

void UIRenderer::render(mu_Context* ctx, CommandEncoder& encoder, int width, int height) {
    if (!s_ui.device) return;

    mu_Command* cmd = NULL;
    s_ui.vertices.clear();
    s_ui.indices.clear();
    s_ui.batches.clear();

    // Default clip is full screen
    mu_Rect currentClip = mu_rect(0, 0, width, height); 
    
    // Start first batch
    s_ui.batches.push_back({0, 0, currentClip});

    while (mu_next_command(ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT: {
                 mu_Rect src = atlas[ATLAS_FONT + 0]; 
                 int x = cmd->text.pos.x;
                 int y = cmd->text.pos.y;
                 for (const char* p = cmd->text.str; *p; ++p) {
                    if ((*p & 0xc0) == 0x80) continue;
                    int chr = mu_min((unsigned char)*p, 127);
                    src = atlas[ATLAS_FONT + chr];
                    mu_Rect dst = {x, y, src.w, src.h};
                    PushQuad(dst, src, cmd->text.color);
                    x += src.w;
                 }
                break;
            }
            case MU_COMMAND_RECT:
                PushQuad(cmd->rect.rect, atlas[ATLAS_WHITE], cmd->rect.color);
                break;
            case MU_COMMAND_ICON: {
                mu_Rect src = atlas[cmd->icon.id];
                int x = cmd->icon.rect.x + (cmd->icon.rect.w - src.w) / 2;
                int y = cmd->icon.rect.y + (cmd->icon.rect.h - src.h) / 2;
                mu_Rect dst = {x, y, src.w, src.h};
                PushQuad(dst, src, cmd->icon.color);
                break;
            }
            case MU_COMMAND_CLIP: {
                mu_Rect newClip = cmd->clip.rect;
                if (std::memcmp(&newClip, &currentClip, sizeof(mu_Rect)) != 0) {
                    // Break Batch
                    
                    // Close previous
                    auto& last = s_ui.batches.back();
                    last.indexCount = (uint32_t)s_ui.indices.size() - last.indexOffset;
                    
                    if (last.indexCount == 0) {
                        // Previous was empty, just update clip
                        last.clip = newClip;
                    } else {
                        // Start new
                        s_ui.batches.push_back({(uint32_t)s_ui.indices.size(), 0, newClip});
                    }
                    currentClip = newClip;
                }
                break;
            }
        }
    }

    // Close final batch
    if (!s_ui.batches.empty()) {
        s_ui.batches.back().indexCount = (uint32_t)s_ui.indices.size() - s_ui.batches.back().indexOffset;
    }

    // --- Submit to RHI ---

    if (s_ui.vertices.empty()) return;

    size_t vSize = s_ui.vertices.size() * sizeof(UIVertex);
    size_t iSize = s_ui.indices.size() * sizeof(uint32_t);

    if (vSize > 128 * 1024) {
        // Simple safe guard: truncate
        std::cerr << "UI Buffer Overflow: too many vertices." << std::endl;
        return; 
    }

    s_ui.device->UpdateBuffer(s_ui.vbo, s_ui.vertices.data(), vSize);
    s_ui.device->UpdateBuffer(s_ui.ibo, s_ui.indices.data(), iSize);

    // Setup Pipeline
    encoder.SetPipeline(s_ui.pipeline);
    encoder.SetVertexStream(0, s_ui.vbo, 0, sizeof(UIVertex));
    encoder.SetIndexBuffer(s_ui.ibo);
    encoder.SetTexture(0, s_ui.texture);
    
    // Ortho Matrix
    // Left=0, Right=width, Bottom=height, Top=0, Near=-1, Far=1
    // Row-Major:
    // 2/w  0    0   0
    // 0    -2/h 0   0
    // 0    0    -1  0
    // -1   1    0   1
    
    Mat4 ortho = Mat4::Identity();
    ortho.m[0] = 2.0f / width;
    ortho.m[5] = -2.0f / height;
    ortho.m[10] = -1.0f; 
    ortho.m[12] = -1.0f;
    ortho.m[13] = 1.0f;
    
    encoder.UpdateUniform(0, ortho); 

    for (const auto& batch : s_ui.batches) {
        if (batch.indexCount > 0) {
            // Correct Scissor Y for OpenGL (Lower-Left origin)
            int glY = height - (batch.clip.y + batch.clip.h);
            encoder.SetScissor(batch.clip.x, glY, batch.clip.w, batch.clip.h);
            
            encoder.DrawIndexed(batch.indexCount, batch.indexOffset, 0);
        }
    }
    
    encoder.SetScissor(-1, -1, -1, -1); // Disable scissor
}

}