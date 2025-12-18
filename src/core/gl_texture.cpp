#include <tinygl/tinygl.h>


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
TextureObject* SoftRenderContext::getTextureObject(GLuint id) {
    if (id == 0) return nullptr;
    auto it = textures.find(id);
    if (it != textures.end()) {
        return &it->second;
    }
    return nullptr;
}

TextureObject* SoftRenderContext::getTexture(GLuint unit) {
    if (unit >= 32) return nullptr;
    GLuint id = m_boundTextures[unit];
    if (id == 0) return nullptr;
    auto it = textures.find(id);
    return (it != textures.end()) ? &it->second : nullptr;
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
