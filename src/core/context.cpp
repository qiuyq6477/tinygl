#define STB_IMAGE_IMPLEMENTATION
#include <tinygl/tinygl.h>
#include <algorithm> // for std::clamp, std::fill_n

void SoftRenderContext::glViewport(GLint x, GLint y, GLsizei w, GLsizei h) {
    m_viewport.x = x;
    m_viewport.y = y;
    m_viewport.w = w;
    m_viewport.h = h;
}

void SoftRenderContext::glPolygonMode(GLenum face, GLenum mode) {
    // 暂时只支持 GL_FRONT_AND_BACK
    if (face == GL_FRONT_AND_BACK) {
        m_polygonMode = mode;
    }
}

void SoftRenderContext::glClearColor(float r, float g, float b, float a) {
    m_clearColor = {r, g, b, a};
}

// --- Textures ---
TextureObject* SoftRenderContext::getTextureObject(GLuint id) {
    if (id == 0) return nullptr;
    auto it = textures.find(id);
    if (it != textures.end()) {
        return &it->second;
    }
    return nullptr;
}

// --- Buffers ---
void SoftRenderContext::glGenBuffers(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = m_nextID++;
        buffers[res[i]]; 
        LOG_INFO("GenBuffer ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glBindBuffer(GLenum target, GLuint buffer) {
    if (target == GL_ARRAY_BUFFER) m_boundArrayBuffer = buffer;
    else if (target == GL_ELEMENT_ARRAY_BUFFER) getVAO().elementBufferID = buffer;
}

void SoftRenderContext::glBufferData(GLenum target, GLsizei size, const void* data, GLenum usage) {
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
void SoftRenderContext::glGenVertexArrays(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = m_nextID++;
        vaos[res[i]];
        LOG_INFO("GenVAO ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glBindVertexArray(GLuint array) { m_boundVertexArray = array; }

void SoftRenderContext::glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* pointer) {
    if (index >= MAX_ATTRIBS) return;
    if (m_boundArrayBuffer == 0) {
        LOG_ERROR("No VBO bound!");
        return;
    }
    VertexAttribState& attr = getVAO().attributes[index];
    attr.bindingBufferID = m_boundArrayBuffer;
    attr.size = size;
    attr.type = type;
    attr.normalized = norm; 
    attr.stride = stride;
    attr.pointerOffset = reinterpret_cast<size_t>(pointer);
    LOG_INFO("Attrib " + std::to_string(index) + " bound to VBO " + std::to_string(m_boundArrayBuffer));
}

void SoftRenderContext::glEnableVertexAttribArray(GLuint index) {
    if (index < MAX_ATTRIBS) getVAO().attributes[index].enabled = true;
}

void SoftRenderContext::glVertexAttribDivisor(GLuint index, GLuint divisor) {
    if (index < MAX_ATTRIBS) {
        getVAO().attributes[index].divisor = divisor;
    }
}

// --- Textures ---
void SoftRenderContext::glGenTextures(GLsizei n, GLuint* res) {
    for(int i=0; i<n; i++) {
        res[i] = m_nextID++;
        // 【Fix】: 立即创建 Texture 对象，防止后续 lookup 失败
        textures[res[i]].id = res[i]; 
        LOG_INFO("GenTexture ID: " + std::to_string(res[i]));
    }
}

void SoftRenderContext::glActiveTexture(GLenum texture) {
    if (texture >= GL_TEXTURE0 && texture < GL_TEXTURE0 + 32) m_activeTextureUnit = texture - GL_TEXTURE0;
}

void SoftRenderContext::glBindTexture(GLenum target, GLuint texture) {
    if (target == GL_TEXTURE_2D) {
        m_boundTextures[m_activeTextureUnit] = texture;
        // 【Robustness】: 如果绑定的 ID 合法但 Map 中不存在（某些用法允许Bind时创建），则补建
        if (texture != 0 && textures.find(texture) == textures.end()) {
            textures[texture].id = texture;
            LOG_INFO("Implicitly created Texture ID: " + std::to_string(texture));
        }
    }
}

void SoftRenderContext::glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* p) {
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

void SoftRenderContext::glTexParameteri(GLenum target, GLenum pname, GLint param) {
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
        default: break;
    }
}

void SoftRenderContext::glTexParameterf(GLenum target, GLenum pname, GLfloat param) {
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

void SoftRenderContext::glTexParameteriv(GLenum target, GLenum pname, const GLint* params) {
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

void SoftRenderContext::glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params) {
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
        } else {
            LOG_ERROR("Unsupported source format with GL_UNSIGNED_BYTE type.");
            return false;
        }
    } else {
        LOG_ERROR("Unsupported source type for pixel conversion.");
        return false;
    }
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
    v.scn.z = v.pos.z * rhw;
    v.scn.w = rhw; // 存储 1/w 用于透视插值
}

Gradients SoftRenderContext::calcGradients(const VOut& v0, const VOut& v1, const VOut& v2, float invArea, float f0, float f1, float f2) {
    float temp0 = f1 - f0;
    float temp1 = f2 - f0;
    float dfdx = (temp0 * (v2.scn.y - v0.scn.y) - temp1 * (v1.scn.y - v0.scn.y)) * invArea;
    float dfdy = (temp1 * (v1.scn.x - v0.scn.x) - temp0 * (v2.scn.x - v0.scn.x)) * invArea;
    return {dfdx, dfdy};
}

Vec4 SoftRenderContext::fetchAttribute(const VertexAttribState& attr, int vertexIdx, int instanceIdx) {
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

 StaticVector<VOut, 16> SoftRenderContext::clipAgainstPlane(const StaticVector<VOut, 16>& inputVerts, int planeID) {
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


bool SoftRenderContext::clipLineAxis(float p, float q, float& t0, float& t1) {
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

StaticVector<VOut, 16> SoftRenderContext::clipLine(const VOut& v0, const VOut& v1) {
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

void SoftRenderContext::glClear(uint32_t buffersToClear) {
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