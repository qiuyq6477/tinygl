#include "tinygl/framework/ui_renderer.h"
#include <tinygl/base/log.h>
#include <vector>
#include "tinygl/framework/atlas.inl" 

#include <iostream>
#include <algorithm>
#include <cstring>

namespace tinygl {

static const char button_map[256] = {
  [ SDL_BUTTON_LEFT   & 0xff ] =  MU_MOUSE_LEFT,
  [ SDL_BUTTON_RIGHT  & 0xff ] =  MU_MOUSE_RIGHT,
  [ SDL_BUTTON_MIDDLE & 0xff ] =  MU_MOUSE_MIDDLE,
};

static const char key_map[256] = {
  [ SDLK_LSHIFT       & 0xff ] = MU_KEY_SHIFT,
  [ SDLK_RSHIFT       & 0xff ] = MU_KEY_SHIFT,
  [ SDLK_LCTRL        & 0xff ] = MU_KEY_CTRL,
  [ SDLK_RCTRL        & 0xff ] = MU_KEY_CTRL,
  [ SDLK_LALT         & 0xff ] = MU_KEY_ALT,
  [ SDLK_RALT         & 0xff ] = MU_KEY_ALT,
  [ SDLK_RETURN       & 0xff ] = MU_KEY_RETURN,
  [ SDLK_BACKSPACE    & 0xff ] = MU_KEY_BACKSPACE,
};

/*
 * 参数:
 * text: 指向 C 字符串的指针，表示要计算宽度的文本。
 * len: 要处理的文本的最大长度。这允许你计算部分字符串的宽度。
 * 功能: 它遍历文本中的每个字符，查找该字符在 atlas（纹理图集）中对应的宽度，并将这些宽度累加起来。对于多字节 UTF-8 字符（尽管这里简单地跳过，只处理 ASCII 字符），它会跳过后续字节。
 * 返回值: 文本的总像素宽度。
 */
int UIRenderer::textWidth(mu_Font font, const char *text, int len)
{
  int res = 0;
  // 遍历文本中的每个字符，计算其在纹理图集中的宽度并累加
  for (const char *p = text; *p && len--; p++)
  {
    // 跳过 UTF-8 编码的多字节字符的后续字节（这里假设只处理 ASCII 字符或单字节字符）
    if ((*p & 0xc0) == 0x80)
    {
      continue;
    }
    // 获取字符的 ASCII 值，并限制在 0-127 范围内，以对应图集中的字体索引
    int chr = mu_min((unsigned char)*p, 127);
    // 累加字符在图集中的宽度
    res += atlas[ATLAS_FONT + chr].w;
  }
  return res;
}

/*
这个函数返回文本的固定行高。
* 功能: 在这个渲染器中，字体的高度是预设的固定值 18 像素。这个函数直接返回这个值，而不是根据实际字符计算高度，因为 MicroUI 库中的文本通常是单行且固定高度的。
* 返回值: 文本的固定像素高度。
*/
int UIRenderer::textHeight(mu_Font font) {
    return 18;
}

void UIRenderer::init(mu_Context* ctx) {
    mu_init(ctx);
    ctx->text_width = UIRenderer::textWidth;
    ctx->text_height = UIRenderer::textHeight;
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
            if (e.button.button == SDL_BUTTON_LEFT) b = MU_MOUSE_LEFT;
            else if (e.button.button == SDL_BUTTON_RIGHT) b = MU_MOUSE_RIGHT;
            else if (e.button.button == SDL_BUTTON_MIDDLE) b = MU_MOUSE_MIDDLE;
            if (b) {
                if (e.type == SDL_MOUSEBUTTONDOWN) mu_input_mousedown(ctx, e.button.x, e.button.y, b);
                else mu_input_mouseup(ctx, e.button.x, e.button.y, b);
            }
            break;
        }
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            int key = e.key.keysym.sym & 0xff;
            if (key < 256 && key_map[key]) {
                if (e.type == SDL_KEYDOWN) mu_input_keydown(ctx, key_map[key]);
                else mu_input_keyup(ctx, key_map[key]);
            }
            break;
        }
    }
}

static inline uint32_t blendColor(uint32_t dst, mu_Color color) {
    if (color.a == 0) return dst;
    
    // Fast path for opaque
    if (color.a == 255) {
        return (255 << 24) | (color.b << 16) | (color.g << 8) | color.r;
    }

    uint32_t dstR = (dst >> 0) & 0xFF;
    uint32_t dstG = (dst >> 8) & 0xFF;
    uint32_t dstB = (dst >> 16) & 0xFF;
    
    float alpha = color.a / 255.0f;
    float invA = 1.0f - alpha;

    uint32_t r = (uint32_t)(color.r * alpha + dstR * invA);
    uint32_t g = (uint32_t)(color.g * alpha + dstG * invA);
    uint32_t b = (uint32_t)(color.b * alpha + dstB * invA);

    return (255 << 24) | (b << 16) | (g << 8) | r;
}

static void draw_quad(uint32_t* buf, int fbW, int fbH, mu_Rect dst, mu_Rect src, mu_Color color, mu_Rect clip) {
    // Intersect dst with clip
    int x1 = std::max(dst.x, clip.x);
    int y1 = std::max(dst.y, clip.y);
    int x2 = std::min(dst.x + dst.w, clip.x + clip.w);
    int y2 = std::min(dst.y + dst.h, clip.y + clip.h);

    // Also clip against framebuffer
    x1 = std::max(x1, 0);
    y1 = std::max(y1, 0);
    x2 = std::min(x2, fbW);
    y2 = std::min(y2, fbH);

    if (x2 <= x1 || y2 <= y1) return;

    float u_scale = src.w / (float)dst.w;
    float v_scale = src.h / (float)dst.h;

    for (int y = y1; y < y2; ++y) {
        uint32_t* row = buf + y * fbW;
        int tex_y_base = src.y + (int)((y - dst.y) * v_scale);
        // Clamp tex_y
        tex_y_base = std::clamp(tex_y_base, 0, ATLAS_HEIGHT - 1);

        for (int x = x1; x < x2; ++x) {
            int tex_x = src.x + (int)((x - dst.x) * u_scale);
            // Clamp tex_x
            tex_x = std::clamp(tex_x, 0, ATLAS_WIDTH - 1);

            // Sample atlas
            uint8_t alpha = atlas_texture[tex_y_base * ATLAS_WIDTH + tex_x];
            
            // Apply mask
            mu_Color finalColor = color;
            finalColor.a = (uint8_t)((int)color.a * alpha / 255);

            if (finalColor.a > 0) {
                row[x] = blendColor(row[x], finalColor);
            }
        }
    }
}

void UIRenderer::render(mu_Context* ctx, SoftRenderContext& renderCtx) {
    mu_Command* cmd = NULL;
    uint32_t* buf = renderCtx.getColorBuffer();
    int fbW = renderCtx.getWidth();
    int fbH = renderCtx.getHeight();
    mu_Rect current_clip = mu_rect(0, 0, fbW, fbH);
    
    if (!buf) return;

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
                    draw_quad(buf, fbW, fbH, dst, src, cmd->text.color, current_clip);
                    x += src.w;
                 }
                break;
            }
            case MU_COMMAND_RECT:
                draw_quad(buf, fbW, fbH, cmd->rect.rect, atlas[ATLAS_WHITE], cmd->rect.color, current_clip);
                break;
            case MU_COMMAND_ICON: {
                mu_Rect src = atlas[cmd->icon.id];
                int x = cmd->icon.rect.x + (cmd->icon.rect.w - src.w) / 2;
                int y = cmd->icon.rect.y + (cmd->icon.rect.h - src.h) / 2;
                mu_Rect dst = {x, y, src.w, src.h};
                draw_quad(buf, fbW, fbH, dst, src, cmd->icon.color, current_clip);
                break;
            }
            case MU_COMMAND_CLIP:
                current_clip = cmd->clip.rect;
                break;
        }
    }
}

}
