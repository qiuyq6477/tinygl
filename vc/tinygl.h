/**
 * Professional Software Rasterizer (Final Stable Version)
 * -------------------------------------------------------
 * Fixes:
 * - Solved [glTexImage2D] error by properly initializing Texture objects in glGenTextures.
 * - Added auto-creation logic in glBindTexture for robustness.
 * - Confirmed execution flow via logs.
 */

#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstring> // for memcpy
#include <cstdint>
#include <cmath>
#include <functional>
#include <algorithm>
#include <string>
#include <memory>
#include <iomanip>
#include <random>



// ==========================================
// 1. 日志系统 (Logging System)
// ==========================================
enum LogLevel { INFO, WARN, ERRR };

class Logger {
public:
    static void log(LogLevel level, const std::string& funcName, const std::string& msg) {
        switch (level) {
            case INFO: std::cout << "[INFO]  "; break;
            case WARN: std::cout << "[WARN]  "; break;
            case ERRR: std::cerr << "[ERROR] "; break;
        }
        std::cout << "[" << funcName << "] " << msg << std::endl;
    }
};

#define LOG_INFO(msg) Logger::log(INFO, __FUNCTION__, msg)
#define LOG_WARN(msg) Logger::log(WARN, __FUNCTION__, msg)
#define LOG_ERROR(msg) Logger::log(ERRR, __FUNCTION__, msg)

// Buffer Type for glClear
enum BufferType {
    COLOR = 1 << 0,
    DEPTH = 1 << 1,
    STENCIL = 1 << 2, // Not implemented
};

// ==========================================
// 2. 数学库
// ==========================================
struct Vec2 { float x, y; };
struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    
    Vec4 operator+(const Vec4& v) const { return {x+v.x, y+v.y, z+v.z, w+v.w}; }
    Vec4 operator-(const Vec4& v) const { return {x-v.x, y-v.y, z-v.z, w-v.w}; }
    Vec4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
    Vec4 operator*(const Vec4& v) const { return {x*v.x, y*v.y, z*v.z, w*v.w}; }
};

struct Mat4 {
    float m[16];

    // 初始化为单位矩阵
    static Mat4 Identity() {
        Mat4 res = {0};
        res.m[0] = res.m[5] = res.m[10] = res.m[15] = 1.0f;
        return res;
    }

    // 平移矩阵
    static Mat4 Translate(float x, float y, float z) {
        Mat4 res = Identity();
        res.m[12] = x; res.m[13] = y; res.m[14] = z; // Column-major layout: translation is in last column
        return res;
    }

    // 缩放矩阵
    static Mat4 Scale(float x, float y, float z) {
        Mat4 res = Identity();
        res.m[0] = x; res.m[5] = y; res.m[10] = z;
        return res;
    }

    // 透视投影矩阵 (简易版 FOV=90, Aspect=1)
    // 使得 Z 坐标产生透视效果 (w = -z)
    static Mat4 Perspective(float fovInDegrees, float aspect, float near, float far) {
        Mat4 res = {0};
        float fovInRadians = fovInDegrees * M_PI / 180.0f;
        float f = 1.0f / std::tan(fovInRadians * 0.5f);
        res.m[0] = f / aspect;
        res.m[5] = f;
        res.m[10] = (far + near) / (near - far);
        res.m[11] = -1.0f; // Convention: Camera looks towards -Z
        res.m[14] = (2.0f * far * near) / (near - far);
        return res;
    }

    // 旋转矩阵 (绕 X 轴)
    static Mat4 RotateX(float angleInDegrees) {
        Mat4 res = Identity();
        float rad = angleInDegrees * M_PI / 180.0f;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[5] = c;  res.m[6] = s;
        res.m[9] = -s; res.m[10] = c;
        return res;
    }

    // 旋转矩阵 (绕 Y 轴)
    static Mat4 RotateY(float angleInDegrees) {
        Mat4 res = Identity();
        float rad = angleInDegrees * M_PI / 180.0f;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[0] = c;   res.m[2] = -s;
        res.m[8] = s;   res.m[10] = c;
        return res;
    }

    // 旋转矩阵 (绕 Z 轴)
    static Mat4 RotateZ(float angleInDegrees) {
        Mat4 res = Identity();
        float rad = angleInDegrees * M_PI / 180.0f;
        float c = std::cos(rad);
        float s = std::sin(rad);
        res.m[0] = c;  res.m[1] = s;
        res.m[4] = -s; res.m[5] = c;
        return res;
    }


    // 矩阵乘法 (A * B)
    Mat4 operator*(const Mat4& r) const {
        Mat4 res = {0};
        for (int row = 0; row < 4; ++row) {
            for (int col = 0; col < 4; ++col) {
                float sum = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    // Column-major access: m[col * 4 + row]
                    // But our struct is flat array, let's assume standard OpenGL Column-Major order:
                    // Index = col * 4 + row
                    sum += m[k*4 + row] * r.m[col*4 + k];
                }
                res.m[col*4 + row] = sum;
            }
        }
        return res;
    }

    // 矩阵 * 向量
    Vec4 operator*(const Vec4& v) const {
        // x = m0*vx + m4*vy + m8*vz + m12*vw
        return Vec4(
            m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        );
    }
};

// ==========================================
// 3. 资源定义
// ==========================================
using GLenum = uint32_t;
using GLuint = uint32_t;
using GLint = int32_t;
using GLsizei = int32_t;
using GLboolean = bool;
using GLfloat = float;

const GLenum GL_ARRAY_BUFFER = 0x8892;
const GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
const GLenum GL_STATIC_DRAW = 0x88E4;
const GLenum GL_FLOAT = 0x1406;
const GLenum GL_TRIANGLES = 0x0004;
const GLenum GL_TEXTURE_2D = 0x0DE1;
const GLenum GL_RGBA = 0x1908;
const GLenum GL_UNSIGNED_BYTE = 0x1401;

// 纹理单元常量
const GLenum GL_TEXTURE0 = 0x84C0;
const GLenum GL_TEXTURE1 = 0x84C1;
const GLenum GL_TEXTURE2 = 0x84C2;
const GLenum GL_TEXTURE3 = 0x84C3;
const GLenum GL_TEXTURE4 = 0x84C4;
const GLenum GL_TEXTURE5 = 0x84C5;
const GLenum GL_TEXTURE6 = 0x84C6;
const GLenum GL_TEXTURE7 = 0x84C7;
const GLenum GL_TEXTURE8 = 0x84C8;
const GLenum GL_TEXTURE9 = 0x84C9;
const GLenum GL_TEXTURE10 = 0x84CA;
const GLenum GL_TEXTURE11 = 0x84CB;
const GLenum GL_TEXTURE12 = 0x84CC;
const GLenum GL_TEXTURE13 = 0x84CD;
const GLenum GL_TEXTURE14 = 0x84CE;
const GLenum GL_TEXTURE15 = 0x84CF;

// 纹理参数枚举
const GLenum GL_TEXTURE_WRAP_S = 0x2802;
const GLenum GL_TEXTURE_WRAP_T = 0x2803;
const GLenum GL_TEXTURE_MIN_FILTER = 0x2801;
const GLenum GL_TEXTURE_MAG_FILTER = 0x2800;

// Wrap Modes
const GLenum GL_REPEAT = 0x2901;
const GLenum GL_CLAMP_TO_EDGE = 0x812F;
const GLenum GL_MIRRORED_REPEAT = 0x8370;

// Filter Modes
const GLenum GL_NEAREST = 0x2600;
const GLenum GL_LINEAR = 0x2601;
const GLenum GL_NEAREST_MIPMAP_NEAREST = 0x2700;
const GLenum GL_LINEAR_MIPMAP_NEAREST = 0x2701;
const GLenum GL_NEAREST_MIPMAP_LINEAR = 0x2702;
const GLenum GL_LINEAR_MIPMAP_LINEAR = 0x2703;

// System Limits & Defaults
constexpr int MAX_ATTRIBS       = 16;   // 最大顶点属性数量
constexpr int MAX_VARYINGS      = 8;    // 最大插值变量数量
constexpr int MAX_TEXTURE_UNITS = 16;   // 最大纹理单元数量

// Math & Colors
constexpr float EPSILON         = 1e-5f;
constexpr float DEPTH_INFINITY  = 1e9f;
constexpr uint32_t COLOR_BLACK  = 0xFF000000; // AABBGGRR (Alpha=255, RGB=0)
constexpr uint32_t COLOR_WHITE  = 0xFFFFFFFF;
constexpr uint32_t COLOR_GREY   = 0xFFAAAAAA;

// Buffer
struct BufferObject {
    std::vector<uint8_t> data;
    GLenum usage = GL_STATIC_DRAW; // Default usage
    
    template <typename T>
    bool readSafe(size_t offset, T& outValue) const {
        if (offset + sizeof(T) > data.size()) {
            static bool logged = false;
            if (!logged) {
                LOG_ERROR("Buffer Read Overflow! Offset: " + std::to_string(offset) + ", Size: " + std::to_string(data.size()));
                logged = true;
            }
            return false;
        }
        std::memcpy(&outValue, data.data() + offset, sizeof(T));
        return true;
    }
};

// Texture
struct TextureObject {
    GLuint id;
    GLsizei width = 0, height = 0;
    
    // Mipmaps 存储：Level 0 是原始图像，Level 1 是 1/2 大小，以此类推
    std::vector<std::vector<uint32_t>> levels; 

    // 纹理参数状态
    GLenum wrapS = GL_REPEAT;
    GLenum wrapT = GL_REPEAT;
    GLenum minFilter = GL_NEAREST_MIPMAP_LINEAR; // 默认三线性
    GLenum magFilter = GL_LINEAR;


    // 生成 Mipmaps (Box Filter 下采样)
    void generateMipmaps() {
        if (levels.empty()) return;
        
        // 确保 Level 0 存在
        int w = width;
        int h = height;
        int currentLevel = 0;

        while (w > 1 || h > 1) {
            const auto& src = levels[currentLevel];
            int nextW = std::max(1, w / 2);
            int nextH = std::max(1, h / 2);
            std::vector<uint32_t> dst(nextW * nextH);

            for (int y = 0; y < nextH; ++y) {
                for (int x = 0; x < nextW; ++x) {
                    // 简单的 2x2 均值采样
                    int srcX = x * 2;
                    int srcY = y * 2;
                    // 边界检查省略，因为总是缩小
                    uint32_t c00 = src[srcY * w + srcX];
                    uint32_t c10 = src[srcY * w + std::min(srcX + 1, w - 1)];
                    uint32_t c01 = src[std::min(srcY + 1, h - 1) * w + srcX];
                    uint32_t c11 = src[std::min(srcY + 1, h - 1) * w + std::min(srcX + 1, w - 1)];

                    auto unpack = [](uint32_t c, int shift) { return (c >> shift) & 0xFF; };
                    auto avg = [&](int shift) {
                        return (unpack(c00, shift) + unpack(c10, shift) + unpack(c01, shift) + unpack(c11, shift)) / 4;
                    };

                    uint8_t R = avg(0);
                    uint8_t G = avg(8);
                    uint8_t B = avg(16);
                    uint8_t A = avg(24);
                    dst[y * nextW + x] = (A << 24) | (B << 16) | (G << 8) | R;
                }
            }

            levels.push_back(std::move(dst));
            w = nextW;
            h = nextH;
            currentLevel++;
        }
        LOG_INFO("Generated " + std::to_string(levels.size()) + " mipmap levels.");
    }

    // 处理 Wrap Mode
    float applyWrap(float val, GLenum mode) const {
        switch (mode) {
            case GL_REPEAT: 
                return val - std::floor(val);
            case GL_MIRRORED_REPEAT: 
                return std::abs(val - 2.0f * std::floor(val / 2.0f)) - 1.0f; // 简化版
            case GL_CLAMP_TO_EDGE: 
                return std::clamp(val, 0.0f, 0.9999f); // 防止触底
            default: return val - std::floor(val);
        }
    }

    // 获取单个像素 (Nearest)
    Vec4 getTexel(int level, int x, int y) const {
        // 安全检查
        if(level < 0 || level >= levels.size()) return {0,0,0,1};
        int w = std::max(1, width >> level);
        int h = std::max(1, height >> level);
        
        // 此处不再处理 Wrap，假设传入的 x, y 已经在 [0, w-1] 范围内
        // 但为了安全起见：
        x = std::clamp(x, 0, w - 1);
        y = std::clamp(y, 0, h - 1);
        
        uint32_t p = levels[level][y * w + x];
        return Vec4((p&0xFF)/255.0f, ((p>>8)&0xFF)/255.0f, ((p>>16)&0xFF)/255.0f, ((p>>24)&0xFF)/255.0f);
    }

    // 双线性插值采样 (Bilinear Interpolation)
    Vec4 sampleBilinear(float u, float v, int level) const {
        if(level >= levels.size()) return {0,0,0,1};
        int w = std::max(1, width >> level);
        int h = std::max(1, height >> level);

        // 应用 Wrap
        float uImg = applyWrap(u, wrapS) * w - 0.5f;
        float vImg = applyWrap(v, wrapT) * h - 0.5f;

        int x0 = (int)std::floor(uImg);
        int y0 = (int)std::floor(vImg);
        int x1 = x0 + 1;
        int y1 = y0 + 1;
        
        // 权重
        float s = uImg - x0;
        float t = vImg - y0;

        // 注意：getTexel 内部有 clamp，所以 wrap 逻辑最好在坐标转整数时处理
        // 对于 REPEAT 模式，如果 x1 越界，应该回到 0。
        auto wrapIdx = [](int idx, int max, GLenum mode) {
             if (mode == GL_REPEAT) return (idx % max + max) % max;
             return std::clamp(idx, 0, max - 1);
        };
        
        x0 = wrapIdx(x0, w, wrapS); x1 = wrapIdx(x1, w, wrapS);
        y0 = wrapIdx(y0, h, wrapT); y1 = wrapIdx(y1, h, wrapT);

        Vec4 c00 = getTexel(level, x0, y0);
        Vec4 c10 = getTexel(level, x1, y0);
        Vec4 c01 = getTexel(level, x0, y1);
        Vec4 c11 = getTexel(level, x1, y1);

        auto lerp = [](const Vec4& a, const Vec4& b, float f) { return a * (1.0f - f) + b * f; };
        return lerp(lerp(c00, c10, s), lerp(c01, c11, s), t);
    }

    // 主采样函数 (根据 LOD 选择 Filter)
    Vec4 sample(float u, float v, float lod = 0.0f) const {
        if (levels.empty()) return {1, 0, 1, 1};

        // 1. Magnification (放大): LOD < 0 (纹理像素比屏幕像素大)
        if (lod <= 0.0f && magFilter == GL_NEAREST) {
            int w = width; int h = height;
            int x = (int)(applyWrap(u, wrapS) * w);
            int y = (int)(applyWrap(v, wrapT) * h);
            return getTexel(0, x, y); // Nearest Base Level
        }
        
        // 2. Minification (缩小): LOD > 0
        // 限制 LOD 范围
        float maxLevel = (float)levels.size() - 1;
        lod = std::clamp(lod, 0.0f, maxLevel);

        // 根据 minFilter 选择策略
        if (minFilter == GL_NEAREST) {
            return sampleBilinear(u, v, 0); // 忽略 Mipmap，仅 Base Level (实际标准是 Nearest on Base)
        }
        if (minFilter == GL_LINEAR) {
             return sampleBilinear(u, v, 0); // Base Level Bilinear
        }
        if (minFilter == GL_NEAREST_MIPMAP_NEAREST) {
            int level = (int)std::round(lod);
            int w = std::max(1, width >> level);
            int h = std::max(1, height >> level);
            int x = (int)(applyWrap(u, wrapS) * w);
            int y = (int)(applyWrap(v, wrapT) * h);
            return getTexel(level, x, y);
        }
        if (minFilter == GL_LINEAR_MIPMAP_NEAREST) {
            int level = (int)std::round(lod);
            return sampleBilinear(u, v, level);
        }
        if (minFilter == GL_NEAREST_MIPMAP_LINEAR) {
            // 在两个 Mipmap 层级间插值，但层级内使用 Nearest
            // (省略实现，较少用)
            return sampleBilinear(u, v, (int)lod); 
        }

        // Default: GL_LINEAR_MIPMAP_LINEAR (Trilinear)
        int levelBase = (int)lod;
        int levelNext = std::min(levelBase + 1, (int)maxLevel);
        float f = lod - levelBase;

        Vec4 cBase = sampleBilinear(u, v, levelBase);
        Vec4 cNext = sampleBilinear(u, v, levelNext);
        return cBase * (1.0f - f) + cNext * f;
    }
};

// VAO Components
struct VertexAttribState {
    bool enabled = false;
    GLuint bindingBufferID = 0;
    GLint size = 3;
    GLenum type = GL_FLOAT;
    GLboolean normalized = false; // Add this line
    GLsizei stride = 0;
    size_t pointerOffset = 0;
};

struct VertexArrayObject {
    VertexAttribState attributes[MAX_ATTRIBS];
    GLuint elementBufferID = 0;
};

// Shader & Program
struct ShaderContext { 
    Vec4 varyings[MAX_VARYINGS]; 
    float lod = 0.0f; // [新增]
};

// VOut: 顶点着色器的输出，也是裁剪阶段的输入
struct VOut { 
    Vec4 pos;       // Clip Space Position (未除以 w)
    Vec4 scn;       // Screen Space Position (已除以 w)
    ShaderContext ctx; 
};

struct UniformValue {
    enum Type { INT, FLOAT, MAT4 } type;
    union { int i; float f; float mat[16]; } data;
};

struct ProgramObject {
    GLuint id;
    std::unordered_map<std::string, GLint> uniformLocs;
    std::unordered_map<GLint, UniformValue> uniforms;
    std::function<Vec4(const std::vector<Vec4>&, ShaderContext&)> vertexShader;
    std::function<Vec4(const ShaderContext&)> fragmentShader;
};

// 辅助：计算平面梯度的结构
struct Gradients {
    float dfdx, dfdy;
};

// ==========================================
// 4. 上下文核心 (Context)
// ==========================================
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
    std::vector<float> depthBuffer;

public:
    SoftRenderContext(GLsizei width, GLsizei height) {
        fbWidth = width;
        fbHeight = height;

        vaos[0] = VertexArrayObject{}; 
        colorBuffer.resize(fbWidth * fbHeight, COLOR_BLACK); // 黑色背景
        // 【Fix 1】: Depth 初始化为极大值，确保 z=1.0 的物体能通过测试
        depthBuffer.resize(fbWidth * fbHeight, DEPTH_INFINITY);       
        LOG_INFO("Context Initialized (" + std::to_string(fbWidth) + "x" + std::to_string(fbHeight) + ")");
    }
    
    // Clear buffer function
    void glClear(uint32_t buffersToClear) {
        if (buffersToClear & BufferType::COLOR) {
            std::fill(colorBuffer.begin(), colorBuffer.end(), COLOR_BLACK);
        }
        if (buffersToClear & BufferType::DEPTH) {
            std::fill(depthBuffer.begin(), depthBuffer.end(), DEPTH_INFINITY);
        }
    }

    // Get color buffer for external display
    uint32_t* getColorBuffer() { return colorBuffer.data(); }

    // --- Accessors ---
    VertexArrayObject& getVAO() { return vaos[m_boundVertexArray]; }
    
    ProgramObject* getCurrentProgram() {
        if (m_currentProgram == 0) return nullptr;
        auto it = programs.find(m_currentProgram);
        return (it != programs.end()) ? &it->second : nullptr;
    }

    ProgramObject* getProgramByID(GLuint id) {
        auto it = programs.find(id);
        return (it != programs.end()) ? &it->second : nullptr;
    }
    
    TextureObject* getTexture(GLuint unit) {
        if (unit >= 32) return nullptr;
        GLuint id = m_boundTextures[unit];
        if (id == 0) return nullptr;
        auto it = textures.find(id);
        return (it != textures.end()) ? &it->second : nullptr;
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
        tex->width = w; 
        tex->height = h;
        // 清空旧数据，初始化 Level 0
        tex->levels.clear();
        tex->levels.resize(1);
        tex->levels[0].resize(w * h);
        
        if(p) memcpy(tex->levels[0].data(), p, w*h*4);
        
        // 自动生成 Mipmaps (模拟 glGenerateMipmap)
        tex->generateMipmaps();
        LOG_INFO("Loaded Texture " + std::to_string(w) + "x" + std::to_string(h) + " with Mipmaps.");
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
        }
    }

    // --- Program ---
    GLuint glCreateProgram() {
        GLuint id = m_nextID++;
        programs[id].id = id;
        LOG_INFO("CreateProgram ID: " + std::to_string(id));
        return id;
    }
    void glUseProgram(GLuint prog) {
        if (prog != 0 && programs.find(prog) == programs.end()) {
            LOG_ERROR("Invalid Program ID");
            return;
        }
        m_currentProgram = prog;
        LOG_INFO("UseProgram: " + std::to_string(prog));
    }

    void setVertexShader(GLuint progID, std::function<Vec4(const std::vector<Vec4>&, ShaderContext&)> shader) {
        if (auto* p = getProgramByID(progID)) p->vertexShader = shader;
    }
    void setFragmentShader(GLuint progID, std::function<Vec4(const ShaderContext&)> shader) {
        if (auto* p = getProgramByID(progID)) p->fragmentShader = shader;
    }

    GLint glGetUniformLocation(GLuint prog, const char* name) {
        programs[prog].uniformLocs[name]; // create entry
        GLint loc = (GLint)std::hash<std::string>{}(name) & 0x7FFFFFFF;
        return loc;
    }

    void glUniform1i(GLint loc, int val) {
        if (auto* p = getCurrentProgram()) {
            p->uniforms[loc].type = UniformValue::INT;
            p->uniforms[loc].data.i = val;
        }
    }
    void glUniformMatrix4fv(GLint loc, GLsizei /*count*/, GLboolean /*transpose*/, const GLfloat* value) {
        if (auto* p = getCurrentProgram()) {
            p->uniforms[loc].type = UniformValue::MAT4;
            std::memcpy(p->uniforms[loc].data.mat, value, 16*sizeof(float));
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

    // Sutherland-Hodgman 裁剪算法的核心：针对单个平面进行裁剪
    // inputVerts: 输入的顶点列表
    // planeID: 0=Left, 1=Right, 2=Bottom, 3=Top, 4=Near, 5=Far
    std::vector<VOut> clipAgainstPlane(const std::vector<VOut>& inputVerts, int planeID) {
        std::vector<VOut> outputVerts;
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

    // 执行透视除法与视口变换 (Perspective Division & Viewport)
    // 将 Clip Space (x,y,z,w) -> Screen Space (sx, sy, sz, 1/w)
    void transformToScreen(VOut& v) {
        float w = v.pos.w;
        // 此时 w 已经被 Near Plane 裁剪过，一定 > EPSILON
        float rhw = 1.0f / w;

        // Viewport Mapping: [-1, 1] -> [0, Width/Height]
        // 注意 Y 轴翻转: (1.0 - y)
        v.scn.x = (v.pos.x * rhw + 1.0f) * 0.5f * fbWidth;
        v.scn.y = (1.0f - v.pos.y * rhw) * 0.5f * fbHeight;
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

    // 纯粹的光栅化三角形逻辑 (Rasterize Triangle)
    // 输入已经是 Screen Space 的三个顶点
    void rasterizeTriangle(const VOut& v0, const VOut& v1, const VOut& v2) {
        auto* prog = getCurrentProgram();
        
        // Bounding Box
        int minX = std::max(0, (int)std::min({v0.scn.x, v1.scn.x, v2.scn.x}));
        int maxX = std::min(fbWidth-1, (int)std::max({v0.scn.x, v1.scn.x, v2.scn.x}) + 1);
        int minY = std::max(0, (int)std::min({v0.scn.y, v1.scn.y, v2.scn.y}));
        int maxY = std::min(fbHeight-1, (int)std::max({v0.scn.y, v1.scn.y, v2.scn.y}) + 1);

        // Edge Function (Y-Down 修正版)
        auto edge = [](const Vec4& a, const Vec4& b, const Vec4& p) {
            return (b.y - a.y) * (p.x - a.x) - (b.x - a.x) * (p.y - a.y);
        };

        // Pre-compute area (Doubled Area)
        float area = edge(v0.scn, v1.scn, v2.scn);
        
        // Backface Culling (CCW)
        if (area <= 0) return;
        float invArea = 1.0f / area;

        // [新增] 1. 预计算 1/w, U/w, V/w 的屏幕空间梯度
        // 假设 UV 存储在 varyings[0] (x=u, y=v)
        // 这里的 varyings 已经是经过 Vertex Shader 的，但还没有除以 w
        // 注意：我们的 v.ctx.varyings 存储的是原始属性（未除w），v.scn.w 存储的是 1/w
        
        // 属性 0: 1/w
        float w0 = v0.scn.w, w1 = v1.scn.w, w2 = v2.scn.w;
        Gradients gradW = calcGradients(v0, v1, v2, invArea, w0, w1, w2);

        // 属性 1: U/w (假设 U 在 varyings[0].x)
        float u0 = v0.ctx.varyings[0].x * w0;
        float u1 = v1.ctx.varyings[0].x * w1;
        float u2 = v2.ctx.varyings[0].x * w2;
        Gradients gradU_w = calcGradients(v0, v1, v2, invArea, u0, u1, u2);

        // 属性 2: V/w (假设 V 在 varyings[0].y)
        float v_0 = v0.ctx.varyings[0].y * w0; // 变量名 v_0 避免冲突
        float v_1 = v1.ctx.varyings[0].y * w1;
        float v_2 = v2.ctx.varyings[0].y * w2;
        Gradients gradV_w = calcGradients(v0, v1, v2, invArea, v_0, v_1, v_2);
        
        // 纹理尺寸 (用于 scaling)
        // 注意：这里需要知道当前绑定的纹理尺寸，稍微有点耦合，
        // 或者在 Shader 中传 LOD，这里只传导数。为简化，假设 TextureUnit 0
        float texW = 256.0f, texH = 256.0f;
        if (auto* t = getTexture(0)) { texW = t->width; texH = t->height; }

        for (int y = minY; y <= maxY; ++y) {
            for (int x = minX; x <= maxX; ++x) {
                Vec4 p((float)x + 0.5f, (float)y + 0.5f, 0, 0);
                
                float w0 = edge(v1.scn, v2.scn, p);
                float w1 = edge(v2.scn, v0.scn, p);
                float w2 = edge(v0.scn, v1.scn, p);

                // Inside Test
                if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                    w0 *= invArea; w1 *= invArea; w2 *= invArea;
                    
                    // Perspective Correct Depth
                    float zInv = w0 * v0.scn.w + w1 * v1.scn.w + w2 * v2.scn.w;
                    float z = 1.0f / zInv;

                    // Depth Test
                    int pix = y * fbWidth + x;
                    if (z < depthBuffer[pix]) {
                        depthBuffer[pix] = z;
                        
                        // [新增] 2. 逐像素计算 LOD
                        // 恢复当前的 g = 1/w
                        float g = w0 * w0 + w1 * w1 + w2 * w2; // 当前像素的 1/w
                        float g2 = g * g;

                        // 利用商法则计算 du/dx, du/dy
                        // d(f/g)/dx = (f'g - fg') / g^2
                        // f = U/w, g = 1/w
                        // f' = gradU_w.dfdx (常数)
                        // g' = gradW.dfdx (常数)
                        
                        // 当前像素的 f = U/w
                        float f_u = w0 * u0 + w1 * u1 + w2 * u2;
                        float f_v = w0 * v_0 + w1 * v_1 + w2 * v_2;

                        float dudx = (gradU_w.dfdx * g - f_u * gradW.dfdx) / g2;
                        float dudy = (gradU_w.dfdy * g - f_u * gradW.dfdy) / g2;
                        float dvdx = (gradV_w.dfdx * g - f_v * gradW.dfdx) / g2;
                        float dvdy = (gradV_w.dfdy * g - f_v * gradW.dfdy) / g2;

                        // 计算 rho (纹理空间距离)
                        float deltaX = std::sqrt(dudx*dudx * texW*texW + dvdx*dvdx * texH*texH);
                        float deltaY = std::sqrt(dudy*dudy * texW*texW + dvdy*dvdy * texH*texH);
                        float rho = std::max(deltaX, deltaY);
                        
                        // 计算 LOD
                        float lod = std::log2(rho);
                        
                        // 填入 Context
                        ShaderContext fsIn;
                        fsIn.lod = lod;

                        // Attribute Interpolation
                        for (int k = 0; k < MAX_VARYINGS; ++k) {
                            // Perspective Correct Interpolation Formula:
                            // Attr = (Attr0/w0 * b0 + Attr1/w1 * b1 + Attr2/w2 * b2) / (1/w_interpolated)
                            Vec4 a = v0.ctx.varyings[k] * v0.scn.w * w0 + 
                                     v1.ctx.varyings[k] * v1.scn.w * w1 + 
                                     v2.ctx.varyings[k] * v2.scn.w * w2;
                            fsIn.varyings[k] = a * z;
                        }

                        // Fragment Shader
                        Vec4 c = prog->fragmentShader(fsIn);
                        
                        // Output
                        uint8_t R = (uint8_t)(std::clamp(c.x, 0.0f, 1.0f) * 255);
                        uint8_t G = (uint8_t)(std::clamp(c.y, 0.0f, 1.0f) * 255);
                        uint8_t B = (uint8_t)(std::clamp(c.z, 0.0f, 1.0f) * 255);
                        colorBuffer[pix] = (255 << 24) | (B << 16) | (G << 8) | R;
                    }
                }
            }
        }
    }

    // --- Rasterizer ---
    Vec4 fetchAttribute(const VertexAttribState& attr, int idx) {
        if (!attr.enabled) return Vec4(0,0,0,1);
        auto it = buffers.find(attr.bindingBufferID);
        if (it == buffers.end()) return Vec4(0,0,0,1);
        
        size_t stride = attr.stride ? attr.stride : attr.size * sizeof(float); // Default stride for floats
        size_t offset = attr.pointerOffset + idx * stride;
        
        float raw[4] = {0,0,0,1};

        switch (attr.type) {
            case GL_FLOAT: {
                for(int i=0; i<attr.size; ++i) {
                    it->second.readSafe<float>(offset + i*sizeof(float), raw[i]);
                }
                break;
            }
            case GL_UNSIGNED_BYTE: {
                stride = attr.stride ? attr.stride : attr.size * sizeof(uint8_t);
                offset = attr.pointerOffset + idx * stride;
                uint8_t ubyte_raw[4] = {0,0,0,255};
                for(int i=0; i<attr.size; ++i) {
                    it->second.readSafe<uint8_t>(offset + i*sizeof(uint8_t), ubyte_raw[i]);
                }
                
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

    // glDrawArrays
    void glDrawArrays(GLenum /*mode*/, GLint first, GLsizei count) {
        auto* prog = getCurrentProgram();
        if (!prog || !prog->vertexShader || !prog->fragmentShader) return;
        
        // 遍历所有三角形 (每次处理3个顶点，线性读取)
        for(int i=0; i<count; i+=3) {
            // 如果剩余顶点不足一个三角形则退出
            if (i + 2 >= count) break;

            std::vector<VOut> triangle(3);
            
            // Step 1: Vertex Processing
            for (int k=0; k<3; ++k) {
                // 直接使用线性索引 (first + i + k)
                int vertexIdx = first + i + k;
                
                std::vector<Vec4> ins(MAX_ATTRIBS);
                VertexArrayObject& vao = getVAO();
                for(int a=0; a<MAX_ATTRIBS; a++) {
                    if(vao.attributes[a].enabled) ins[a] = fetchAttribute(vao.attributes[a], vertexIdx);
                }
                
                ShaderContext ctx;
                triangle[k].pos = prog->vertexShader(ins, ctx);
                triangle[k].ctx = ctx;
            }

            // Step 2: Clipping (复制自 glDrawElements)
            std::vector<VOut> polygon = triangle;
            for (int p = 0; p < 6; ++p) {
                polygon = clipAgainstPlane(polygon, p);
                if (polygon.empty()) break;
            }
            if (polygon.empty()) continue;

            // Step 3: Transform (复制自 glDrawElements)
            for (auto& v : polygon) transformToScreen(v);

            // Step 4: Rasterization (复制自 glDrawElements)
            for (size_t k = 1; k < polygon.size() - 1; ++k) {
                rasterizeTriangle(polygon[0], polygon[k], polygon[k+1]);
            }
        }
    }

    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
        LOG_INFO("DrawElements Count: " + std::to_string(count));
        
        ProgramObject* prog = getCurrentProgram();
        if (!prog || !prog->vertexShader || !prog->fragmentShader) return;
        VertexArrayObject& vao = getVAO();
        if (vao.elementBufferID == 0) return;

        // 1. 读取索引
        std::vector<uint32_t> idxCache(count);
        size_t idxOffset = reinterpret_cast<size_t>(indices);
        for(int i=0; i<count; ++i) {
            buffers[vao.elementBufferID].readSafe<uint32_t>(idxOffset + i*4, idxCache[i]);
        }

        // 遍历所有三角形 (每次处理3个顶点)
        for(int i=0; i<count; i+=3) {
            std::vector<VOut> triangle(3);
            
            // Step 1: Vertex Processing (运行顶点着色器)
            for (int k=0; k<3; ++k) {
                uint32_t vertexIdx = idxCache[i+k];
                std::vector<Vec4> ins(MAX_ATTRIBS);
                for(int a=0; a<MAX_ATTRIBS; a++) {
                    if(vao.attributes[a].enabled) ins[a] = fetchAttribute(vao.attributes[a], vertexIdx);
                }
                
                ShaderContext ctx;
                // 注意：VS 输出的是 Clip Space Position (未除 w)
                triangle[k].pos = prog->vertexShader(ins, ctx);
                triangle[k].ctx = ctx;
            }

            // Step 2: Clipping (裁剪)
            // 将三角形依次通过 6 个平面的裁剪
            // 顺序：Near -> Far -> Left -> Right -> Top -> Bottom
            // 最重要的是 Near Plane，因为它防止 w <= 0 导致的除以零崩溃
            std::vector<VOut> polygon = triangle;
            
            // 依次针对 6 个平面裁剪
            for (int p = 0; p < 6; ++p) {
                polygon = clipAgainstPlane(polygon, p);
                if (polygon.empty()) break; // 如果完全被裁掉了，提前结束
            }
            
            if (polygon.empty()) continue;

            // Step 3: Perspective Division & Viewport (坐标变换)
            // 对裁剪剩下的多边形顶点进行变换
            for (auto& v : polygon) {
                transformToScreen(v);
            }

            // Step 4: Triangulation & Rasterization (扇形剖分与光栅化)
            // 裁剪后的多边形可能是 3, 4, 5... 个顶点 (凸多边形)
            // 我们将其拆解为 (v0, v1, v2), (v0, v2, v3)...
            for (size_t k = 1; k < polygon.size() - 1; ++k) {
                rasterizeTriangle(polygon[0], polygon[k], polygon[k+1]);
            }
        }
        
        LOG_INFO("Draw Finished.");
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


void glDrawElementsSample()
{
    SoftRenderContext ctx(800, 600);

    GLuint progID = ctx.glCreateProgram();
    GLint uMVP = ctx.glGetUniformLocation(progID, "uMVP");
    GLint uTex = ctx.glGetUniformLocation(progID, "uTex");

    // Vertex Shader: 增加对 Color (索引1) 的处理，UV 移至索引 2
    ctx.setVertexShader(progID, [uMVP, &ctx](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        Vec4 pos   = attribs[0]; // Loc 0: 位置
        Vec4 color = attribs[1]; // Loc 1: 颜色 (新增)
        Vec4 uv    = attribs[2]; // Loc 2: UV (原 Loc 1)

        outCtx.varyings[0] = uv;    // 传递 UV
        outCtx.varyings[1] = color; // 传递 颜色 (插值)

        Mat4 mvp = Mat4::Identity();
        if (auto* p = ctx.getCurrentProgram()) {
             if (p->uniforms.count(uMVP)) std::memcpy(mvp.m, p->uniforms[uMVP].data.mat, 16*sizeof(float));
        }
        pos.w = 1.0f;
        return mvp * pos;
    });

    // Fragment Shader: 将插值后的颜色与纹理相乘
    ctx.setFragmentShader(progID, [uTex, &ctx](const ShaderContext& inCtx) -> Vec4 {
        Vec4 uv    = inCtx.varyings[0];
        Vec4 color = inCtx.varyings[1]; // 获取插值后的顶点颜色

        int unit = 0;
        if (auto* p = ctx.getCurrentProgram()) if (p->uniforms.count(uTex)) unit = p->uniforms[uTex].data.i;
        
        Vec4 texColor = {1, 1, 1, 1};
        if (auto* tex = ctx.getTexture(unit)) texColor = tex->sample(uv.x, uv.y);

        // 核心修改: 顶点颜色 * 纹理颜色
        return color * texColor; 
    });

    GLuint tex;
    ctx.glGenTextures(1, &tex);
    ctx.glActiveTexture(GL_TEXTURE0);
    ctx.glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<uint32_t> pixels(256*256);
    for(int y=0; y<256; ++y) for(int x=0; x<256; ++x) {
        pixels[y*256+x] = (((x/32)+(y/32))%2) ? COLOR_WHITE : COLOR_GREY;
    }
    ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    float vertices[] = {
        // Position (XYZ)     // Color (RGB)       // UV (UV)
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,    0.0f, 0.0f, // 左下 (红)
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,    1.0f, 0.0f, // 右下 (绿)
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,    1.0f, 1.0f, // 右上 (蓝)
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,    0.0f, 1.0f  // 左上 (黄)
    };
    uint32_t indices[] = { 0, 1, 2, 2, 3, 0 };

    GLuint vao, vbo, ebo;
    ctx.glGenVertexArrays(1, &vao);
    ctx.glBindVertexArray(vao);
    ctx.glGenBuffers(1, &vbo);
    ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    ctx.glGenBuffers(1, &ebo);
    ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // 计算新的步长: 8个float * 4字节 = 32字节
    GLsizei stride = 8 * sizeof(float);

    // 属性 0: Position (偏移 0)
    ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);
    ctx.glEnableVertexAttribArray(0);

    // 属性 1: Color (新增，偏移 3 * float)
    ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3 * sizeof(float)));
    ctx.glEnableVertexAttribArray(1);

    // 属性 2: UV (原属性1，偏移 6 * float)
    ctx.glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6 * sizeof(float)));
    ctx.glEnableVertexAttribArray(2);

    ctx.glUseProgram(progID);
    ctx.glUniform1i(uTex, 0);
    Mat4 mvp = Mat4::Identity(); mvp.m[0]=1.5f; mvp.m[5]=1.5f;
    ctx.glUniformMatrix4fv(uMVP, 1, false, mvp.m);

    ctx.glDrawElements(GL_TRIANGLES, 6, 0, (void*)0);
    ctx.savePPM("result.ppm");
}

void glDrawArraySample()
{
    SoftRenderContext ctx(800, 600);

    GLuint progID = ctx.glCreateProgram();
    GLint uMVP = ctx.glGetUniformLocation(progID, "uMVP");
    GLint uTex = ctx.glGetUniformLocation(progID, "uTex");

    // Vertex Shader: 增加对 Color (索引1) 的处理，UV 移至索引 2
    ctx.setVertexShader(progID, [uMVP, &ctx](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        Vec4 pos   = attribs[0]; // Loc 0: 位置
        Vec4 color = attribs[1]; // Loc 1: 颜色 (新增)
        Vec4 uv    = attribs[2]; // Loc 2: UV (原 Loc 1)

        outCtx.varyings[0] = uv;    // 传递 UV
        outCtx.varyings[1] = color; // 传递 颜色 (插值)

        Mat4 mvp = Mat4::Identity();
        if (auto* p = ctx.getCurrentProgram()) {
             if (p->uniforms.count(uMVP)) std::memcpy(mvp.m, p->uniforms[uMVP].data.mat, 16*sizeof(float));
        }
        pos.w = 1.0f;
        return mvp * pos;
    });

    // Fragment Shader: 将插值后的颜色与纹理相乘
    ctx.setFragmentShader(progID, [uTex, &ctx](const ShaderContext& inCtx) -> Vec4 {
        Vec4 uv    = inCtx.varyings[0];
        Vec4 color = inCtx.varyings[1]; // 获取插值后的顶点颜色

        int unit = 0;
        if (auto* p = ctx.getCurrentProgram()) if (p->uniforms.count(uTex)) unit = p->uniforms[uTex].data.i;
        
        Vec4 texColor = {1, 1, 1, 1};
        if (auto* tex = ctx.getTexture(unit)) texColor = tex->sample(uv.x, uv.y);

        // 核心修改: 顶点颜色 * 纹理颜色
        return color * texColor; 
    });

    GLuint tex;
    ctx.glGenTextures(1, &tex);
    ctx.glActiveTexture(GL_TEXTURE0);
    ctx.glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<uint32_t> pixels(256*256);
    for(int y=0; y<256; ++y) for(int x=0; x<256; ++x) {
        pixels[y*256+x] = (((x/32)+(y/32))%2) ? COLOR_WHITE : COLOR_GREY;
    }
    ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    
    // --- Vertex Data (VBO Only / Non-Indexed) ---
    // 为了画一个矩形，我们需要2个三角形。
    // 在没有 EBO 的情况下，我们需要手动列出所有 6 个顶点。
    // 顺序: Tri 1 (BL, BR, TR), Tri 2 (TR, TL, BL)
    float vertices[] = {
        // Pos(3)             Color(3)           UV(2)
        // Triangle 1
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  0.0f, 0.0f, // BL (Red)
         0.5f, -0.5f, 0.0f,   0.0f, 1.0f, 0.0f,  1.0f, 0.0f, // BR (Green)
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f, // TR (Blue)

        // Triangle 2
         0.5f,  0.5f, 0.0f,   0.0f, 0.0f, 1.0f,  1.0f, 1.0f, // TR (Blue)
        -0.5f,  0.5f, 0.0f,   1.0f, 1.0f, 0.0f,  0.0f, 1.0f, // TL (Yellow)
        -0.5f, -0.5f, 0.0f,   1.0f, 0.0f, 0.0f,  0.0f, 0.0f  // BL (Red)
    };

    // --- Setup Buffers ---
    // 注意：不再创建 EBO
    GLuint vao, vbo;
    ctx.glGenVertexArrays(1, &vao); ctx.glBindVertexArray(vao);
    ctx.glGenBuffers(1, &vbo); ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // --- Configure Attributes ---
    GLsizei stride = 8 * sizeof(float);
    ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)0);                   ctx.glEnableVertexAttribArray(0);
    ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, stride, (void*)(3*sizeof(float)));   ctx.glEnableVertexAttribArray(1);
    ctx.glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)(6*sizeof(float)));   ctx.glEnableVertexAttribArray(2);

    // --- Draw ---
    ctx.glUseProgram(progID);
    ctx.glUniform1i(uTex, 0);
    Mat4 mvp = Mat4::Identity(); mvp.m[0]=1.5f; mvp.m[5]=1.5f;
    ctx.glUniformMatrix4fv(uMVP, 1, false, mvp.m);

    // 使用 glDrawArrays 绘制 6 个顶点
    ctx.glDrawArrays(GL_TRIANGLES, 0, 6);
    
    ctx.savePPM("result_arrays.ppm");
}


void drawCubeSample() {
    int windowWidth = 800, windowHeight = 600;
    SoftRenderContext ctx(windowWidth, windowHeight);
    GLuint progID = ctx.glCreateProgram();
    GLint uMVP = ctx.glGetUniformLocation(progID, "uMVP");
    GLint uTex = ctx.glGetUniformLocation(progID, "uTex");

    // 1. 设置 Shader (逻辑保持不变，依赖传入的 MVP 矩阵)
    ctx.setVertexShader(progID, [uMVP, &ctx](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        outCtx.varyings[0] = attribs[2]; // UV
        outCtx.varyings[1] = attribs[1]; // Color
        
        Mat4 mvp = Mat4::Identity();
        if (auto* p = ctx.getCurrentProgram()) {
             if (p->uniforms.count(uMVP)) std::memcpy(mvp.m, p->uniforms[uMVP].data.mat, 16*sizeof(float));
        }
        Vec4 pos = attribs[0]; 
        pos.w = 1.0f;
        return mvp * pos; // MVP 变换
    });

    ctx.setFragmentShader(progID, [uTex, &ctx](const ShaderContext& inCtx) -> Vec4 {
        int unit = 0;
        if (auto* p = ctx.getCurrentProgram()) if (p->uniforms.count(uTex)) unit = p->uniforms[uTex].data.i;
        Vec4 texColor = {1,1,1,1};
        if (auto* tex = ctx.getTexture(unit)) texColor = tex->sample(inCtx.varyings[0].x, inCtx.varyings[0].y, inCtx.lod);
        // 混合顶点颜色和纹理
        return inCtx.varyings[1] * texColor;
    });

    // 2. 生成简单的纹理 (白/蓝 格子)
    GLuint tex; ctx.glGenTextures(1, &tex); ctx.glActiveTexture(GL_TEXTURE0); ctx.glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<uint32_t> pixels(256 * 256);
    for(int i=0; i<256*256; i++) 
        pixels[i] = (((i%256/32)+(i/256/32))%2) ? COLOR_WHITE : 0xFF000000; // White / Black
    ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    // 设置纹理参数
    ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    ctx.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    
    // 3. 构建 Cube 数据 (修正版：24个顶点，标准UV映射)
    // 为了让每个面都有正确的纹理映射，每个面必须有独立的顶点数据。
    // 6 个面 * 4 个顶点 = 24 个顶点
    float cubeVertices[] = {
        //   x,     y,    z,       r, g, b,       u, v
        // 1. Front Face (Z = 1)
        -1.0f, -1.0f,  1.0f,    1.0f, 0.0f, 0.0f,   0.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.0f, 0.0f,   1.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.0f,   1.0f, 1.0f,
        -1.0f,  1.0f,  1.0f,    1.0f, 0.0f, 0.0f,   0.0f, 1.0f,

        // 2. Back Face (Z = -1)
         1.0f, -1.0f, -1.0f,    0.0f, 1.0f, 0.0f,   0.0f, 0.0f,
        -1.0f, -1.0f, -1.0f,    0.0f, 1.0f, 0.0f,   1.0f, 0.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 1.0f, 0.0f,   1.0f, 1.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 1.0f, 0.0f,   0.0f, 1.0f,

        // 3. Left Face (X = -1)
        -1.0f, -1.0f, -1.0f,    0.0f, 0.0f, 1.0f,   0.0f, 0.0f,
        -1.0f, -1.0f,  1.0f,    0.0f, 0.0f, 1.0f,   1.0f, 0.0f,
        -1.0f,  1.0f,  1.0f,    0.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 0.0f, 1.0f,   0.0f, 1.0f,

        // 4. Right Face (X = 1)
         1.0f, -1.0f,  1.0f,    1.0f, 1.0f, 0.0f,   0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 1.0f, 0.0f,   1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    1.0f, 1.0f, 0.0f,   1.0f, 1.0f,
         1.0f,  1.0f,  1.0f,    1.0f, 1.0f, 0.0f,   0.0f, 1.0f,

        // 5. Top Face (Y = 1)
        -1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 1.0f,   0.0f, 0.0f,
         1.0f,  1.0f,  1.0f,    0.0f, 1.0f, 1.0f,   1.0f, 0.0f,
         1.0f,  1.0f, -1.0f,    0.0f, 1.0f, 1.0f,   1.0f, 1.0f,
        -1.0f,  1.0f, -1.0f,    0.0f, 1.0f, 1.0f,   0.0f, 1.0f,

        // 6. Bottom Face (Y = -1)
        -1.0f, -1.0f, -1.0f,    1.0f, 0.0f, 1.0f,   0.0f, 0.0f,
         1.0f, -1.0f, -1.0f,    1.0f, 0.0f, 1.0f,   1.0f, 0.0f,
         1.0f, -1.0f,  1.0f,    1.0f, 0.0f, 1.0f,   1.0f, 1.0f,
        -1.0f, -1.0f,  1.0f,    1.0f, 0.0f, 1.0f,   0.0f, 1.0f
    };

    // 索引也要更新，适配 24 个顶点
    // 规律：每个面 4 个顶点，画 2 个三角形
    uint32_t cubeIndices[36];
    for(int i=0; i<6; i++) {
        uint32_t base = i * 4;
        // Tri 1
        cubeIndices[i*6+0] = base + 0;
        cubeIndices[i*6+1] = base + 1;
        cubeIndices[i*6+2] = base + 2;
        // Tri 2
        cubeIndices[i*6+3] = base + 2;
        cubeIndices[i*6+4] = base + 3;
        cubeIndices[i*6+5] = base + 0;
    }


    // Upload Data
    GLuint vao, vbo, ebo;
    ctx.glGenVertexArrays(1, &vao); ctx.glBindVertexArray(vao);
    ctx.glGenBuffers(1, &vbo); ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
    ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);
    ctx.glGenBuffers(1, &ebo); ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // Attributes
    constexpr int STRIDE = 8 * sizeof(float);
    ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, STRIDE, (void*)0);                   ctx.glEnableVertexAttribArray(0);
    ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, STRIDE, (void*)(3*sizeof(float)));   ctx.glEnableVertexAttribArray(1);
    ctx.glVertexAttribPointer(2, 2, GL_FLOAT, false, STRIDE, (void*)(6*sizeof(float)));   ctx.glEnableVertexAttribArray(2);

    // 4. 生成位置和大小
    float rX = 0;
    float rY = 0;
    float rZ = -30;
    float rS = 1;
    
    LOG_INFO("Generated Cube at (" + std::to_string(rX) + ", " + std::to_string(rY) + ", " + std::to_string(rZ) + ") Scale: " + std::to_string(rS));

    // 5. 计算矩阵 (MVP = Projection * View * Model)
    // Model: 先缩放，再平移
    Mat4 model = Mat4::Translate(rX, rY, rZ) * Mat4::Scale(rS, rS, rS);
    // View: 简单的 Identity (相机在原点，看向 -Z)
    Mat4 view = Mat4::Identity();
    float aspect = (float)windowWidth / (float)windowHeight;
    Mat4 proj = Mat4::Perspective(90.0f, aspect, 0.1f, 100.0f); // 60 deg FOV
    // MVP = P * V * M (注意乘法顺序，通常是反向应用)
    Mat4 mvp = proj * view * model;
    // 6. Draw
    ctx.glUseProgram(progID);
    ctx.glUniform1i(uTex, 0);
    ctx.glUniformMatrix4fv(uMVP, 1, false, mvp.m);

    ctx.glDrawElements(GL_TRIANGLES, 36, 0, (void*)0); // 36 indices
    
    ctx.savePPM("result_random_cube.ppm");
}