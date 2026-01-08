#pragma once
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
#include <type_traits>

#include "gl_defs.h"
#include "gl_texture.h"
#include "gl_buffer.h"
#include "gl_shader.h"

#include "../base/tmath.h"
#include "../base/math_simd.h"
#include "../base/color.h"
#include "../base/colors.h"
#include "../base/log.h"
#include "../base/container.h"

namespace tinygl {

// 辅助：计算平面梯度的结构
struct Gradients {
    float dfdx, dfdy;
};

struct Viewport {
    GLint x, y;
    GLsizei w, h;
};

class TINYGL_API SoftRenderContext {
public:
    template <typename T>
    class ResourcePool {
    private:
        struct Slot {
            T obj;
            bool active = false;
        };
        std::vector<Slot> pool;
        std::vector<GLuint> freeList;
    public:
        ResourcePool() { pool.resize(1); } // Reserve 0
        
        GLuint allocate() {
            if (!freeList.empty()) {
                GLuint id = freeList.back();
                freeList.pop_back();
                pool[id].active = true;
                pool[id].obj = T();
                return id;
            }
            pool.push_back({T(), true});
            return (GLuint)pool.size() - 1;
        }

        void release(GLuint id) {
            if (id > 0 && id < pool.size() && pool[id].active) {
                pool[id].active = false;
                pool[id].obj = T();
                freeList.push_back(id);
            }
        }

        T* get(GLuint id) {
            if (id < pool.size() && pool[id].active) return &pool[id].obj;
            return nullptr;
        }

        T* forceAllocate(GLuint id) {
            if (id >= pool.size()) pool.resize(id + 1);
            if (!pool[id].active) {
                pool[id].active = true;
                pool[id].obj = T();
            }
            return &pool[id].obj;
        }

        bool isActive(GLuint id) const {
            return id < pool.size() && pool[id].active;
        }
    };

private:
    ResourcePool<BufferObject> buffers;
    ResourcePool<VertexArrayObject> vaos;
    ResourcePool<TextureObject> textures;

    GLuint m_boundArrayBuffer = 0;
    GLuint m_boundVertexArray = 0;
    GLuint m_boundCopyReadBuffer = 0;
    GLuint m_boundCopyWriteBuffer = 0;
    GLuint m_activeTextureUnit = 0;
    GLuint m_boundTextures[MAX_TEXTURE_UNITS] = {0};

    GLsizei fbWidth = 800;
    GLsizei fbHeight = 600;
    
    std::vector<uint32_t> colorBuffer;
    uint32_t* m_colorBufferPtr = nullptr; // Pointer to the active color buffer (internal or external)

    std::vector<float> depthBuffer;
    std::vector<uint8_t> stencilBuffer; // 8-bit Stencil Buffer

    std::vector<uint32_t> m_indexCache;
    Vec4 m_clearColor = {0.0f, 0.0f, 0.0f, 1.0f}; // Default clear color is black

    Viewport m_viewport;
    GLenum m_polygonMode = GL_FILL; // 默认填充
    GLenum m_cullFaceMode = GL_BACK; // Default cull back faces
    GLenum m_frontFace = GL_CCW;     // Default counter-clockwise is front
    GLboolean m_depthMask = GL_TRUE; // Default depth writing enabled

    // Stencil State
    GLenum m_stencilFunc = GL_ALWAYS;
    GLint m_stencilRef = 0;
    GLuint m_stencilValueMask = 0xFFFFFFFF;
    GLuint m_stencilWriteMask = 0xFFFFFFFF;
    GLenum m_stencilFail = GL_KEEP;
    GLenum m_stencilPassDepthFail = GL_KEEP;
    GLenum m_stencilPassDepthPass = GL_KEEP;
    GLint m_clearStencil = 0;

    // Capabilities
    std::unordered_map<GLenum, GLboolean> m_capabilities = {
        {GL_DEPTH_TEST, GL_TRUE}, // Default enabled for backward compatibility
        {GL_CULL_FACE, GL_FALSE}, // Default disabled
        {GL_STENCIL_TEST, GL_FALSE}
    };
    GLenum m_depthFunc = GL_LESS;

    // Stencil Op Helper
    inline void applyStencilOp(GLenum op, uint8_t& val) {
        uint8_t newVal = val;
        switch (op) {
            case GL_KEEP: return;
            case GL_ZERO: newVal = 0; break;
            case GL_REPLACE: newVal = (uint8_t)(m_stencilRef & 0xFF); break;
            case GL_INCR: if (newVal < 255) newVal++; break;
            case GL_DECR: if (newVal > 0) newVal--; break;
            case GL_INVERT: newVal = ~newVal; break;
            case GL_INCR_WRAP: newVal++; break;
            case GL_DECR_WRAP: newVal--; break;
            default: return;
        }
        val = (val & ~m_stencilWriteMask) | (newVal & m_stencilWriteMask);
    }

    inline bool checkStencil(uint8_t stencilVal) {
        // Optimization: Capability check moved to caller
        uint8_t val = stencilVal & m_stencilValueMask;
        uint8_t ref = m_stencilRef & m_stencilValueMask;
        
        switch (m_stencilFunc) {
            case GL_NEVER: return false;
            case GL_LESS: return ref < val;
            case GL_LEQUAL: return ref <= val;
            case GL_GREATER: return ref > val;
            case GL_GEQUAL: return ref >= val;
            case GL_EQUAL: return ref == val;
            case GL_NOTEQUAL: return ref != val;
            case GL_ALWAYS: return true;
            default: return true;
        }
    }

    // Stencil Test Helper
    // Returns true if the fragment passes the stencil test.
    // DOES NOT update stencil buffer (that depends on depth result).
    inline bool checkStencil(int x, int y) {
        if (!m_capabilities[GL_STENCIL_TEST]) return true;
        int idx = y * fbWidth + x;
        return checkStencil(stencilBuffer[idx]);
    }

    inline bool testDepth(float z, float currentDepth) {
        // Optimization: Capability check moved to caller
        switch (m_depthFunc) {
            case GL_NEVER:      return false;
            case GL_LESS:       return z < currentDepth;
            case GL_EQUAL:      return std::abs(z - currentDepth) < EPSILON;
            case GL_LEQUAL:     return z <= currentDepth;
            case GL_GREATER:    return z > currentDepth;
            case GL_NOTEQUAL:   return std::abs(z - currentDepth) > EPSILON;
            case GL_GEQUAL:     return z >= currentDepth;
            case GL_ALWAYS:     return true;
            default:            return z < currentDepth;
        }
    }

    GLuint getBufferID(GLenum target) {
        if (target == GL_ARRAY_BUFFER) return m_boundArrayBuffer;
        if (target == GL_ELEMENT_ARRAY_BUFFER) {
             VertexArrayObject* v = vaos.get(m_boundVertexArray);
             return v ? v->elementBufferID : 0;
        }
        if (target == GL_COPY_READ_BUFFER) return m_boundCopyReadBuffer;
        if (target == GL_COPY_WRITE_BUFFER) return m_boundCopyWriteBuffer;
        return 0;
    }

    // Depth Test Helper
    // Returns true if the fragment passes the depth test.
    // Automatically updates the depth buffer if the test passes and Depth Test is enabled.
    inline bool checkDepth(float z, float& currentDepth) {
        if (!m_capabilities[GL_DEPTH_TEST]) return true;

        bool pass = testDepth(z, currentDepth);

        if (pass && m_depthMask) {
            currentDepth = z;
        }
        return pass;
    }
public:
    SoftRenderContext(GLsizei width, GLsizei height) {
        fbWidth = width;
        fbHeight = height;
        // Viewport 初始化
        m_viewport = {0, 0, fbWidth, fbHeight};
        
        vaos.forceAllocate(0); 
        colorBuffer.resize(fbWidth * fbHeight, COLOR_BLACK); // 黑色背景
        m_colorBufferPtr = colorBuffer.data();               // Default to internal buffer

        // 【Fix 1】: Depth 初始化为极大值，确保 z=1.0 的物体能通过测试
        depthBuffer.resize(fbWidth * fbHeight, DEPTH_INFINITY); 
        // Stencil Init
        stencilBuffer.resize(fbWidth * fbHeight, 0);

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

    void glViewport(GLint x, GLint y, GLsizei w, GLsizei h);
    // 设置多边形模式 API
    void glPolygonMode(GLenum face, GLenum mode);

    void glCullFace(GLenum mode) {
        m_cullFaceMode = mode;
    }

    void glFrontFace(GLenum mode) {
        m_frontFace = mode;
    }
    
    void glDepthMask(GLboolean flag){
        m_depthMask = flag;
    }

    void glStencilFunc(GLenum func, GLint ref, GLuint mask){
        m_stencilFunc = func;
        m_stencilRef = ref;
        m_stencilValueMask = mask;
    }
    void glStencilOp(GLenum fail, GLenum zfail, GLenum zpass){
        m_stencilFail = fail;
        m_stencilPassDepthFail = zfail;
        m_stencilPassDepthPass = zpass;
    }
    void glStencilMask(GLuint mask){
        m_stencilWriteMask = mask;
    }
    void glClearStencil(GLint s){
        m_clearStencil = s;
    }

    void glEnable(GLenum cap) {
        m_capabilities[cap] = GL_TRUE;
    }

    void glDisable(GLenum cap) {
        m_capabilities[cap] = GL_FALSE;
    }

    GLboolean glIsEnabled(GLenum cap) {
        auto it = m_capabilities.find(cap);
        if (it != m_capabilities.end()) return it->second;
        return GL_FALSE;
    }

    void glDepthFunc(GLenum func) {
        m_depthFunc = func;
    }

    // --- State Management ---
    void glClearColor(float r, float g, float b, float a);
    // Clear buffer function
    void glClear(uint32_t buffersToClear);
    // Get color buffer for external display
    uint32_t* getColorBuffer() { return m_colorBufferPtr; }
    
    GLsizei getWidth() const { return fbWidth; }
    GLsizei getHeight() const { return fbHeight; }
    const Viewport& glGetViewport() const { return m_viewport; }

    // --- Buffers ---
    void glGenBuffers(GLsizei n, GLuint* res);
    void glDeleteBuffers(GLsizei n, const GLuint* buffers);
    void glBindBuffer(GLenum target, GLuint buffer);
    void glBufferData(GLenum target, GLsizei size, const void* data, GLenum usage);
    void glBufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
    void glCopyBufferSubData(GLenum readTarget, GLenum writeTarget, GLintptr readOffset, GLintptr writeOffset, GLsizeiptr size);
    void* glMapBuffer(GLenum target, GLenum access);
    GLboolean glUnmapBuffer(GLenum target);

    // --- DSA Buffers (Phase 1) ---
    void glCreateBuffers(GLsizei n, GLuint* buffers);
    void glNamedBufferStorage(GLuint buffer, GLsizeiptr size, const void* data, GLbitfield flags);

    // --- Vertex Arrays ---
    void glGenVertexArrays(GLsizei n, GLuint* res);
    void glDeleteVertexArrays(GLsizei n, const GLuint* arrays);
    void glBindVertexArray(GLuint array) { m_boundVertexArray = array; }
    VertexArrayObject& getVAO() { 
        VertexArrayObject* v = vaos.get(m_boundVertexArray);
        if(!v) return *vaos.get(0);
        return *v;
    }
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* pointer);
    void glEnableVertexAttribArray(GLuint index);
    
    // --- DSA Vertex Arrays (Phase 1 & 2) ---
    void glCreateVertexArrays(GLsizei n, GLuint* arrays);
    void glVertexArrayAttribFormat(GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset);
    void glVertexArrayAttribBinding(GLuint vaobj, GLuint attribindex, GLuint bindingindex);
    void glVertexArrayVertexBuffer(GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride);
    void glVertexArrayElementBuffer(GLuint vaobj, GLuint buffer);
    void glEnableVertexArrayAttrib(GLuint vaobj, GLuint index);
    void glVertexAttribDivisor(GLuint index, GLuint divisor);
    Vec4 fetchAttribute(const ResolvedAttribute& attr, int vertexIdx, int instanceIdx);

    // --- Textures ---
    void glGenTextures(GLsizei n, GLuint* res);
    void glDeleteTextures(GLsizei n, const GLuint* textures);
    void glActiveTexture(GLenum texture);
    void glBindTexture(GLenum target, GLuint texture);
    TextureObject* getTexture(GLuint unit);
    TextureObject* getTextureObject(GLuint id);
    void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* p);
    void glTexParameteri(GLenum target, GLenum pname, GLint param); // 设置纹理参数
    void glTexParameterf(GLenum target, GLenum pname, GLfloat param); // Float Scalar 版本
    void glTexParameteriv(GLenum target, GLenum pname, const GLint* params); // nt Vector 版本
    void glTexParameterfv(GLenum target, GLenum pname, const GLfloat* params); // Float Vector 版本 (关键：Border Color)
    void glGenerateMipmap(GLenum target); // 生成 Mipmap

    // --- Draw Execution Helpers ---
    void prepareDraw();

    // --- util ---
    // 线性插值辅助函数 (Linear Interpolation)
    // 用于在被裁剪的边上生成新的顶点
    // t: [0, 1] 插值系数
    VOut lerpVertex(const VOut& a, const VOut& b, float t);
    const std::vector<uint32_t>& readIndicesAsInts(GLsizei count, GLenum type, const void* indices_ptr);
    // Converts source pixel data to internal RGBA8888 format
    bool convertToInternalFormat(const void* src_data, GLsizei src_width, GLsizei src_height,
                                 GLenum src_format, GLenum src_type, std::vector<uint32_t>& dst_pixels);
    // 辅助：计算属性 f 在屏幕空间的偏导数
    Gradients calcGradients(const VOut& v0, const VOut& v1, const VOut& v2, float invArea, float f0, float f1, float f2);
    // 执行透视除法与视口变换 (Perspective Division & Viewport)
    // 将 Clip Space (x,y,z,w) -> Screen Space (sx, sy, sz, 1/w)
    void transformToScreen(VOut& v);

    // --- clip ---
    // Sutherland-Hodgman 裁剪算法的核心：针对单个平面进行裁剪
    // inputVerts: 输入的顶点列表
    // planeID: 0=Left, 1=Right, 2=Bottom, 3=Top, 4=Near, 5=Far
    StaticVector<VOut, 16> clipAgainstPlane(const StaticVector<VOut, 16>& inputVerts, int planeID);
    // Liang-Barsky Line Clipping against a single axis-aligned boundary
    // Returns false if the line is completely outside.
    // Modifies t0 and t1 to the new intersection points.
    bool clipLineAxis(float p, float q, float& t0, float& t1);
    // Clips a line against the canonical view volume. Returns the clipped vertices.
    StaticVector<VOut, 16> clipLine(const VOut& v0, const VOut& v1);

    // --- rasterize ---
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

        // Determine Face Orientation
        // In this implementation: Positive Area = CCW, Negative Area = CW
        bool isCCW = area > 0;
        bool isFront = (m_frontFace == GL_CCW) ? isCCW : !isCCW;
        
        if (m_capabilities[GL_CULL_FACE]) {
            if (m_cullFaceMode == GL_FRONT_AND_BACK) return;
            if (m_cullFaceMode == GL_FRONT && isFront) return;
            if (m_cullFaceMode == GL_BACK && !isFront) return;
        }

        // Setup vertex references (handle winding for rasterizer math)
        // If area is negative (CW), we swap v1 and v2 effectively for the math to ensure positive area
        bool swap = area < 0;
        const VOut& tv0 = v0;
        const VOut& tv1 = swap ? v2 : v1;
        const VOut& tv2 = swap ? v1 : v2;
        
        if (swap) area = -area;

        // 面积 <= 0 剔除 (Degenerate)
        // 使用 epsilon 避免浮点误差导致的闪烁
        if (area <= 1e-6f) return;
        float invArea = 1.0f / area;

        // 3. 增量系数 Setup
        // Edge 0: tv1 -> tv2
        float A0 = tv2.scn.y - tv1.scn.y; float B0 = tv1.scn.x - tv2.scn.x;
        // Edge 1: tv2 -> tv0
        float A1 = tv0.scn.y - tv2.scn.y; float B1 = tv2.scn.x - tv0.scn.x;
        // Edge 2: tv0 -> tv1
        float A2 = tv1.scn.y - tv0.scn.y; float B2 = tv0.scn.x - tv1.scn.x;

        // 4. [关键优化] 预计算透视修正后的 Varyings
        // 原理：在三角形 Setup 阶段，先计算好 (Attr * 1/w_clip)
        // 这样在像素循环中，只需要做线性组合，不需要做额外的乘法
        // 我们利用 SIMD 寄存器数组在栈上存储这些预处理数据
        Simd4f preVar0[MAX_VARYINGS];
        Simd4f preVar1[MAX_VARYINGS];
        Simd4f preVar2[MAX_VARYINGS];

        // 广播 1/w_clip 到 SIMD 寄存器
        Simd4f w0_vec(tv0.scn.w);
        Simd4f w1_vec(tv1.scn.w);
        Simd4f w2_vec(tv2.scn.w);

        // 循环展开预处理所有 Varyings
        for (int k = 0; k < MAX_VARYINGS; ++k) {
            preVar0[k] = Simd4f::load(tv0.ctx.varyings[k]) * w0_vec;
            preVar1[k] = Simd4f::load(tv1.ctx.varyings[k]) * w1_vec;
            preVar2[k] = Simd4f::load(tv2.ctx.varyings[k]) * w2_vec;
        }

        // 5. 初始权重计算 (Pixel Center)
        float startX = minX + 0.5f;
        float startY = minY + 0.5f;
        auto edgeFunc = [](float ax, float ay, float bx, float by, float px, float py) {
            return (by - ay) * (px - ax) - (bx - ax) * (py - ay);
        };
        float w0_row = edgeFunc(tv1.scn.x, tv1.scn.y, tv2.scn.x, tv2.scn.y, startX, startY);
        float w1_row = edgeFunc(tv2.scn.x, tv2.scn.y, tv0.scn.x, tv0.scn.y, startX, startY);
        float w2_row = edgeFunc(tv0.scn.x, tv0.scn.y, tv1.scn.x, tv1.scn.y, startX, startY);

        // Optimization: Cache capability flags
        bool enableDepthTest = m_capabilities[GL_DEPTH_TEST];
        bool enableStencilTest = m_capabilities[GL_STENCIL_TEST];

        // 6. 像素遍历循环
        for (int y = minY; y <= maxY; ++y) {
            float w0 = w0_row; float w1 = w1_row; float w2 = w2_row;
            
            // 直接计算指针偏移，避免乘法
            uint32_t pixelOffset = y * fbWidth + minX;
            uint32_t* pColor = m_colorBufferPtr + pixelOffset;
            float* pDepth = depthBuffer.data() + pixelOffset;
            uint8_t* pStencil = stencilBuffer.data() + pixelOffset;

            // [优化] 将 fsIn 提到循环外，避免每次循环都构造 memset
            ShaderContext fsIn; 
            for (int x = minX; x <= maxX; ++x) {
                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    float alpha = w0 * invArea;
                    float beta  = w1 * invArea;
                    float gamma = w2 * invArea;

                    float zInv = alpha * tv0.scn.w + beta * tv1.scn.w + gamma * tv2.scn.w;
                    
                    if (zInv > 1e-6f) {
                        float z = 1.0f / zInv;
                        
                        // 1. Early-Z Optimization (Read-only)
                        bool earlyZPass = true;
                        if (enableDepthTest) {
                            earlyZPass = testDepth(z, *pDepth);
                        }

                        if (earlyZPass) {
                            // 2. Fragment Interpolation
                            fsIn.rho = 0;

                            Simd4f z_vec(z);
                            Simd4f alpha_vec(alpha);
                            Simd4f beta_vec(beta);
                            Simd4f gamma_vec(gamma);

                            for (int k = 0; k < MAX_VARYINGS; ++k) {
                                Simd4f res = preVar0[k] * alpha_vec;
                                res = res.madd(preVar1[k], beta_vec);
                                res = res.madd(preVar2[k], gamma_vec);
                                res = res * z_vec;
                                res.store(fsIn.varyings[k]);
                            }

                            // --- LOD Calculation ---
                            float dADX = A0 * invArea; float dBDX = A1 * invArea; float dGDX = A2 * invArea;
                            float dADY = B0 * invArea; float dBDY = B1 * invArea; float dGDY = B2 * invArea;
                            float dZwDX = dADX * tv0.scn.w + dBDX * tv1.scn.w + dGDX * tv2.scn.w;
                            float dZwDY = dADY * tv0.scn.w + dBDY * tv1.scn.w + dGDY * tv2.scn.w;
                            Vec4 uv0 = tv0.ctx.varyings[0] * tv0.scn.w;
                            Vec4 uv1 = tv1.ctx.varyings[0] * tv1.scn.w;
                            Vec4 uv2 = tv2.ctx.varyings[0] * tv2.scn.w;
                            Vec4 dUVwDX = uv0 * dADX + uv1 * dBDX + uv2 * dGDX;
                            Vec4 dUVwDY = uv0 * dADY + uv1 * dBDY + uv2 * dGDY;
                            fsIn.rho = computeRho(z, dUVwDX, dUVwDY, dZwDX, dZwDY, fsIn.varyings[0].x, fsIn.varyings[0].y);

                            // 3. Fragment Shader
                            // Setup Builtins
                            shader.gl_FragCoord = Vec4(x + 0.5f, y + 0.5f, z, zInv); 
                            shader.gl_FrontFacing = isFront;
                            shader.gl_Discard = false;
                            shader.gl_FragDepth.written = false;

                            shader.fragment(fsIn);
                            Vec4 fColor = shader.gl_FragColor;

                            // 4. Discard Check
                            if (!shader.gl_Discard) {
                                float finalZ = shader.gl_FragDepth.written ? shader.gl_FragDepth.value : z;

                                bool stencilPass = true;
                                bool depthPass = true;

                                // Optimization: Only re-test depth if FragDepth was written.
                                // If not written, we rely on Early-Z result (which must have been true to get here).
                                bool needDepthTest = enableDepthTest && shader.gl_FragDepth.written;

                                if (enableStencilTest) {
                                    if (!checkStencil(*pStencil)) {
                                        applyStencilOp(m_stencilFail, *pStencil);
                                        stencilPass = false;
                                    } else {
                                        // Stencil Passed, check Depth for Stencil Op
                                        if (needDepthTest && !testDepth(finalZ, *pDepth)) {
                                            applyStencilOp(m_stencilPassDepthFail, *pStencil);
                                            depthPass = false;
                                        } else {
                                            applyStencilOp(m_stencilPassDepthPass, *pStencil);
                                            depthPass = true;
                                        }
                                    }
                                } else {
                                    // No Stencil, just check Depth
                                    if (needDepthTest && !testDepth(finalZ, *pDepth)) {
                                        depthPass = false;
                                    }
                                }

                                if (stencilPass && depthPass) {
                                    if (m_depthMask) *pDepth = finalZ;
                                    *pColor = ColorUtils::FloatToUint32(fColor);
                                }
                            }
                        }
                    }
                }
                
                // X轴增量
                w0 += A0; w1 += A1; w2 += A2;
                pColor++; pDepth++; pStencil++;
            }
            // Y轴增量
            w0_row += B0; w1_row += B1; w2_row += B2;
        }
    }

    // Helper for LOD calculation
    // Computes the maximum rate of change (rho) of UV coordinates in screen space
    inline float computeRho(float z, const Vec4& dUVwDX, const Vec4& dUVwDY, float dZwDX, float dZwDY, float u, float v) {
        // Chain rule for perspective correct interpolation derivatives:
        // du/dx = z * ( d(u/w)/dx - u * d(1/w)/dx )
        float dudx = z * (dUVwDX.x - u * dZwDX);
        float dvdx = z * (dUVwDX.y - v * dZwDX);
        float dudy = z * (dUVwDY.x - u * dZwDY);
        float dvdy = z * (dUVwDY.y - v * dZwDY);
        
        // rho = max( length(du/dx, dv/dx), length(du/dy, dv/dy) )
        // Using squared lengths to avoid one sqrt if possible, but log2 needs linear scale.
        // sqrt(max(lenSqX, lenSqY))
        float rhoX2 = dudx*dudx + dvdx*dvdx;
        float rhoY2 = dudy*dudy + dvdy*dvdy;
        return std::sqrt(std::max(rhoX2, rhoY2));
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

        // Optimization: Cache capability flags
        bool enableDepthTest = m_capabilities[GL_DEPTH_TEST];
        bool enableStencilTest = m_capabilities[GL_STENCIL_TEST];

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
                    
                    // 1. Early-Z Optimization
                    bool earlyZPass = true;
                    if (enableDepthTest) {
                        earlyZPass = testDepth(z, depthBuffer[pix]);
                    }

                    if (earlyZPass) {
                        // 2. Interpolate
                        ShaderContext fsIn;
                        fsIn.rho = 0; // Lines usually don't have well defined Rho without extra math

                        float w_t0 = v0.scn.w * (1.0f - t) * z;
                        float w_t1 = v1.scn.w * t * z;

                        for(int k=0; k<MAX_VARYINGS; ++k) {
                            fsIn.varyings[k] = v0.ctx.varyings[k] * w_t0 + v1.ctx.varyings[k] * w_t1;
                        }

                        // 3. Shader
                        // Setup Builtins
                        shader.gl_FragCoord = Vec4(x0 + 0.5f, y0 + 0.5f, z, zInv); 
                        shader.gl_FrontFacing = true;
                        shader.gl_Discard = false;
                        shader.gl_FragDepth.written = false;

                        shader.fragment(fsIn);
                        Vec4 fColor = shader.gl_FragColor;

                        // 4. Discard & Late-Z
                        if (!shader.gl_Discard) {
                            float finalZ = shader.gl_FragDepth.written ? shader.gl_FragDepth.value : z;
                            
                            bool stencilPass = true;
                            bool depthPass = true;

                            bool needDepthTest = enableDepthTest && shader.gl_FragDepth.written;

                            if (enableStencilTest) {
                                if (!checkStencil(stencilBuffer[pix])) {
                                    applyStencilOp(m_stencilFail, stencilBuffer[pix]);
                                    stencilPass = false;
                                } else {
                                    if (needDepthTest && !testDepth(finalZ, depthBuffer[pix])) {
                                        applyStencilOp(m_stencilPassDepthFail, stencilBuffer[pix]);
                                        depthPass = false;
                                    } else {
                                        applyStencilOp(m_stencilPassDepthPass, stencilBuffer[pix]);
                                        depthPass = true;
                                    }
                                }
                            } else {
                                if (needDepthTest && !testDepth(finalZ, depthBuffer[pix])) {
                                    depthPass = false;
                                }
                            }

                            if (stencilPass && depthPass) {
                                if (m_depthMask) depthBuffer[pix] = finalZ;
                                m_colorBufferPtr[pix] = ColorUtils::FloatToUint32(fColor);
                            }
                        }
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
        
        // v.scn.z 存储的是 Z/W (NDC depth range 0-1 if transformed correctly, or clip z)
        float z = v.scn.z; 

        // Optimization: Cache capability flags
        bool enableDepthTest = m_capabilities[GL_DEPTH_TEST];
        bool enableStencilTest = m_capabilities[GL_STENCIL_TEST];

        // 1. Early-Z
        bool earlyZPass = true;
        if (enableDepthTest) {
            earlyZPass = testDepth(z, depthBuffer[pix]);
        }

        if (earlyZPass) {
            // 2. Setup Context
            ShaderContext fsIn = v.ctx; 
            fsIn.rho = 0;

            // 3. Fragment Shader
            // Setup Builtins
            shader.gl_FragCoord = Vec4(x + 0.5f, y + 0.5f, z, v.scn.w); 
            shader.gl_FrontFacing = true;
            shader.gl_Discard = false;
            shader.gl_FragDepth.written = false;

            shader.fragment(fsIn);
            Vec4 fColor = shader.gl_FragColor;

            // 4. Discard & Late-Z
            if (!shader.gl_Discard) {
                float finalZ = shader.gl_FragDepth.written ? shader.gl_FragDepth.value : z;

                bool stencilPass = true;
                bool depthPass = true;

                bool needDepthTest = enableDepthTest && shader.gl_FragDepth.written;

                if (enableStencilTest) {
                    if (!checkStencil(stencilBuffer[pix])) {
                        applyStencilOp(m_stencilFail, stencilBuffer[pix]);
                        stencilPass = false;
                    } else {
                        if (needDepthTest && !testDepth(finalZ, depthBuffer[pix])) {
                            applyStencilOp(m_stencilPassDepthFail, stencilBuffer[pix]);
                            depthPass = false;
                        } else {
                            applyStencilOp(m_stencilPassDepthPass, stencilBuffer[pix]);
                            depthPass = true;
                        }
                    }
                } else {
                    if (needDepthTest && !testDepth(finalZ, depthBuffer[pix])) {
                        depthPass = false;
                    }
                }

                if (stencilPass && depthPass) {
                    if (m_depthMask) depthBuffer[pix] = finalZ;
                    m_colorBufferPtr[pix] = ColorUtils::FloatToUint32(fColor);
                }
            }
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
                if (vao.bakedAttributes[a].enabled) {
                    attribs[a] = fetchAttribute(vao.bakedAttributes[a], idx, instanceID);
                }
            }

            ShaderContext ctx;
            // 调用 Shader (传入数组指针)
            // 如果 Shader 需要 gl_InstanceID，通常需要修改 shader.vertex 签名或者作为 uniform 传入
            // 这里我们保持接口不变，仅通过 Attribute Divisor 支持 Instancing
            shader.vertex(attribs, ctx);
            VOut v = {.pos = shader.gl_Position, .ctx = ctx};
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
            if (vao.bakedAttributes[a].enabled) {
                attribs[a] = fetchAttribute(vao.bakedAttributes[a], idx, instanceID);
            }
        }
        ShaderContext ctx;
        shader.vertex(attribs, ctx);
        VOut v = {.pos = shader.gl_Position, .ctx = ctx};

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
                if (vao.bakedAttributes[a].enabled) {
                    attribs[a] = fetchAttribute(vao.bakedAttributes[a], indices[i], instanceID);
                }
            }
            ShaderContext ctx;
            shader.vertex(attribs, ctx);
            verts[i].pos = shader.gl_Position;
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
        
        prepareDraw();

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
        
        prepareDraw();

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
        
        prepareDraw();

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
            BufferObject* bufferPtr = buffers.get(vao.elementBufferID);
            if (bufferPtr) {
                const auto& buffer = *bufferPtr;
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

}