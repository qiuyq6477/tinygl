#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstring> 
#include <cstdint>
#include <cmath>
#include <functional>
#include <algorithm>
#include <string>
#include <memory>
#include <iomanip>
#include <random>
#include <arm_neon.h>
#include "tmath.h"
#include "math_simd.h"
#include "color.h"
#include "colors.h"
#include "log.h"
#include "container.h"
#include "shader.h"

#define STB_IMAGE_IMPLEMENTATION
#include "third_party/stb_image.h"

#include "core/gl_defs.h"

#include "core/texture.h"
#include "core/vao.h"

#include "core/buffer.h"

#include "core/shader_structs.h"
#include "core/geometry.h"
class SoftRenderContext {
private:
    std::unordered_map<GLuint, BufferObject> buffers;
    std::unordered_map<GLuint, VertexArrayObject> vaos;
    std::unordered_map<GLuint, TextureObject> textures;
    std::unordered_map<GLuint, ProgramObject> programs;

    GLuint m_boundArrayBuffer = 0;
    GLuint m_boundVertexArray = 0;
    GLuint m_currentProgram = 0;
    GLuint m_activeTextureUnit = 0;
    GLuint m_boundTextures[MAX_TEXTURE_UNITS] = {0};

    GLuint m_nextID = 1;
    GLsizei fbWidth = 800;
    GLsizei fbHeight = 600;
    
    std::vector<uint32_t> colorBuffer;
    uint32_t* m_colorBufferPtr = nullptr; // Pointer to the active color buffer (internal or external)

    std::vector<float> depthBuffer;
    std::vector<uint32_t> m_indexCache;
    Vec4 m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f}; // Default clear color is black

    Viewport m_viewport;
    GLenum m_polygonMode = GL_FILL; // 默认填充
public:
    SoftRenderContext(GLsizei width, GLsizei height) {
        fbWidth = width;
        fbHeight = height;
        // Viewport 初始化
        m_viewport = {0, 0, fbWidth, fbHeight};
        
        vaos[0] = VertexArrayObject{}; 
        colorBuffer.resize(fbWidth * fbHeight, COLOR_BLACK); // 黑色背景
        m_colorBufferPtr = colorBuffer.data();               // Default to internal buffer

        // 【Fix 1】: Depth 初始化为极大值，确保 z=1.0 的物体能通过测试
        depthBuffer.resize(fbWidth * fbHeight, DEPTH_INFINITY);       
        LOG_INFO("Context Initialized (" + std::to_string(fbWidth) + "x" + std::to_string(fbHeight) + ")");
    }

    // Set an external buffer for rendering (e.g., SDL texture memory)
    // Pass nullptr to revert to the internal buffer.
    void setExternalBuffer(uint32_t* ptr) {
        if (ptr) {
            m_colorBufferPtr = ptr;
        } else {
            m_colorBufferPtr = colorBuffer.data();
        }
    }

    void glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
        m_viewport.x = x;
        m_viewport.y = y;
        m_viewport.w = w;
        m_viewport.h = h;
    }

    // 设置多边形模式 API
    void glPolygonMode(GLenum face, GLenum mode) {
        // 暂时只支持 GL_FRONT_AND_BACK
        if (face == GL_FRONT_AND_BACK) {
            m_polygonMode = mode;
        }
    }

    // --- State Management ---
    void glClearColor(float r, float g, float b, float a) {
        m_clearColor = {r, g, b, a};
    }
    
    // Clear buffer function
    void glClear(uint32_t buffersToClear) {
        if (buffersToClear & BufferType::COLOR) {
            uint8_t R = (uint8_t)(std::clamp(m_clearColor.x, 0.0f, 1.0f) * 255);
            uint8_t G = (uint8_t)(std::clamp(m_clearColor.y, 0.0f, 1.0f) * 255);
            uint8_t B = (uint8_t)(std::clamp(m_clearColor.z, 0.0f, 1.0f) * 255);
            uint8_t A = (uint8_t)(std::clamp(m_clearColor.w, 0.0f, 1.0f) * 255);
            uint32_t clearColorInt = (A << 24) | (B << 16) | (G << 8) | R;
            // Use std::fill_n on the pointer
            std::fill_n(m_colorBufferPtr, fbWidth * fbHeight, clearColorInt);
        }
        if (buffersToClear & BufferType::DEPTH) {
            std::fill(depthBuffer.begin(), depthBuffer.end(), DEPTH_INFINITY);
        }
    }

    // Get color buffer for external display
    uint32_t* getColorBuffer() { return m_colorBufferPtr; }

    // --- Accessors ---
    VertexArrayObject& getVAO() { return vaos[m_boundVertexArray]; }
    
    ProgramObject* getCurrentProgram() {
        if (m_currentProgram == 0) return nullptr;
        auto it = programs.find(m_currentProgram);
        return (it != programs.end()) ? &it->second : nullptr;
    }
    
    TextureObject* getTexture(GLuint unit) {
        if (unit >= 32) return nullptr;
        GLuint id = m_boundTextures[unit];
        if (id == 0) return nullptr;
        auto it = textures.find(id);
        return (it != textures.end()) ? &it->second : nullptr;
    }
    
    // 直接通过 ID 获取纹理对象 (绕过 Texture Unit 绑定状态)
    // 这种方式更适合我们现在的 Template Shader 架构
    TextureObject* getTextureObject(GLuint id) {
        if (id == 0) return nullptr;
        auto it = textures.find(id);
        if (it != textures.end()) {
            return &it->second;
        }
        return nullptr;
    }

    // --- Buffers ---
    void glGenBuffers(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) {
            res[i] = m_nextID++;
            buffers[res[i]]; 
            LOG_INFO("GenBuffer ID: " + std::to_string(res[i]));
        }
    }

    void glBindBuffer(GLenum target, GLuint buffer) {
        if (target == GL_ARRAY_BUFFER) m_boundArrayBuffer = buffer;
        else if (target == GL_ELEMENT_ARRAY_BUFFER) getVAO().elementBufferID = buffer;
    }

    void glBufferData(GLenum target, GLsizei size, const void* data, GLenum usage) {
        GLuint id = (target == GL_ARRAY_BUFFER) ? m_boundArrayBuffer : getVAO().elementBufferID;
        if (id == 0 || buffers.find(id) == buffers.end()) {
            LOG_ERROR("Invalid Buffer Binding");
            return;
        }
        buffers[id].data.resize(size);
        if (data) std::memcpy(buffers[id].data.data(), data, size);
        buffers[id].usage = usage;
        LOG_INFO("BufferData " + std::to_string(size) + " bytes to ID " + std::to_string(id));
    }

    // --- VAO ---
    void glGenVertexArrays(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) {
            res[i] = m_nextID++;
            vaos[res[i]];
            LOG_INFO("GenVAO ID: " + std::to_string(res[i]));
        }
    }

    void glBindVertexArray(GLuint array) { m_boundVertexArray = array; }

    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* pointer) {
        if (index >= MAX_ATTRIBS) return;
        if (m_boundArrayBuffer == 0) {
            LOG_ERROR("No VBO bound!");
            return;
        }
        VertexAttribState& attr = getVAO().attributes[index];
        attr.bindingBufferID = m_boundArrayBuffer;
        attr.size = size;
        attr.type = type;
        attr.normalized = norm; // Store the normalized flag
        attr.stride = stride;
        attr.pointerOffset = reinterpret_cast<size_t>(pointer);
        LOG_INFO("Attrib " + std::to_string(index) + " bound to VBO " + std::to_string(m_boundArrayBuffer));
    }

    void glEnableVertexAttribArray(GLuint index) {
        if (index < MAX_ATTRIBS) getVAO().attributes[index].enabled = true;
    }

    void glVertexAttribDivisor(GLuint index, GLuint divisor) {
        if (index < MAX_ATTRIBS) {
            getVAO().attributes[index].divisor = divisor;
        }
    }

    // --- Textures (FIXED) ---
    void glGenTextures(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) {
            res[i] = m_nextID++;
            // 【Fix】: 立即创建 Texture 对象，防止后续 lookup 失败
            textures[res[i]].id = res[i]; 
            LOG_INFO("GenTexture ID: " + std::to_string(res[i]));
        }
    }

    void glActiveTexture(GLenum texture) {
        if (texture >= GL_TEXTURE0 && texture < GL_TEXTURE0 + 32) m_activeTextureUnit = texture - GL_TEXTURE0;
    }

    void glBindTexture(GLenum target, GLuint texture) {
        if (target == GL_TEXTURE_2D) {
            m_boundTextures[m_activeTextureUnit] = texture;
            // 【Robustness】: 如果绑定的 ID 合法但 Map 中不存在（某些用法允许Bind时创建），则补建
            if (texture != 0 && textures.find(texture) == textures.end()) {
                textures[texture].id = texture;
                LOG_INFO("Implicitly created Texture ID: " + std::to_string(texture));
            }
        }
    }

    void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* p) {
        auto* tex = getTexture(m_activeTextureUnit); if(!tex) return;

        // Phase 1: Basic Parameter Handling
        if (target != GL_TEXTURE_2D) {
            LOG_WARN("glTexImage2D: Only GL_TEXTURE_2D is supported for target.");
            return;
        }
        if (border != 0) {
            LOG_WARN("glTexImage2D: Border must be 0.");
            return;
        }
        if (internalformat != GL_RGBA) {
            LOG_WARN("glTexImage2D: Only GL_RGBA internalformat is fully supported for storage.");
            // Continue with GL_RGBA storage regardless
        }

        tex->width = w; 
        tex->height = h;

        // Resize levels vector if necessary to accommodate this level
        if (level >= static_cast<GLint>(tex->levels.size())) {
            tex->levels.resize(level + 1);
        }
        // Initialize this specific mipmap level
        tex->levels[level].resize(w * h);
        
        // Phase 2: Format and Type Conversion
        if (p) {
            if (!convertToInternalFormat(p, w, h, format, type, tex->levels[level])) {
                LOG_ERROR("glTexImage2D: Failed to convert source pixel data.");
                return;
            }
        }
    }

    // 设置纹理参数
    void glTexParameteri(GLenum target, GLenum pname, GLint param) {
        if (target != GL_TEXTURE_2D) return;
        TextureObject* tex = getTexture(m_activeTextureUnit);
        if (!tex) return;

        switch (pname) {
            case GL_TEXTURE_WRAP_S: tex->wrapS = (GLenum)param; break;
            case GL_TEXTURE_WRAP_T: tex->wrapT = (GLenum)param; break;
            case GL_TEXTURE_MIN_FILTER: tex->minFilter = (GLenum)param; break;
            case GL_TEXTURE_MAG_FILTER: tex->magFilter = (GLenum)param; break;
            case GL_TEXTURE_MIN_LOD: tex->minLOD = (float)param; break;
            case GL_TEXTURE_MAX_LOD: tex->maxLOD = (float)param; break;
            case GL_TEXTURE_LOD_BIAS: tex->lodBias = (float)param; break;
        }
    }
    // Float Scalar 版本
    void glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
        if (target != GL_TEXTURE_2D) return;
        TextureObject* tex = getTexture(m_activeTextureUnit);
        if (!tex) return;

        switch (pname) {
            case GL_TEXTURE_MIN_LOD: tex->minLOD = param; break;
            case GL_TEXTURE_MAX_LOD: tex->maxLOD = param; break;
            case GL_TEXTURE_LOD_BIAS: tex->lodBias = param; break;
            // 对于 Enum 类型的参数，允许用 float 传进来 (兼容性)
            case GL_TEXTURE_WRAP_S: tex->wrapS = (GLenum)param; break;
            case GL_TEXTURE_WRAP_T: tex->wrapT = (GLenum)param; break;
            case GL_TEXTURE_MIN_FILTER: tex->minFilter = (GLenum)param; break;
            case GL_TEXTURE_MAG_FILTER: tex->magFilter = (GLenum)param; break;
        }
    }
    // nt Vector 版本
    void glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
        if (target != GL_TEXTURE_2D || !params) return;
        TextureObject* tex = getTexture(m_activeTextureUnit);
        if (!tex) return;

        switch (pname) {
            case GL_TEXTURE_BORDER_COLOR:
                // 将 Int 颜色 (0-2147483647 或 0-255? OpenGL 这里的 int 实际上是映射到 0-1)
                // 标准规定：Int 值会线性映射。通常 0 -> 0.0, MAX_INT -> 1.0
                // 但为了简化软渲染器，我们假设用户传入的是 0-255 范围
                tex->borderColor = Vec4(params[0]/255.0f, params[1]/255.0f, params[2]/255.0f, params[3]/255.0f);
                break;
            default:
                glTexParameteri(target, pname, params[0]);
                break;
        }
    }

    // Float Vector 版本 (关键：Border Color)
    void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
        if (target != GL_TEXTURE_2D || !params) return;
        TextureObject* tex = getTexture(m_activeTextureUnit);
        if (!tex) return;

        switch (pname) {
            case GL_TEXTURE_BORDER_COLOR:
                tex->borderColor = Vec4(params[0], params[1], params[2], params[3]);
                break;
            default:
                // 如果用户传的是标量参数的指针形式
                glTexParameterf(target, pname, params[0]);
                break;
        }
    }
    // --- 裁剪与光栅化辅助结构 ---
    // 线性插值辅助函数 (Linear Interpolation)
    // 用于在被裁剪的边上生成新的顶点
    // t: [0, 1] 插值系数
    VOut lerpVertex(const VOut& a, const VOut& b, float t) {
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

    const std::vector<uint32_t>& readIndicesAsInts(GLsizei count, GLenum type, const void* indices_ptr) {
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
                    buffers[vao.elementBufferID].readSafe<uint32_t>(idxOffset + i*sizeof(uint32_t), index);
                    m_indexCache.push_back(index);
                }
                break;
            }
            case GL_UNSIGNED_SHORT: {
                for(int i=0; i<count; ++i) {
                    uint16_t index;
                    buffers[vao.elementBufferID].readSafe<uint16_t>(idxOffset + i*sizeof(uint16_t), index);
                    m_indexCache.push_back(static_cast<uint32_t>(index));
                }
                break;
            }
            case GL_UNSIGNED_BYTE: {
                 for(int i=0; i<count; ++i) {
                    uint8_t index;
                    buffers[vao.elementBufferID].readSafe<uint8_t>(idxOffset + i*sizeof(uint8_t), index);
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

    // Converts source pixel data to internal RGBA8888 format
    bool convertToInternalFormat(const void* src_data, GLsizei src_width, GLsizei src_height,
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
            } else {
                LOG_ERROR("Unsupported source format with GL_UNSIGNED_BYTE type.");
                return false;
            }
        } else {
            LOG_ERROR("Unsupported source type for pixel conversion.");
            return false;
        }
    }

    // Sutherland-Hodgman 裁剪算法的核心：针对单个平面进行裁剪
    // inputVerts: 输入的顶点列表
    // planeID: 0=Left, 1=Right, 2=Bottom, 3=Top, 4=Near, 5=Far
    StaticVector<VOut, 16> clipAgainstPlane(const StaticVector<VOut, 16>& inputVerts, int planeID) {
        StaticVector<VOut, 16> outputVerts;
        if (inputVerts.empty()) return outputVerts;

        // Lambda: 判断点是否在平面内 (Inside Test)
        // OpenGL Frustum Planes based on w:
        // Left:   w + x > 0 | Right:  w - x > 0
        // Bottom: w + y > 0 | Top:    w - y > 0
        // Near:   w + z > 0 | Far:    w - z > 0
        auto isInside = [&](const Vec4& p) {
            switch (planeID) {
                case 0: return p.w + p.x >= 0; // Left
                case 1: return p.w - p.x >= 0; // Right
                case 2: return p.w + p.y >= 0; // Bottom
                case 3: return p.w - p.y >= 0; // Top
                case 4: return p.w + p.z >= EPSILON; // Near (关键! 防止除以0)
                case 5: return p.w - p.z >= 0; // Far
                default: return true;
            }
        };

        // Lambda: 计算线段与平面的交点插值系数 t (Intersection)
        // 利用相似三角形原理: t = dist_in / (dist_in - dist_out)
        auto getIntersectT = [&](const Vec4& prev, const Vec4& curr) {
            float dp = 0, dc = 0; // Dist Prev, Dist Curr
            switch (planeID) {
                case 0: dp = prev.w + prev.x; dc = curr.w + curr.x; break;
                case 1: dp = prev.w - prev.x; dc = curr.w - curr.x; break;
                case 2: dp = prev.w + prev.y; dc = curr.w + curr.y; break;
                case 3: dp = prev.w - prev.y; dc = curr.w - curr.y; break;
                case 4: dp = prev.w + prev.z; dc = curr.w + curr.z; break;
                case 5: dp = prev.w - prev.z; dc = curr.w - curr.z; break;
            }
            return dp / (dp - dc);
        };

        const VOut* prev = &inputVerts.back();
        bool prevInside = isInside(prev->pos);

        for (const auto& curr : inputVerts) {
            bool currInside = isInside(curr.pos);

            if (currInside) {
                if (!prevInside) {
                    // 情况 1: Out -> In (外部进入内部)
                    // 需要在交点处生成新顶点，并加入
                    float t = getIntersectT(prev->pos, curr.pos);
                    outputVerts.push_back(lerpVertex(*prev, curr, t));
                }
                // 情况 2: In -> In (一直在内部)
                // 直接加入当前点
                outputVerts.push_back(curr);
            } else if (prevInside) {
                // 情况 3: In -> Out (内部跑到外部)
                // 需要在交点处生成新顶点，并加入
                float t = getIntersectT(prev->pos, curr.pos);
                outputVerts.push_back(lerpVertex(*prev, curr, t));
            }
            // 情况 4: Out -> Out (一直在外部)，直接丢弃

            prev = &curr;
            prevInside = currInside;
        }

        return outputVerts;
    }

    // Liang-Barsky Line Clipping against a single axis-aligned boundary
    // Returns false if the line is completely outside.
    // Modifies t0 and t1 to the new intersection points.
    bool clipLineAxis(float p, float q, float& t0, float& t1) {
        if (std::abs(p) < EPSILON) { // Parallel to the boundary
            if (q < 0) return false; // Outside the boundary
        } else {
            float t = q / p;
            if (p < 0) { // Potentially entering
                if (t > t1) return false;
                if (t > t0) t0 = t;
            } else { // Potentially exiting
                if (t < t0) return false;
                if (t < t1) t1 = t;
            }
        }
        return true;
    }

    // Clips a line against the canonical view volume. Returns the clipped vertices.
    StaticVector<VOut, 16> clipLine(const VOut& v0, const VOut& v1) {
        float t0 = 0.0f, t1 = 1.0f;
        Vec4 d = v1.pos - v0.pos;

        // Clip against 6 planes of the canonical view volume
        if (!clipLineAxis( d.x + d.w, -(v0.pos.x + v0.pos.w), t0, t1)) return {}; // Left
        if (!clipLineAxis(-d.x + d.w,  (v0.pos.x - v0.pos.w), t0, t1)) return {}; // Right
        if (!clipLineAxis( d.y + d.w, -(v0.pos.y + v0.pos.w), t0, t1)) return {}; // Bottom
        if (!clipLineAxis(-d.y + d.w,  (v0.pos.y - v0.pos.w), t0, t1)) return {}; // Top
        if (!clipLineAxis( d.z + d.w, -(v0.pos.z + v0.pos.w), t0, t1)) return {}; // Near
        if (!clipLineAxis(-d.z + d.w,  (v0.pos.z - v0.pos.w), t0, t1)) return {}; // Far

        StaticVector<VOut, 16> clippedVerts;
        if (t0 > 0.0f) {
            clippedVerts.push_back(lerpVertex(v0, v1, t0));
        } else {
            clippedVerts.push_back(v0);
        }

        if (t1 < 1.0f) {
            clippedVerts.push_back(lerpVertex(v0, v1, t1));
        } else {
            clippedVerts.push_back(v1);
        }
        
        return clippedVerts;
    }

    // 执行透视除法与视口变换 (Perspective Division & Viewport)
    // 将 Clip Space (x,y,z,w) -> Screen Space (sx, sy, sz, 1/w)
    void transformToScreen(VOut& v) {
        float w = v.pos.w;
        // 此时 w 已经被 Near Plane 裁剪过，一定 > EPSILON
        float rhw = 1.0f / w;

        // Viewport Mapping: [-1, 1] -> [0, Width/Height]
        // X 轴: (ndc + 1) * 0.5 * w + x
        // Y 轴: y + (1 - ndc) * 0.5 * h  (注意：这里假设屏幕原点在左上角)
        v.scn.x = m_viewport.x + (v.pos.x * rhw + 1.0f) * 0.5f * m_viewport.w;
        v.scn.y = m_viewport.y + (1.0f - v.pos.y * rhw) * 0.5f * m_viewport.h;
        v.scn.z = v.pos.z * rhw;
        v.scn.w = rhw; // 存储 1/w 用于透视插值
    }

    // 辅助：计算属性 f 在屏幕空间的偏导数
    Gradients calcGradients(const VOut& v0, const VOut& v1, const VOut& v2, float invArea, float f0, float f1, float f2) {
        float temp0 = f1 - f0;
        float temp1 = f2 - f0;
        float dfdx = (temp0 * (v2.scn.y - v0.scn.y) - temp1 * (v1.scn.y - v0.scn.y)) * invArea;
        float dfdy = (temp1 * (v1.scn.x - v0.scn.x) - temp0 * (v2.scn.x - v0.scn.x)) * invArea;
        return {dfdx, dfdy};
    }

    Vec4 fetchAttribute(const VertexAttribState& attr, int vertexIdx, int instanceIdx) {
        if (!attr.enabled) return Vec4(0,0,0,1);

        auto it = buffers.find(attr.bindingBufferID);
        // 检查 1: Buffer 是否存在
        if (it == buffers.end()) return Vec4(0,0,0,1);
        
        const auto& buffer = it->second;
        // 检查 2: Buffer 是否为空
        if (buffer.data.empty()) return Vec4(0,0,0,1);

        // 计算 stride 
        // TODO: 支持其他类型的 Attribute
        size_t stride = attr.stride ? attr.stride : attr.size * sizeof(float); // Default stride for floats
        // Instancing 核心逻辑
        // 如果 divisor 为 0，使用 vertexIdx。
        // 如果 divisor > 0，使用 instanceIdx / divisor。
        int effectiveIdx = (attr.divisor == 0) ? vertexIdx : (instanceIdx / attr.divisor);
        // 计算起始偏移量
        size_t offset = attr.pointerOffset + effectiveIdx * stride;
        
        // 计算读取该属性所需的总大小
        // 注意：attr.type 可能是 float 或 byte，这里假设标准 float 为 4 字节
        // 如果你的系统支持更多类型，这里需要根据 attr.type 动态计算 dataSize
        size_t elementSize = (attr.type == GL_UNSIGNED_BYTE) ? sizeof(uint8_t) : sizeof(float);
        size_t readSize = attr.size * elementSize;

        // 检查 3: 严格的越界检查
        // offset + readSize > buffer.size()
        if (offset >= buffer.data.size() || (buffer.data.size() - offset) < readSize) {
            // 返回默认值，防止崩溃
            return Vec4(0,0,0,1);
        }

        float raw[4] = {0,0,0,1};
        
        // 读取逻辑 (使用 readSafe 进一步保证，或者直接 memcpy 因为上面已检查)
        switch (attr.type) {
            case GL_FLOAT: {
                // for(int i=0; i<attr.size; ++i) {
                //     it->second.readSafe<float>(offset + i*sizeof(float), raw[i]);
                // }
                // 直接拷贝，比循环 readSafe 快
                std::memcpy(raw, buffer.data.data() + offset, attr.size * sizeof(float));
                break;
            }
            case GL_UNSIGNED_BYTE: {
                // stride = attr.stride ? attr.stride : attr.size * sizeof(uint8_t);
                // offset = attr.pointerOffset + vertexIdx * stride;
                // 如果是 BYTE 类型，stride 默认值计算可能需要区分，但上面已经根据类型计算了 stride，
                // 这里的逻辑复用 fetchAttribute 原有逻辑即可，重点是 offset 计算变了。
                uint8_t ubyte_raw[4] = {0,0,0,255};
                // for(int i=0; i<attr.size; ++i) {
                //     it->second.readSafe<uint8_t>(offset + i*sizeof(uint8_t), ubyte_raw[i]);
                // }
                std::memcpy(ubyte_raw, buffer.data.data() + offset, attr.size * sizeof(uint8_t));
                
                if (attr.normalized) {
                    for(int i=0; i<4; ++i) raw[i] = ubyte_raw[i] / 255.0f;
                } else {
                    for(int i=0; i<4; ++i) raw[i] = (float)ubyte_raw[i];
                }
                break;
            }
            default:
                LOG_WARN("Unsupported vertex attribute type.");
                break;
        }
        
        return Vec4(raw[0], raw[1], raw[2], raw[3]);
    }

    // 纯粹的光栅化三角形逻辑 (Rasterize Triangle) - Scanline Implementation
    /*
    * 顶点排序：首先根据 Y 坐标对三个顶点进行排序（Top, Mid, Bottom）。
    * 梯度计算：基于平面方程预计算 Z (深度) 和所有 Varyings (属性) 随 X 和 Y 的变化率 (d/dx, d/dy)，避免了逐像素的重心坐标计算。
    * 边缘遍历：利用 Bresenham 思想或斜率步进，计算每一行扫描线的左右边界 (cx_left, cx_right)。
    * 扫描线填充：
        * 将三角形分为上半部分（Top-Mid）和下半部分（Mid-Bottom）分别循环。
        * 在每行扫描线起始处，利用梯度公式计算初始属性值。
        * 行内循环利用增量更新 (val += dVal/dX) 快速插值属性，极大提升了内层循环效率。
    * 透视校正：保留了基于 1/w 的透视校正插值逻辑，确保纹理和颜色在 3D 空间中的正确性。
    * 逻辑优化：移除了每像素的 LOD 计算（强制 LOD=0），进一步优化性能。
     */
    void rasterizeTriangleScanline(const VOut& v0_in, const VOut& v1_in, const VOut& v2_in) {
        // 1. Backface Culling & Area Calculation (Use original winding)
        float area = (v1_in.scn.x - v0_in.scn.x) * (v2_in.scn.y - v0_in.scn.y) - 
                     (v1_in.scn.y - v0_in.scn.y) * (v2_in.scn.x - v0_in.scn.x);
        
        // 【Fix】: Invert culling logic.
        // In screen space (Y-down), standard CCW winding produces a NEGATIVE determinant/area.
        // So Front Faces have area < 0. We cull if area >= 0.
        if (area >= 0) return;
        float invArea = 1.0f / area;

        // 2. Gradient Calculation (Plane Equations)
        // dVal/dX = [ (val1-val0)*(y2-y0) - (val2-val0)*(y1-y0) ] / Area
        // dVal/dY = [ (val2-val0)*(x1-x0) - (val1-val0)*(x2-x0) ] / Area
        auto calcGrad = [&](float val0, float val1, float val2) -> std::pair<float, float> {
             float t0 = val1 - val0;
             float t1 = val2 - val0;
             float ddx = (t0 * (v2_in.scn.y - v0_in.scn.y) - t1 * (v1_in.scn.y - v0_in.scn.y)) * invArea;
             float ddy = (t1 * (v1_in.scn.x - v0_in.scn.x) - t0 * (v2_in.scn.x - v0_in.scn.x)) * invArea;
             return {ddx, ddy};
        };

        // Gradients for Z (1/w)
        auto [dzdx, dzdy] = calcGrad(v0_in.scn.w, v1_in.scn.w, v2_in.scn.w);

        // Gradients for Varyings (Pre-multiplied by 1/w)
        struct VaryingGrad { float dx[4], dy[4]; };
        VaryingGrad dv[MAX_VARYINGS];

        for(int i=0; i<MAX_VARYINGS; ++i) {
             Vec4 va0 = v0_in.ctx.varyings[i] * v0_in.scn.w;
             Vec4 va1 = v1_in.ctx.varyings[i] * v1_in.scn.w;
             Vec4 va2 = v2_in.ctx.varyings[i] * v2_in.scn.w;
             
             auto gX = calcGrad(va0.x, va1.x, va2.x); dv[i].dx[0] = gX.first; dv[i].dy[0] = gX.second;
             auto gY = calcGrad(va0.y, va1.y, va2.y); dv[i].dx[1] = gY.first; dv[i].dy[1] = gY.second;
             auto gZ = calcGrad(va0.z, va1.z, va2.z); dv[i].dx[2] = gZ.first; dv[i].dy[2] = gZ.second;
             auto gW = calcGrad(va0.w, va1.w, va2.w); dv[i].dx[3] = gW.first; dv[i].dy[3] = gW.second;
        }

        // 3. Vertex Sorting (Top to Bottom)
        const VOut* p[3] = { &v0_in, &v1_in, &v2_in };
        if (p[0]->scn.y > p[1]->scn.y) std::swap(p[0], p[1]);
        if (p[0]->scn.y > p[2]->scn.y) std::swap(p[0], p[2]);
        if (p[1]->scn.y > p[2]->scn.y) std::swap(p[1], p[2]);

        const VOut& vTop = *p[0];
        const VOut& vMid = *p[1];
        const VOut& vBot = *p[2];

        // 4. Setup Edges
        int yStart = (int)std::ceil(vTop.scn.y - 0.5f);
        int yMid   = (int)std::ceil(vMid.scn.y - 0.5f);
        int yEnd   = (int)std::ceil(vBot.scn.y - 0.5f);

        if (yStart >= yEnd) return; // Zero height

        // Calculate Edge Slopes (dX/dY)
        // Guard against div by zero? (yDiff is at least something if yStart < yEnd, but logic below handles blocks)
        float dyLong = vBot.scn.y - vTop.scn.y;
        float dyTop  = vMid.scn.y - vTop.scn.y;
        float dyBot  = vBot.scn.y - vMid.scn.y;

        float dxLong = (vBot.scn.x - vTop.scn.x) / (std::abs(dyLong) < EPSILON ? 1.0f : dyLong);
        float dxTop  = (vMid.scn.x - vTop.scn.x) / (std::abs(dyTop) < EPSILON ? 1.0f : dyTop);
        float dxBot  = (vBot.scn.x - vMid.scn.x) / (std::abs(dyBot) < EPSILON ? 1.0f : dyBot);

        // Determine Left/Right
        // Cross product logic to see if Mid is to the left of Long Edge
        // (vBot.x - vTop.x)*(vMid.y - vTop.y) - (vBot.y - vTop.y)*(vMid.x - vTop.x)
        // Note: Y is down in Screen Space? Code says v.scn.y = (1.0 - y)*H. So Y=0 is Top.
        // Let's rely on X comparison at Mid Y.
        float xLongAtMid = vTop.scn.x + dxLong * dyTop;
        bool midIsLeft = vMid.scn.x < xLongAtMid;

        // Current X trackers
        float cx_left, cx_right;
        float dLeft, dRight;

        // Initialize X at yStart
        float dyStart = (yStart + 0.5f) - vTop.scn.y;
        
        // We handle two phases: Top Half (yStart -> yMid) and Bottom Half (yMid -> yEnd)
        auto* prog = getCurrentProgram();

        // ---------------------------------------------------------
        // Loop Helper (Macro or Lambda to avoid code duplication)
        // ---------------------------------------------------------
        auto processScanline = [&](int y, int xL, int xR) {
            // Viewport & Boundary Clipping
            int vy0 = std::max(0, m_viewport.y);
            int vy1 = std::min((int)fbHeight, m_viewport.y + m_viewport.h);
            if (y < vy0 || y >= vy1) return;

            int vx0 = std::max(0, m_viewport.x);
            int vx1 = std::min((int)fbWidth, m_viewport.x + m_viewport.w);

            int xs = std::max(vx0, xL);
            int xe = std::min(vx1, xR);
            if (xs >= xe) return;

            // Row Start Params
            float dy = (y + 0.5f) - v0_in.scn.y; // Relative to v0 for gradient calc
            float dx_start = (xs + 0.5f) - v0_in.scn.x;

            // Initialize running values at start of span
            float zInv = v0_in.scn.w + dx_start * dzdx + dy * dzdy;
            Vec4 runVar[MAX_VARYINGS];
            for(int i=0; i<MAX_VARYINGS; ++i) {
                // Pre-calc va0
                Vec4 va0 = v0_in.ctx.varyings[i] * v0_in.scn.w;
                runVar[i].x = va0.x + dx_start * dv[i].dx[0] + dy * dv[i].dy[0];
                runVar[i].y = va0.y + dx_start * dv[i].dx[1] + dy * dv[i].dy[1];
                runVar[i].z = va0.z + dx_start * dv[i].dx[2] + dy * dv[i].dy[2];
                runVar[i].w = va0.w + dx_start * dv[i].dx[3] + dy * dv[i].dy[3];
            }

            int pixOffset = y * fbWidth + xs;
            ShaderContext fsIn;
            fsIn.lod = 0;

            for (int x = xs; x < xe; ++x) {
                if (zInv > 0) {
                    float z = 1.0f / zInv;
                    if (z < depthBuffer[pixOffset]) {
                        depthBuffer[pixOffset] = z;

                        // Recover Attributes
                        for(int k=0; k<MAX_VARYINGS; ++k) {
                            fsIn.varyings[k].x = runVar[k].x * z;
                            fsIn.varyings[k].y = runVar[k].y * z;
                            fsIn.varyings[k].z = runVar[k].z * z;
                            fsIn.varyings[k].w = runVar[k].w * z;
                        }

                        // Shading
                        Vec4 c = prog->fragmentShader(fsIn);
                        uint32_t R = (uint32_t)(std::min(c.x, 1.0f) * 255.0f);
                        uint32_t G = (uint32_t)(std::min(c.y, 1.0f) * 255.0f);
                        uint32_t B = (uint32_t)(std::min(c.z, 1.0f) * 255.0f);
                        m_colorBufferPtr[pixOffset] = (255 << 24) | (B << 16) | (G << 8) | R;
                    }
                }
                
                // Increment X
                zInv += dzdx;
                for(int k=0; k<MAX_VARYINGS; ++k) {
                    runVar[k].x += dv[k].dx[0];
                    runVar[k].y += dv[k].dx[1];
                    runVar[k].z += dv[k].dx[2];
                    runVar[k].w += dv[k].dx[3];
                }
                pixOffset++;
            }
        };

        // --- Phase 1: Top Half ---
        if (midIsLeft) {
            dLeft = dxTop; dRight = dxLong;
            cx_left = vTop.scn.x + dLeft * dyStart;
            cx_right = vTop.scn.x + dRight * dyStart;
        } else {
            dLeft = dxLong; dRight = dxTop;
            cx_left = vTop.scn.x + dLeft * dyStart;
            cx_right = vTop.scn.x + dRight * dyStart;
        }

        for (int y = yStart; y < yMid; ++y) {
            processScanline(y, (int)std::ceil(cx_left - 0.5f), (int)std::ceil(cx_right - 0.5f));
            cx_left += dLeft;
            cx_right += dRight;
        }

        // --- Phase 2: Bottom Half ---
        // Need to realign the short edge to start from Mid
        float dyMid = (yMid + 0.5f) - vMid.scn.y;
        
        // If midIsLeft, Left edge switches from Top->Mid to Mid->Bot
        // Right edge continues Top->Bot
        if (midIsLeft) {
            dLeft = dxBot; 
            // Reset left X to Mid
            cx_left = vMid.scn.x + dLeft * dyMid;
            // Right X continues, but to be safe/precise we could leave it
            // or re-calculate from top? Incrementing is usually fine.
        } else {
            dRight = dxBot;
            // Reset right X to Mid
            cx_right = vMid.scn.x + dRight * dyMid;
        }

        for (int y = yMid; y < yEnd; ++y) {
             processScanline(y, (int)std::ceil(cx_left - 0.5f), (int)std::ceil(cx_right - 0.5f));
             cx_left += dLeft;
             cx_right += dRight;
        }
    }

    // 纯粹的光栅化三角形逻辑 (Rasterize Triangle)
    // 输入已经是 Screen Space 的三个顶点
    void rasterizeTriangleEdgeFunction(const VOut& v0, const VOut& v1, const VOut& v2) {
        auto* prog = getCurrentProgram();
        // 1. 包围盒计算 (Bounding Box)
        int limitMinX = std::max(0, m_viewport.x);
        int limitMaxX = std::min((int)fbWidth, m_viewport.x + m_viewport.w);
        int limitMinY = std::max(0, m_viewport.y);
        int limitMaxY = std::min((int)fbHeight, m_viewport.y + m_viewport.h);

        int minX = std::max(limitMinX, (int)std::min({v0.scn.x, v1.scn.x, v2.scn.x}));
        int maxX = std::min(limitMaxX - 1, (int)std::max({v0.scn.x, v1.scn.x, v2.scn.x}) + 1);
        int minY = std::max(limitMinY, (int)std::min({v0.scn.y, v1.scn.y, v2.scn.y}));
        int maxY = std::min(limitMaxY - 1, (int)std::max({v0.scn.y, v1.scn.y, v2.scn.y}) + 1);

        // 2. 面积计算与背面剔除 (Area & Backface Culling)
        // 注意：这里使用的是屏幕空间坐标 (x, y)
        float area = (v1.scn.y - v0.scn.y) * (v2.scn.x - v0.scn.x) - 
                    (v1.scn.x - v0.scn.x) * (v2.scn.y - v0.scn.y);
        
        // 如果面积 <= 0，说明是背面或退化三角形，直接剔除
        if (area <= 0) return; 
        float invArea = 1.0f / area;

        // ==========================================
        // [优化核心]：增量式光栅化设置 (Incremental Setup)
        // ==========================================
        
        // 定义 Edge 函数辅助 Lambda (仅用于初始化起点)
        auto edgeFunc = [](const Vec4& a, const Vec4& b, float px, float py) {
            return (b.y - a.y) * (px - a.x) - (b.x - a.x) * (py - a.y);
        };

        // 计算步进系数 (Stepping Coefficients)
        // A 代表向右走一步 (x+1) 权重的增量: dEdge/dx = y_b - y_a
        // B 代表向下走一步 (y+1) 权重的增量: dEdge/dy = x_a - x_b
        
        // Edge 0: v1 -> v2 (控制 v0 的权重)
        float A0 = v2.scn.y - v1.scn.y; 
        float B0 = v1.scn.x - v2.scn.x;
        
        // Edge 1: v2 -> v0 (控制 v1 的权重)
        float A1 = v0.scn.y - v2.scn.y; 
        float B1 = v2.scn.x - v0.scn.x;
        
        // Edge 2: v0 -> v1 (控制 v2 的权重)
        float A2 = v1.scn.y - v0.scn.y; 
        float B2 = v0.scn.x - v1.scn.x;

        // 采样中心偏移 (Pixel Center Offset)
        float startX = minX + 0.5f;
        float startY = minY + 0.5f;

        // 计算起始行 (minY, minX) 的初始权重
        float w0_row = edgeFunc(v1.scn, v2.scn, startX, startY);
        float w1_row = edgeFunc(v2.scn, v0.scn, startX, startY);
        float w2_row = edgeFunc(v0.scn, v1.scn, startX, startY);

        // ==========================================
        // 光栅化循环 (Rasterization Loop)
        // ==========================================
        
        for (int y = minY; y <= maxY; ++y) {
            // 初始化当前扫描线的权重
            float w0 = w0_row;
            float w1 = w1_row;
            float w2 = w2_row;

            for (int x = minX; x <= maxX; ++x) {
                // Inside Test: 检查所有权重是否 >= 0
                // 使用位运算优化符号检查 (或者简单的 if)
                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    
                    // 1. 计算重心坐标 (Barycentric Coordinates)
                    float alpha = w0 * invArea;
                    float beta  = w1 * invArea;
                    float gamma = w2 * invArea;

                    // 2. 透视校正深度插值 (Perspective Correct Depth)
                    // v.scn.w 存储的是 1/w_clip
                    float zInv = alpha * v0.scn.w + beta * v1.scn.w + gamma * v2.scn.w;
                    
                    // 3. 深度测试 (Early Depth Test)
                    // 只有当 zInv > 0 时才有效 (虽已由 Clip 保证，但安全第一)
                    if (zInv > 0) {
                        float z = 1.0f / zInv;
                        int pix = y * fbWidth + x;

                        if (z < depthBuffer[pix]) {
                            depthBuffer[pix] = z;

                            // 4. 属性插值 (Attribute Interpolation)
                            ShaderContext fsIn;
                            fsIn.lod = 0.0f; // [优化] 强制关闭每像素 LOD 计算，极大提升性能

                            // 展开循环以利用 CPU 流水线 (编译器通常会自动展开)
                            // for (int k = 0; k < MAX_VARYINGS; ++k) {
                            //     // 插值公式: Interpolated = (Attr0*w0 + Attr1*w1 + Attr2*w2) / zInv
                            //     // 预先乘好 1/w 部分: var * scn.w
                            //     fsIn.varyings[k] = (v0.ctx.varyings[k] * v0.scn.w * alpha + 
                            //                         v1.ctx.varyings[k] * v1.scn.w * beta + 
                            //                         v2.ctx.varyings[k] * v2.scn.w * gamma) * z;
                            // }
                            // 预计算这三个顶点的 varyings 指针，避免 vector 索引开销
                            const Vec4* var0Ptr = v0.ctx.varyings;
                            const Vec4* var1Ptr = v1.ctx.varyings;
                            const Vec4* var2Ptr = v2.ctx.varyings;

                            // 预计算权重因子
                            float w_alpha = v0.scn.w * alpha * z;
                            float w_beta  = v1.scn.w * beta * z;
                            float w_gamma = v2.scn.w * gamma * z;

                            // 手动展开 MAX_VARYINGS 循环 (假设 MAX_VARYINGS = 8)
                            // 并且手动展开 Vec4 的 x,y,z,w 计算，避免创建 Vec4 临时对象
                            // 实际上，为了通用性，我们保留 k 循环，但展开内部的 Vec4 运算
                            for (int k = 0; k < MAX_VARYINGS; ++k) {
                                // 直接操作 float，不创建 Vec4 对象
                                float x = var0Ptr[k].x * w_alpha + var1Ptr[k].x * w_beta + var2Ptr[k].x * w_gamma;
                                float y = var0Ptr[k].y * w_alpha + var1Ptr[k].y * w_beta + var2Ptr[k].y * w_gamma;
                                float z = var0Ptr[k].z * w_alpha + var1Ptr[k].z * w_beta + var2Ptr[k].z * w_gamma;
                                float w = var0Ptr[k].w * w_alpha + var1Ptr[k].w * w_beta + var2Ptr[k].w * w_gamma;
                                
                                // 仅在赋值时构造一次
                                fsIn.varyings[k].x = x;
                                fsIn.varyings[k].y = y;
                                fsIn.varyings[k].z = z;
                                fsIn.varyings[k].w = w;
                            }

                            // 5. 执行 Fragment Shader
                            // (此时建议 fs 内部已经是优化过的 Lambda，不含哈希查找)
                            Vec4 c = prog->fragmentShader(fsIn);
                            
                            // 6. 颜色写入 (Color Write)
                            // 使用 min 避免 clamp 下溢出的分支，转为整数
                            uint32_t R = (uint32_t)(std::min(c.x, 1.0f) * 255.0f);
                            uint32_t G = (uint32_t)(std::min(c.y, 1.0f) * 255.0f);
                            uint32_t B = (uint32_t)(std::min(c.z, 1.0f) * 255.0f);
                            
                            m_colorBufferPtr[pix] = (255 << 24) | (B << 16) | (G << 8) | R;
                        }
                    }
                }

                // [增量] X 轴推进一步 (Move Right)
                w0 += A0;
                w1 += A1;
                w2 += A2;
            }

            // [增量] Y 轴推进一步 (Move Down)
            w0_row += B0;
            w1_row += B1;
            w2_row += B2;
        }
    }

    template <typename ShaderT>
    void rasterizeTriangleTemplate(ShaderT& shader, const VOut& v0, const VOut& v1, const VOut& v2) {
        // 1. 包围盒计算 (Bounding Box)
        int limitMinX = std::max(0, m_viewport.x);
        int limitMaxX = std::min((int)fbWidth, m_viewport.x + m_viewport.w);
        int limitMinY = std::max(0, m_viewport.y);
        int limitMaxY = std::min((int)fbHeight, m_viewport.y + m_viewport.h);

        int minX = std::max(limitMinX, (int)std::min({v0.scn.x, v1.scn.x, v2.scn.x}));
        int maxX = std::min(limitMaxX - 1, (int)std::max({v0.scn.x, v1.scn.x, v2.scn.x}) + 1);
        int minY = std::max(limitMinY, (int)std::min({v0.scn.y, v1.scn.y, v2.scn.y}));
        int maxY = std::min(limitMaxY - 1, (int)std::max({v0.scn.y, v1.scn.y, v2.scn.y}) + 1);

        // 2. 面积计算 (Backface Culling)
        float area = (v1.scn.y - v0.scn.y) * (v2.scn.x - v0.scn.x) - 
                     (v1.scn.x - v0.scn.x) * (v2.scn.y - v0.scn.y);
        
        // 面积 <= 0 剔除 (假设 CCW)
        if (area <= 0) return;
        float invArea = 1.0f / area;

        // 3. 增量系数 Setup
        // Edge 0: v1 -> v2
        float A0 = v2.scn.y - v1.scn.y; float B0 = v1.scn.x - v2.scn.x;
        // Edge 1: v2 -> v0
        float A1 = v0.scn.y - v2.scn.y; float B1 = v2.scn.x - v0.scn.x;
        // Edge 2: v0 -> v1
        float A2 = v1.scn.y - v0.scn.y; float B2 = v0.scn.x - v1.scn.x;

        // 4. [关键优化] 预计算透视修正后的 Varyings
        // 原理：在三角形 Setup 阶段，先计算好 (Attr * 1/w_clip)
        // 这样在像素循环中，只需要做线性组合，不需要做额外的乘法
        // 我们利用 SIMD 寄存器数组在栈上存储这些预处理数据
        Simd4f preVar0[MAX_VARYINGS];
        Simd4f preVar1[MAX_VARYINGS];
        Simd4f preVar2[MAX_VARYINGS];

        // 广播 1/w_clip 到 SIMD 寄存器
        Simd4f w0_vec(v0.scn.w);
        Simd4f w1_vec(v1.scn.w);
        Simd4f w2_vec(v2.scn.w);

        // 循环展开预处理所有 Varyings
        for (int k = 0; k < MAX_VARYINGS; ++k) {
            preVar0[k] = Simd4f::load(v0.ctx.varyings[k]) * w0_vec;
            preVar1[k] = Simd4f::load(v1.ctx.varyings[k]) * w1_vec;
            preVar2[k] = Simd4f::load(v2.ctx.varyings[k]) * w2_vec;
        }

        // 5. 初始权重计算 (Pixel Center)
        float startX = minX + 0.5f;
        float startY = minY + 0.5f;
        auto edgeFunc = [](float ax, float ay, float bx, float by, float px, float py) {
            return (by - ay) * (px - ax) - (bx - ax) * (py - ay);
        };
        float w0_row = edgeFunc(v1.scn.x, v1.scn.y, v2.scn.x, v2.scn.y, startX, startY);
        float w1_row = edgeFunc(v2.scn.x, v2.scn.y, v0.scn.x, v0.scn.y, startX, startY);
        float w2_row = edgeFunc(v0.scn.x, v0.scn.y, v1.scn.x, v1.scn.y, startX, startY);

        // 6. 像素遍历循环
        for (int y = minY; y <= maxY; ++y) {
            float w0 = w0_row; float w1 = w1_row; float w2 = w2_row;
            
            // 直接计算指针偏移，避免乘法
            uint32_t pixelOffset = y * fbWidth + minX;
            uint32_t* pColor = m_colorBufferPtr + pixelOffset;
            float* pDepth = depthBuffer.data() + pixelOffset;
            // [优化] 将 fsIn 提到循环外，避免每次循环都构造 memset
            ShaderContext fsIn; 
            fsIn.lod = 0; 
            for (int x = minX; x <= maxX; ++x) {
                // 位运算优化符号检查: (w0 | w1 | w2) >= 0
                // 注意：需要确保 float 的符号位逻辑，这里用 union cast 或简单的 if
                // 现代编译器对 if (f>=0) 优化得很好，我们保持可读性
                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    float alpha = w0 * invArea;
                    float beta  = w1 * invArea;
                    float gamma = w2 * invArea;

                    // 透视校正深度 1/w_view
                    float zInv = alpha * v0.scn.w + beta * v1.scn.w + gamma * v2.scn.w;
                    
                    // 深度测试
                    if (zInv > 1e-6f) { // 避免除以0
                        float z = 1.0f / zInv;
                        if (z < *pDepth) {
                            *pDepth = z;
                            
                            // 广播 z 到 SIMD 寄存器，用于最后的透视恢复
                            Simd4f z_vec(z);
                            Simd4f alpha_vec(alpha);
                            Simd4f beta_vec(beta);
                            Simd4f gamma_vec(gamma);

                            // [通用型 SIMD 插值]
                            // 无论 Shader 用了几个 Varying，我们都利用 SIMD 一次处理 4 个分量
                            // 编译器会自动展开这个循环 (Loop Unrolling)
                            for (int k = 0; k < MAX_VARYINGS; ++k) {
                                // Interpolated = (PreVar0 * alpha + PreVar1 * beta + PreVar2 * gamma) * z
                                // 使用 FMA (Fused Multiply-Add) 指令加速
                                Simd4f res = preVar0[k] * alpha_vec;
                                res = res.madd(preVar1[k], beta_vec);
                                res = res.madd(preVar2[k], gamma_vec);
                                res = res * z_vec;
                                
                                // 存储回 ShaderContext
                                res.store(fsIn.varyings[k]);
                            }

                            // 内联调用 Fragment Shader
                            Vec4 fColor = shader.fragment(fsIn);
                            *pColor = ColorUtils::FloatToUint32(fColor);
                        }
                    }
                }
                
                // X轴增量
                w0 += A0; w1 += A1; w2 += A2;
                pColor++; pDepth++;
            }
            // Y轴增量
            w0_row += B0; w1_row += B1; w2_row += B2;
        }
    }

    // 模板化的线段光栅化 (Bresenham)
    template <typename ShaderT>
    void rasterizeLineTemplate(ShaderT& shader, const VOut& v0, const VOut& v1) {
        int x0 = (int)v0.scn.x; int y0 = (int)v0.scn.y;
        int x1 = (int)v1.scn.x; int y1 = (int)v1.scn.y;

        int dx = std::abs(x1 - x0);
        int dy = -std::abs(y1 - y0);
        int sx = x0 < x1 ? 1 : -1;
        int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;

        // 线段长度，用于插值
        float total_dist = std::sqrt(std::pow((float)x1 - x0, 2) + std::pow((float)y1 - y0, 2));
        if (total_dist < 1e-5f) total_dist = 1.0f;

        // 视口边界
        int minX = m_viewport.x;
        int maxX = m_viewport.x + m_viewport.w;
        int minY = m_viewport.y;
        int maxY = m_viewport.y + m_viewport.h;

        while (true) {
            // 像素裁剪
            if (x0 >= minX && x0 < maxX && y0 >= minY && y0 < maxY) {
                // 计算当前点在 v0->v1 上的插值系数 t
                float dist = std::sqrt(std::pow((float)x0 - v0.scn.x, 2) + std::pow((float)y0 - v0.scn.y, 2));
                float t = dist / total_dist;
                t = std::clamp(t, 0.0f, 1.0f);

                // 透视校正插值 Z (Linear Z in Screen Space is 1/w)
                // 注意：这里简化处理，严谨的线段透视插值也需要重心坐标逻辑
                float zInv = v0.scn.w * (1.0f - t) + v1.scn.w * t;
                
                if (zInv > 1e-5f) { // 避免除零
                    float z = 1.0f / zInv; // 恢复真实深度
                    int pix = y0 * fbWidth + x0;
                    
                    if (z < depthBuffer[pix]) {
                        depthBuffer[pix] = z;

                        // 属性插值
                        ShaderContext fsIn;
                        fsIn.lod = 0;

                        // 利用 SIMD 批量插值 Varyings
                        // 插值公式: val = (val0 * w0 * (1-t) + val1 * w1 * t) * z
                        // 预计算权重
                        float w_t0 = v0.scn.w * (1.0f - t) * z;
                        float w_t1 = v1.scn.w * t * z;

                        // 简单循环或 SIMD 处理 Varyings
                        for(int k=0; k<MAX_VARYINGS; ++k) {
                            fsIn.varyings[k] = v0.ctx.varyings[k] * w_t0 + v1.ctx.varyings[k] * w_t1;
                        }

                        // 调用 Fragment Shader
                        Vec4 fColor = shader.fragment(fsIn);
                        m_colorBufferPtr[pix] = ColorUtils::FloatToUint32(fColor);
                    }
                }
            }

            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }

    // 模板化的点光栅化
    template <typename ShaderT>
    void rasterizePointTemplate(ShaderT& shader, const VOut& v) {
        int x = (int)v.scn.x;
        int y = (int)v.scn.y;

        // 简单的视口裁剪检查
        if (x < m_viewport.x || x >= m_viewport.x + m_viewport.w ||
            y < m_viewport.y || y >= m_viewport.y + m_viewport.h) return;

        int pix = y * fbWidth + x;
        
        // 深度测试 (注意：Points 通常没有透视插值问题，但仍需 Z 测试)
        // v.scn.z 存储的是 Z/W (NDC depth range 0-1 if transformed correctly, or clip z)
        // 这里的逻辑假设 v.scn.z 是已经透视除法后的深度值
        float z = v.scn.z; // 或者 1.0f/v.scn.w 取决于你的深度缓冲存什么

        if (z < depthBuffer[pix]) {
            depthBuffer[pix] = z;
            
            // 准备 Shader 上下文
            ShaderContext fsIn = v.ctx; 
            fsIn.lod = 0; // 点没有 LOD 概念

            // 调用 Fragment Shader
            Vec4 fColor = shader.fragment(fsIn);
            m_colorBufferPtr[pix] = ColorUtils::FloatToUint32(fColor);
        }
    }

    // =========================================================
    // 处理单个三角形的管线流程
    // 目的：复用 Arrays 和 Elements 的后续逻辑，减少代码重复
    // 强制内联以避免函数调用开销
    // =========================================================
    template <typename ShaderT>
    inline void processTriangleVertices(ShaderT& shader, uint32_t idx0, uint32_t idx1, uint32_t idx2, int instanceID) {
        VertexArrayObject& vao = getVAO();
        uint32_t indices[3] = {idx0, idx1, idx2};
        StaticVector<VOut, 16> triangle;

        // 1. Vertex Shader Stage (零堆内存分配)
        for (int k = 0; k < 3; ++k) {
            uint32_t idx = indices[k];
            
            // 栈上分配属性数组 (SoA -> AoS for Shader)
            Vec4 attribs[MAX_ATTRIBS];
            
            // 收集属性 (手动循环展开或编译器自动优化)
            // 注意：为了极致性能，这里只应该读取 shader 实际需要的属性
            // 但通用管线必须遍历 enabled 的属性
            for (int a = 0; a < MAX_ATTRIBS; ++a) {
                if (vao.attributes[a].enabled) {
                    attribs[a] = fetchAttribute(vao.attributes[a], idx, instanceID);
                }
            }

            ShaderContext ctx;
            // 调用 Shader (传入数组指针)
            // 如果 Shader 需要 gl_InstanceID，通常需要修改 shader.vertex 签名或者作为 uniform 传入
            // 这里我们保持接口不变，仅通过 Attribute Divisor 支持 Instancing
            VOut v;
            v.pos = shader.vertex(attribs, ctx);
            v.ctx = ctx;
            triangle.push_back(v);
        }

        // 2. Clipping Stage (保持不变)
        StaticVector<VOut, 16> polygon = triangle;
        for (int p = 0; p < 6; ++p) {
            polygon = clipAgainstPlane(polygon, p);
            if (polygon.empty()) break;
        }
        if (polygon.empty()) return;

        // 3. Perspective Division & Viewport Transform
        for (auto& v : polygon) transformToScreen(v);

        // 4. Rasterization Stage (根据模式分发)
        
        if (m_polygonMode == GL_FILL) {
            // 三角形化处理裁剪后的多边形 (Triangle Fan)
            for (size_t k = 1; k < polygon.size() - 1; ++k) {
                rasterizeTriangleTemplate(shader, polygon[0], polygon[k], polygon[k+1]);
            }
        } 
        else if (m_polygonMode == GL_LINE) {
            // 线框模式
            // 注意：这里我们绘制的是裁剪后多边形的边缘
            // 对于一个三角形，如果不被裁剪，就是 3 条边
            // 如果被裁剪成多边形，这里会画出多边形的轮廓
            for (size_t k = 0; k < polygon.size(); ++k) {
                const VOut& vStart = polygon[k];
                const VOut& vEnd   = polygon[(k + 1) % polygon.size()];
                rasterizeLineTemplate(shader, vStart, vEnd);
            }
        }
        else if (m_polygonMode == GL_POINT) {
            // 点模式
            for (const auto& v : polygon) {
                rasterizePointTemplate(shader, v);
            }
        }
    }

    // 处理单个点 (Vertex -> Clip -> Raster)
    template <typename ShaderT>
    inline void processPointVertex(ShaderT& shader, uint32_t idx, int instanceID) {
        VertexArrayObject& vao = getVAO();
        
        // 1. Vertex Shader
        Vec4 attribs[MAX_ATTRIBS];
        for (int a = 0; a < MAX_ATTRIBS; ++a) {
            if (vao.attributes[a].enabled) {
                attribs[a] = fetchAttribute(vao.attributes[a], idx, instanceID);
            }
        }
        ShaderContext ctx;
        VOut v;
        v.pos = shader.vertex(attribs, ctx);
        v.ctx = ctx;

        // 2. Clipping (Points)
        // 简单的视锥体剔除: -w <= x,y,z <= w
        if (std::abs(v.pos.x) > v.pos.w || 
            std::abs(v.pos.y) > v.pos.w || 
            std::abs(v.pos.z) > v.pos.w) return;

        // 3. Transform
        transformToScreen(v);

        // 4. Rasterize
        rasterizePointTemplate(shader, v);
    }

    // 处理单条线 (Vertex -> Clip -> Raster)
    template <typename ShaderT>
    inline void processLineVertices(ShaderT& shader, uint32_t idx0, uint32_t idx1, int instanceID) {
        VertexArrayObject& vao = getVAO();
        uint32_t indices[2] = {idx0, idx1};
        VOut verts[2];

        // 1. Vertex Shader
        for(int i=0; i<2; ++i) {
            Vec4 attribs[MAX_ATTRIBS];
            for (int a = 0; a < MAX_ATTRIBS; ++a) {
                if (vao.attributes[a].enabled) {
                    attribs[a] = fetchAttribute(vao.attributes[a], indices[i], instanceID);
                }
            }
            ShaderContext ctx;
            verts[i].pos = shader.vertex(attribs, ctx);
            verts[i].ctx = ctx;
        }

        // 2. Clipping (Lines)
        // 使用 Liang-Barsky 算法裁剪线段
        // 注意：clipLine 返回的是 StaticVector，可能包含 0 个或 2 个顶点
        StaticVector<VOut, 16> clipped = clipLine(verts[0], verts[1]);
        if (clipped.count < 2) return;

        // 3. Transform
        transformToScreen(clipped[0]);
        transformToScreen(clipped[1]);

        // 4. Rasterize
        rasterizeLineTemplate(shader, clipped[0], clipped[1]);
    }

    // 通用图元组装函数
    // ShaderT: 着色器类型
    // IndexGetterF: 获取索引的函数对象/Lambda (int i -> uint32_t index)
    template <typename ShaderT, typename IndexGetterF>
    inline void drawTopology(ShaderT& shader, GLenum mode, GLsizei count, int instanceID, IndexGetterF getIndex) {
        switch (mode) {
            case GL_POINTS: {
                for (int i = 0; i < count; ++i) {
                    processPointVertex(shader, getIndex(i), instanceID);
                }
                break;
            }
            case GL_LINES: {
                for (int i = 0; i < count; i += 2) {
                    if (i + 1 >= count) break;
                    processLineVertices(shader, getIndex(i), getIndex(i+1), instanceID);
                }
                break;
            }
            case GL_LINE_STRIP: {
                if (count < 2) break;
                for (int i = 0; i < count - 1; ++i) {
                    processLineVertices(shader, getIndex(i), getIndex(i+1), instanceID);
                }
                break;
            }
            case GL_LINE_LOOP: {
                if (count < 2) break;
                for (int i = 0; i < count - 1; ++i) {
                    processLineVertices(shader, getIndex(i), getIndex(i+1), instanceID);
                }
                processLineVertices(shader, getIndex(count-1), getIndex(0), instanceID);
                break;
            }
            case GL_TRIANGLES: {
                for (int i = 0; i < count; i += 3) {
                    if (i + 2 >= count) break;
                    processTriangleVertices(shader, getIndex(i), getIndex(i+1), getIndex(i+2), instanceID);
                }
                break;
            }
            case GL_TRIANGLE_STRIP: {
                if (count < 3) break;
                for (int i = 0; i < count - 2; ++i) {
                    uint32_t idx0 = getIndex(i);
                    uint32_t idx1 = getIndex(i+1);
                    uint32_t idx2 = getIndex(i+2);
                    if (i % 2 == 0)
                        processTriangleVertices(shader, idx0, idx1, idx2, instanceID);
                    else
                        processTriangleVertices(shader, idx0, idx2, idx1, instanceID);
                }
                break;
            }
            case GL_TRIANGLE_FAN: {
                if (count < 3) break;
                uint32_t centerIdx = getIndex(0);
                for (int i = 1; i < count - 1; ++i) {
                    processTriangleVertices(shader, centerIdx, getIndex(i), getIndex(i+1), instanceID);
                }
                break;
            }
        }
    }

     // 支持所有 Mode 的 glDrawArrays
    template <typename ShaderT>
    void glDrawArrays(ShaderT& shader, GLenum mode, GLint first, GLsizei count) {
        if (count <= 0) return;
        
        // 定义 getter: 索引就是 first + i
        auto linearIndexGetter = [&](int i) -> uint32_t {
            return (uint32_t)(first + i);
        };

        // 直接调用通用逻辑 (默认 instanceID = 0)
        drawTopology(shader, mode, count, 0, linearIndexGetter);
    }

    template <typename ShaderT>
    void glDrawArraysInstanced(ShaderT& shader, GLenum mode, GLint first, GLsizei count, GLsizei primcount) {
        if (count <= 0 || primcount <= 0) return;
        
        auto linearIndexGetter = [&](int i) -> uint32_t {
            return (uint32_t)(first + i);
        };

        for (int inst = 0; inst < primcount; ++inst) {
            drawTopology(shader, mode, count, inst, linearIndexGetter);
        }
    }

    template <typename ShaderT>
    void glDrawElements(ShaderT& shader, GLenum mode, GLsizei count, GLenum type, const void* indices) {
        // 等价于绘制 1 个实例
        glDrawElementsInstanced(shader, mode, count, type, indices, 1);
    }

    // 获取索引类型的大小（字节）
    size_t getIndexTypeSize(GLenum type) {
        switch (type) {
            case GL_UNSIGNED_INT:   return sizeof(uint32_t);
            case GL_UNSIGNED_SHORT: return sizeof(uint16_t);
            case GL_UNSIGNED_BYTE:  return sizeof(uint8_t);
            default: return 0;
        }
    }

    template <typename ShaderT>
    void glDrawElementsInstanced(ShaderT& shader, GLenum mode, GLsizei count, GLenum type, const void* indices, GLsizei instanceCount) {
        if (count == 0 || instanceCount == 0) return;
        
        size_t indexSize = getIndexTypeSize(type);
        if (indexSize == 0) {
            LOG_ERROR("glDrawElements: Invalid index type.");
            return;
        }

        // 1. 确定索引数据的基地址
        const uint8_t* indexDataPtr = nullptr;
        VertexArrayObject& vao = getVAO();
        if (vao.elementBufferID != 0) {
            // --- EBO 模式 ---
            // 此时 indices 参数被视为 Buffer 内的字节偏移量 (Byte Offset)
            auto it = buffers.find(vao.elementBufferID);
            if (it != buffers.end()) {
                const auto& buffer = it->second;
                // indices 在这里解释为字节偏移量
                size_t offset = reinterpret_cast<size_t>(indices);
                // 计算需要读取的总字节数
                size_t requiredSize = count * indexSize;
                // [关键边界检查]
                // 确保 offset + count * size 不会超出 buffer
                if (offset >= buffer.data.size() || (buffer.data.size() - offset) < requiredSize) {
                    LOG_ERROR("glDrawElements: Index buffer overflow!");
                    return;
                }
                indexDataPtr = buffer.data.data() + offset;
            } else {
                LOG_ERROR("glDrawElements: Bound EBO ID not found in buffers.");
                return;
            }
        } else {
            // --- 用户指针模式 ---
            if (indices == nullptr) return; // 空指针检查
            // 此时 indices 参数被视为 CPU 内存地址
            indexDataPtr = static_cast<const uint8_t*>(indices);
        }
        // 如果最终没有获得有效指针（例如无 EBO 且 indices 为空），则退出
        if (!indexDataPtr) return;

        // 2. 索引读取辅助 Lambda
        // 此时我们只需要基于 indexDataPtr 进行偏移读取，无需再关心是 EBO 还是指针
        // 辅助 Lambda：内联索引读取
        auto getIndex = [&](size_t i) -> uint32_t {
            if (type == GL_UNSIGNED_INT) return ((const uint32_t*)indexDataPtr)[i];
            if (type == GL_UNSIGNED_SHORT) return ((const uint16_t*)indexDataPtr)[i];
            if (type == GL_UNSIGNED_BYTE) return ((const uint8_t*)indexDataPtr)[i];
            return 0;
        };

        // 3. 循环实例并调用通用逻辑
        for (GLsizei instanceID = 0; instanceID < instanceCount; ++instanceID) {
            drawTopology(shader, mode, count, instanceID, getIndex);
        }
    }

    void savePPM(const char* filename) {
        FILE* f = fopen(filename, "wb");
        if(!f) return;
        fprintf(f, "P6\n%d %d\n255\n", fbWidth, fbHeight);
        for(int i=0; i<fbWidth*fbHeight; ++i) {
            uint32_t p = colorBuffer[i];
            uint8_t buf[3] = { (uint8_t)(p&0xFF), (uint8_t)((p>>8)&0xFF), (uint8_t)((p>>16)&0xFF) };
            fwrite(buf, 1, 3, f);
        }
        fclose(f);
        LOG_INFO("Saved PPM to " + std::string(filename));
    }
};
