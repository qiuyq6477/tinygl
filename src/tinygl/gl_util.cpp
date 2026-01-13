#include <tinygl/tinygl.h>

namespace tinygl {

VOut SoftRenderContext::lerpVertex(const VOut& a, const VOut& b, float t) {
    VOut res;
    // 1. 插值位置 (Clip Space)
    res.pos = a.pos * (1.0f - t) + b.pos * t;
    
    // 2. 插值 Varyings (颜色、UV等)
    // 注意：这里的插值是线性的，但在投影后是不正确的。
    // 不过由于我们是在 Clip Space (4D) 进行裁剪，还未进行透视除法，
    // 所以直接线性插值属性是数学上正确的 (Rational Linear Interpolation)。
    for (int i = 0; i < MAX_VARYINGS; ++i) {
        res.ctx.varyings[i] = a.ctx.varyings[i] * (1.0f - t) + b.ctx.varyings[i] * t;
    }
    return res;
}


const std::vector<uint32_t>& SoftRenderContext::readIndicesAsInts(GLsizei count, GLenum type, const void* indices_ptr) {
    m_indexCache.clear();
    m_indexCache.reserve(count);
    size_t idxOffset = reinterpret_cast<size_t>(indices_ptr);
    
    VertexArrayObject& vao = getVAO();
    if (vao.elementBufferID == 0) {
        LOG_ERROR("No EBO bound to current VAO for glDrawElements");
        return m_indexCache;
    }

    switch (type) {
        case GL_UNSIGNED_INT: {
            for(int i=0; i<count; ++i) {
                uint32_t index;
                buffers.get(vao.elementBufferID)->readSafe<uint32_t>(idxOffset + i*sizeof(uint32_t), index);
                m_indexCache.push_back(index);
            }
            break;
        }
        case GL_UNSIGNED_SHORT: {
            for(int i=0; i<count; ++i) {
                uint16_t index;
                buffers.get(vao.elementBufferID)->readSafe<uint16_t>(idxOffset + i*sizeof(uint16_t), index);
                m_indexCache.push_back(static_cast<uint32_t>(index));
            }
            break;
        }
        case GL_UNSIGNED_BYTE: {
                for(int i=0; i<count; ++i) {
                uint8_t index;
                buffers.get(vao.elementBufferID)->readSafe<uint8_t>(idxOffset + i*sizeof(uint8_t), index);
                m_indexCache.push_back(static_cast<uint32_t>(index));
            }
            break;
        }
        default:
            LOG_ERROR("Unsupported index type in glDrawElements");
            return m_indexCache;
    }
    return m_indexCache;
}


// Helper: convertToInternalFormat
bool SoftRenderContext::convertToInternalFormat(const void* src_data, GLsizei src_width, GLsizei src_height,
                                GLenum src_format, GLenum src_type, std::vector<uint32_t>& dst_pixels)
{
    dst_pixels.resize(src_width * src_height);
    if (!src_data) return true; // Just resize, don't fill if no source data

    const uint8_t* src_bytes = static_cast<const uint8_t*>(src_data);
    size_t pixel_count = src_width * src_height;

    if (src_type == GL_UNSIGNED_BYTE) {
        if (src_format == GL_RGBA) {
            // Direct copy for RGBA8888
            std::memcpy(dst_pixels.data(), src_data, pixel_count * 4);
            return true;
        } else if (src_format == GL_RGB) {
            // Convert RGB888 to RGBA8888
            for (size_t i = 0; i < pixel_count; ++i) {
                uint8_t r = src_bytes[i * 3 + 0];
                uint8_t g = src_bytes[i * 3 + 1];
                uint8_t b = src_bytes[i * 3 + 2];
                dst_pixels[i] = (0xFF << 24) | (b << 16) | (g << 8) | r; // AABBGGRR
            }
            return true;
        } else if (src_format == GL_RED) {
             for (size_t i = 0; i < pixel_count; ++i) {
                uint8_t r = src_bytes[i];
                dst_pixels[i] = (0xFF << 24) | (0 << 16) | (0 << 8) | r; // AABBGGRR
            }
            return true;
        } else {
            LOG_ERROR("Unsupported source format with GL_UNSIGNED_BYTE type.");
            return false;
        }
    } else {
        LOG_ERROR("Unsupported source type for pixel conversion.");
        return false;
    }
}


Gradients SoftRenderContext::calcGradients(const VOut& v0, const VOut& v1, const VOut& v2, float invArea, float f0, float f1, float f2) {
    float temp0 = f1 - f0;
    float temp1 = f2 - f0;
    float dfdx = (temp0 * (v2.scn.y - v0.scn.y) - temp1 * (v1.scn.y - v0.scn.y)) * invArea;
    float dfdy = (temp1 * (v1.scn.x - v0.scn.x) - temp0 * (v2.scn.x - v0.scn.x)) * invArea;
    return {dfdx, dfdy};
}

void SoftRenderContext::transformToScreen(VOut& v) {
    float w = v.pos.w;
    // 此时 w 已经被 Near Plane 裁剪过，一定 > EPSILON
    float rhw = 1.0f / w;

    // Viewport Mapping: [-1, 1] -> [0, Width/Height]
    // X 轴: (ndc + 1) * 0.5 * w + x
    // Y 轴: y + (1 - ndc) * 0.5 * h  (注意：这里假设屏幕原点在左上角)
    v.scn.x = m_viewport.x + (v.pos.x * rhw + 1.0f) * 0.5f * m_viewport.w;
    v.scn.y = m_viewport.y + (1.0f - v.pos.y * rhw) * 0.5f * m_viewport.h;
    // Map Z_ndc [-1, 1] to Window Depth [0, 1]
    v.scn.z = (v.pos.z * rhw) * 0.5f + 0.5f;
    v.scn.w = rhw; // 存储 1/w 用于透视插值
}

}