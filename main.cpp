/**
 * Professional Software Rasterizer Simulation
 * -------------------------------------------
 * Features:
 * - Handle-based Resource Management (VBO/VAO/EBO/Texture/Program)
 * - Safe Memory Access (No unaligned pointer casts)
 * - Full bounds checking
 * - Perspective Correct Interpolation
 * - Verbose Logging for Debugging
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
#include <cassert>
#include <iomanip>

// ==========================================
// 1. 日志与调试系统 (Logging System)
// ==========================================
// 在生产环境中，我们需要知道程序执行到了哪一步
enum LogLevel { INFO, WARN, ERRR };

class Logger {
public:
    static void log(LogLevel level, const std::string& msg) {
        switch (level) {
            case INFO: std::cout << "[INFO] "; break;
            case WARN: std::cout << "[WARN] "; break;
            case ERRR: std::cerr << "[ERROR] "; break;
        }
        std::cout << msg << std::endl;
    }
};

#define LOG_INFO(msg) Logger::log(INFO, msg)
#define LOG_WARN(msg) Logger::log(WARN, msg)
#define LOG_ERROR(msg) Logger::log(ERRR, msg)

// ==========================================
// 2. 数学库 (Math Library)
// ==========================================
struct Vec2 { float x, y; };
struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    
    Vec4 operator+(const Vec4& v) const { return {x+v.x, y+v.y, z+v.z, w+v.w}; }
    Vec4 operator-(const Vec4& v) const { return {x-v.x, y-v.y, z-v.z, w-v.w}; }
    Vec4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
};

struct Mat4 {
    float m[16]; // Column-major
    static Mat4 Identity() {
        Mat4 res = {0};
        res.m[0] = res.m[5] = res.m[10] = res.m[15] = 1.0f;
        return res;
    }
    // Matrix-Vector Multiplication
    Vec4 operator*(const Vec4& v) const {
        return Vec4(
            m[0]*v.x + m[4]*v.y + m[8]*v.z + m[12]*v.w,
            m[1]*v.x + m[5]*v.y + m[9]*v.z + m[13]*v.w,
            m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14]*v.w,
            m[3]*v.x + m[7]*v.y + m[11]*v.z + m[15]*v.w
        );
    }
};

// ==========================================
// 3. OpenGL 常量定义
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

// ==========================================
// 4. 资源数据结构 (Resource Structures)
// ==========================================

// 4.1 通用二进制 Buffer (VBO/EBO)
struct BufferObject {
    std::vector<uint8_t> data;
    
    // 安全读取函数：解决内存对齐和越界问题
    // T: 读取的目标类型
    // offset: 字节偏移量
    template <typename T>
    bool readSafe(size_t offset, T& outValue) const {
        if (offset + sizeof(T) > data.size()) {
            LOG_ERROR("Buffer Read Overflow! Offset: " + std::to_string(offset) + ", Size: " + std::to_string(data.size()));
            return false;
        }
        // 使用 memcpy 防止对齐崩溃 (Bus Error / Segfault)
        std::memcpy(&outValue, data.data() + offset, sizeof(T));
        return true;
    }
};

// 4.2 纹理对象
struct TextureObject {
    GLuint id;
    GLsizei width = 0, height = 0;
    std::vector<uint32_t> pixels; // Packed RGBA (0xAABBGGRR usually, depends on endian)

    Vec4 sample(float u, float v) const {
        if (pixels.empty()) return {1, 0, 1, 1}; // Magenta Error Color
        
        // Wrap: Repeat
        u -= std::floor(u);
        v -= std::floor(v);

        int x = static_cast<int>(u * width);
        int y = static_cast<int>(v * height);
        
        // Clamp to edge to be super safe
        x = std::clamp(x, 0, width - 1);
        y = std::clamp(y, 0, height - 1);

        uint32_t p = pixels[y * width + x];
        
        // Unpack: Assuming Little Endian (R is LSB)
        float r = (p & 0xFF) / 255.0f;
        float g = ((p >> 8) & 0xFF) / 255.0f;
        float b = ((p >> 16) & 0xFF) / 255.0f;
        float a = ((p >> 24) & 0xFF) / 255.0f;
        return Vec4(r, g, b, a);
    }
};

// 4.3 顶点属性描述
struct VertexAttribState {
    bool enabled = false;
    GLuint bindingBufferID = 0; // 该属性关联的 VBO ID
    GLint size = 3;             // 1, 2, 3, 4
    GLenum type = GL_FLOAT;
    GLsizei stride = 0;
    size_t pointerOffset = 0;   // Buffer 内的起始偏移
};

// 4.4 VAO
struct VertexArrayObject {
    VertexAttribState attributes[16]; // Max 16 attributes
    GLuint elementBufferID = 0;       // EBO 绑定状态
};

// 4.5 Shader 上下文 (Varyings)
struct ShaderContext {
    Vec4 varyings[8]; // 用于 VS 到 FS 的插值 (例如 UV, Normal)
};

// 4.6 Shader Program
struct UniformValue {
    enum Type { INT, FLOAT, MAT4 } type;
    union { int i; float f; float mat[16]; } data;
};

struct ProgramObject {
    GLuint id;
    std::unordered_map<std::string, GLint> uniformLocs;
    std::unordered_map<GLint, UniformValue> uniforms;
    
    // Shader Function Signatures
    std::function<Vec4(const std::vector<Vec4>& inAttribs, ShaderContext& outCtx)> vertexShader;
    std::function<Vec4(const ShaderContext& inCtx)> fragmentShader;
};

// ==========================================
// 5. 软渲染核心上下文 (The Context)
// ==========================================
class SoftRenderContext {
private:
    // Resource Maps
    std::unordered_map<GLuint, BufferObject> buffers;
    std::unordered_map<GLuint, VertexArrayObject> vaos;
    std::unordered_map<GLuint, TextureObject> textures;
    std::unordered_map<GLuint, ProgramObject> programs;

    // State Machine
    GLuint m_boundArrayBuffer = 0;      // Global binding point for GL_ARRAY_BUFFER
    GLuint m_boundVertexArray = 0;      // Current VAO
    GLuint m_currentProgram = 0;        // Current Program
    GLuint m_activeTextureUnit = 0;
    GLuint m_boundTextures[32] = {0};   // Texture Units

    // ID Generator
    GLuint m_nextID = 1;

    // Framebuffer
    GLsizei fbWidth = 800;
    GLsizei fbHeight = 600;
    std::vector<uint32_t> colorBuffer;
    std::vector<float> depthBuffer;

public:
    SoftRenderContext() {
        // Init default VAO
        vaos[0] = VertexArrayObject{};
        // Init Framebuffer
        colorBuffer.resize(fbWidth * fbHeight, 0xFF000000); // Black
        depthBuffer.resize(fbWidth * fbHeight, 1.0f);       // Far plane
        LOG_INFO("Context Initialized. Resolution: 800x600");
    }

    // --- Helpers ---
    VertexArrayObject& getVAO() { return vaos[m_boundVertexArray]; }
    
    ProgramObject* getProgram() {
        if (programs.find(m_currentProgram) == programs.end()) return nullptr;
        return &programs[m_currentProgram];
    }
    
    TextureObject* getTexture(GLuint unit) {
        if (unit >= 32) return nullptr;
        GLuint id = m_boundTextures[unit];
        if (id == 0 || textures.find(id) == textures.end()) return nullptr;
        return &textures[id];
    }

    // --- Buffer Management ---
    void glGenBuffers(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) {
            res[i] = m_nextID++;
            buffers[res[i]]; // create empty entry
            LOG_INFO("Created Buffer ID: " + std::to_string(res[i]));
        }
    }

    void glBindBuffer(GLenum target, GLuint buffer) {
        if (target == GL_ARRAY_BUFFER) {
            m_boundArrayBuffer = buffer;
        } else if (target == GL_ELEMENT_ARRAY_BUFFER) {
            // EBO binding is stored inside the VAO
            getVAO().elementBufferID = buffer;
        }
    }

    void glBufferData(GLenum target, GLsizei size, const void* data, GLenum usage) {
        GLuint id = 0;
        if (target == GL_ARRAY_BUFFER) id = m_boundArrayBuffer;
        else if (target == GL_ELEMENT_ARRAY_BUFFER) id = getVAO().elementBufferID;

        if (id == 0) {
            LOG_WARN("glBufferData: No buffer bound to target!");
            return;
        }

        BufferObject& buf = buffers[id];
        buf.data.resize(size);
        if (data) {
            std::memcpy(buf.data.data(), data, size);
        }
        LOG_INFO("BufferData uploaded to ID " + std::to_string(id) + ", Size: " + std::to_string(size));
    }

    // --- VAO Management ---
    void glGenVertexArrays(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) {
            res[i] = m_nextID++;
            vaos[res[i]];
            LOG_INFO("Created VAO ID: " + std::to_string(res[i]));
        }
    }

    void glBindVertexArray(GLuint array) {
        m_boundVertexArray = array;
    }

    // --- Attribute Configuration (Critical for Segfault Prevention) ---
    void glVertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
        VertexArrayObject& vao = getVAO();
        if (index >= 16) return;
        
        if (m_boundArrayBuffer == 0) {
            LOG_ERROR("glVertexAttribPointer: No VBO bound! This will lead to errors.");
            return;
        }

        VertexAttribState& attr = vao.attributes[index];
        attr.bindingBufferID = m_boundArrayBuffer; // Capture Current VBO
        attr.size = size;
        attr.type = type;
        attr.stride = stride;
        // In VBO mode, pointer is an integer offset
        attr.pointerOffset = reinterpret_cast<size_t>(pointer);
        
        LOG_INFO("AttribPtr Index " + std::to_string(index) + " bound to Buffer " + std::to_string(m_boundArrayBuffer));
    }

    void glEnableVertexAttribArray(GLuint index) {
        if (index < 16) getVAO().attributes[index].enabled = true;
    }

    // --- Texture Management ---
    void glGenTextures(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) {
            res[i] = m_nextID++;
            textures[res[i]].id = res[i];
        }
    }

    void glActiveTexture(GLenum texture) {
        if (texture >= 0x84C0 && texture < 0x84C0 + 32)
            m_activeTextureUnit = texture - 0x84C0;
    }

    void glBindTexture(GLenum target, GLuint texture) {
        if (target == GL_TEXTURE_2D)
            m_boundTextures[m_activeTextureUnit] = texture;
    }

    void glTexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* pixels) {
        TextureObject* tex = getTexture(m_activeTextureUnit);
        if (!tex) return;
        tex->width = w; tex->height = h;
        tex->pixels.resize(w * h);
        if (pixels) std::memcpy(tex->pixels.data(), pixels, w * h * 4);
        LOG_INFO("Texture loaded: " + std::to_string(w) + "x" + std::to_string(h));
    }

    // --- Program / Uniforms ---
    GLuint glCreateProgram() {
        GLuint id = m_nextID++;
        programs[id].id = id;
        return id;
    }

    void glUseProgram(GLuint prog) { m_currentProgram = prog; }

    GLint glGetUniformLocation(GLuint prog, const char* name) {
        GLint loc = (GLint)std::hash<std::string>{}(name);
        programs[prog].uniformLocs[name] = loc;
        return loc;
    }

    void glUniform1i(GLint loc, int val) {
        if (ProgramObject* p = getProgram()) {
            p->uniforms[loc].type = UniformValue::INT;
            p->uniforms[loc].data.i = val;
        }
    }

    void glUniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat* value) {
        if (ProgramObject* p = getProgram()) {
            p->uniforms[loc].type = UniformValue::MAT4;
            std::memcpy(p->uniforms[loc].data.mat, value, 16*sizeof(float));
        }
    }

    // ==========================================
    // 6. 顶点抓取逻辑 (The Fetcher)
    // ==========================================
    // 从 VBO 中安全读取数据并转换为 Vec4
    Vec4 fetchAttribute(const VertexAttribState& attr, int vertexIndex) {
        if (!attr.enabled) return Vec4(0,0,0,1);

        // 1. Find Buffer
        auto it = buffers.find(attr.bindingBufferID);
        if (it == buffers.end()) {
            LOG_ERROR("FetchAttribute: Buffer ID " + std::to_string(attr.bindingBufferID) + " not found!");
            return Vec4(0,0,0,1);
        }
        const BufferObject& buf = it->second;

        // 2. Calculate Address
        // default stride = size * sizeof(type)
        size_t typeSize = sizeof(float); // Simplified: Assume GL_FLOAT
        size_t stride = attr.stride ? attr.stride : (attr.size * typeSize);
        size_t offset = attr.pointerOffset + (vertexIndex * stride);

        // 3. Safe Read
        float raw[4] = {0, 0, 0, 1}; // Default
        for (int i = 0; i < attr.size; ++i) {
            if (!buf.readSafe<float>(offset + i * typeSize, raw[i])) {
                // Log already handled in readSafe
                return Vec4(0,0,0,1);
            }
        }

        return Vec4(raw[0], raw[1], raw[2], raw[3]);
    }

    // ==========================================
    // 7. 光栅化逻辑 (The Rasterizer)
    // ==========================================
    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
        LOG_INFO("Starting DrawElements. Count: " + std::to_string(count));

        if (mode != GL_TRIANGLES) {
            LOG_WARN("Only GL_TRIANGLES supported.");
            return;
        }

        ProgramObject* prog = getProgram();
        if (!prog) {
            LOG_ERROR("Draw failed: No Program Active.");
            return;
        }

        // 1. Get EBO
        VertexArrayObject& vao = getVAO();
        if (vao.elementBufferID == 0) {
            LOG_ERROR("Draw failed: No EBO bound in VAO.");
            return;
        }
        BufferObject& ebo = buffers[vao.elementBufferID];

        // 2. Read Indices
        // indices param is the OFFSET into the EBO
        size_t indicesOffset = reinterpret_cast<size_t>(indices);
        std::vector<uint32_t> indexCache(count);
        
        for (int i = 0; i < count; ++i) {
            // Assume indices are GL_UNSIGNED_INT for simplicity
            if (!ebo.readSafe<uint32_t>(indicesOffset + i * sizeof(uint32_t), indexCache[i])) {
                LOG_ERROR("Failed to read Index " + std::to_string(i));
                return;
            }
        }

        // 3. Vertex Processing (Vertex Shader)
        // Find max index to know how many vertices to process
        uint32_t maxIndex = 0;
        for (auto idx : indexCache) if (idx > maxIndex) maxIndex = idx;

        struct VSOutput {
            Vec4 pos;       // Clip Space
            Vec4 screenPos; // Screen Space + 1/w
            ShaderContext ctx;
        };
        std::vector<VSOutput> processedVerts(maxIndex + 1);

        // Run VS for each referenced vertex
        // (Optimization: In real GPU, this is cached)
        for (uint32_t i = 0; i <= maxIndex; ++i) {
            // Prepare Inputs
            std::vector<Vec4> inputAttribs(16);
            for (int loc = 0; loc < 16; ++loc) {
                if (vao.attributes[loc].enabled) {
                    inputAttribs[loc] = fetchAttribute(vao.attributes[loc], i);
                }
            }

            // Execute Shader
            ShaderContext ctx;
            Vec4 clipPos = prog->vertexShader(inputAttribs, ctx);

            // Perspective Division
            float w = clipPos.w;
            if (std::abs(w) < 1e-6) w = 1.0f; // Prevent div by zero
            float rhw = 1.0f / w; // Reciprocal Homogeneous W

            // Viewport Transform -> Screen Space
            // x: [0, width], y: [0, height]
            float ndcX = clipPos.x * rhw;
            float ndcY = clipPos.y * rhw;
            
            processedVerts[i].pos = clipPos;
            processedVerts[i].ctx = ctx;
            processedVerts[i].screenPos = Vec4(
                (ndcX + 1.0f) * 0.5f * fbWidth,
                (ndcY + 1.0f) * 0.5f * fbHeight,
                clipPos.z * rhw, // ndcZ
                rhw              // store 1/w for perspective correction
            );
        }

        // 4. Rasterization (Triangle Traversal)
        for (int i = 0; i < count; i += 3) {
            const auto& v0 = processedVerts[indexCache[i]];
            const auto& v1 = processedVerts[indexCache[i+1]];
            const auto& v2 = processedVerts[indexCache[i+2]];

            // Bounding Box
            int minX = std::max(0, (int)std::min({v0.screenPos.x, v1.screenPos.x, v2.screenPos.x}));
            int minY = std::max(0, (int)std::min({v0.screenPos.y, v1.screenPos.y, v2.screenPos.y}));
            int maxX = std::min(fbWidth-1, (int)std::max({v0.screenPos.x, v1.screenPos.x, v2.screenPos.x}) + 1);
            int maxY = std::min(fbHeight-1, (int)std::max({v0.screenPos.y, v1.screenPos.y, v2.screenPos.y}) + 1);

            // Edge function for barycentric coords
            auto edgeFunc = [](const Vec4& a, const Vec4& b, const Vec4& p) {
                return (p.x - a.x) * (b.y - a.y) - (p.y - a.y) * (b.x - a.x);
            };

            float area = edgeFunc(v0.screenPos, v1.screenPos, v2.screenPos);
            // Backface culling (if area < 0) or degenerate triangle
            if (area <= 0) continue; 

            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    Vec4 pixelCenter((float)x + 0.5f, (float)y + 0.5f, 0, 0);

                    float w0 = edgeFunc(v1.screenPos, v2.screenPos, pixelCenter);
                    float w1 = edgeFunc(v2.screenPos, v0.screenPos, pixelCenter);
                    float w2 = edgeFunc(v0.screenPos, v1.screenPos, pixelCenter);

                    // Inside Triangle Check
                    if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                        w0 /= area; w1 /= area; w2 /= area;

                        // Perspective Correct Depth Interpolation
                        // 1/z = w0*(1/z0) + w1*(1/z1) + w2*(1/z2)
                        // Note: screenPos.w stores 1/w_clip
                        float zInv = w0 * v0.screenPos.w + w1 * v1.screenPos.w + w2 * v2.screenPos.w;
                        float z = 1.0f / zInv;

                        // Depth Test
                        int idx = y * fbWidth + x;
                        if (z < depthBuffer[idx]) {
                            depthBuffer[idx] = z;

                            // Interpolate Attributes (Perspective Correct)
                            ShaderContext fsIn;
                            // For each varying: (attr/w)_interp * z
                            for (int k = 0; k < 8; ++k) {
                                Vec4 attr0 = v0.ctx.varyings[k] * v0.screenPos.w;
                                Vec4 attr1 = v1.ctx.varyings[k] * v1.screenPos.w;
                                Vec4 attr2 = v2.ctx.varyings[k] * v2.screenPos.w;
                                Vec4 interp = attr0 * w0 + attr1 * w1 + attr2 * w2;
                                fsIn.varyings[k] = interp * z;
                            }

                            // Fragment Shader
                            Vec4 color = prog->fragmentShader(fsIn);

                            // Output to Framebuffer
                            uint8_t r = (uint8_t)(std::clamp(color.x, 0.0f, 1.0f) * 255);
                            uint8_t g = (uint8_t)(std::clamp(color.y, 0.0f, 1.0f) * 255);
                            uint8_t b = (uint8_t)(std::clamp(color.z, 0.0f, 1.0f) * 255);
                            
                            colorBuffer[idx] = (255 << 24) | (b << 16) | (g << 8) | r;
                        }
                    }
                }
            }
        }
        LOG_INFO("DrawElements Finished.");
    }

    void savePPM(const char* filename) {
        FILE* f = fopen(filename, "wb");
        if (!f) { LOG_ERROR("Failed to open file for writing."); return; }
        fprintf(f, "P6\n%d %d\n255\n", fbWidth, fbHeight);
        for(int i=0; i<fbWidth*fbHeight; ++i) {
            uint32_t p = colorBuffer[i];
            uint8_t r = p & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = (p >> 16) & 0xFF;
            fwrite(&r, 1, 1, f);
            fwrite(&g, 1, 1, f);
            fwrite(&b, 1, 1, f);
        }
        fclose(f);
        LOG_INFO("Image saved to " + std::string(filename));
    }
};

// ==========================================
// 8. 客户端代码 (User Application)
// ==========================================
int main() {
    SoftRenderContext ctx;

    // 1. Create Shader
    GLuint prog = ctx.glCreateProgram();
    GLint uMVP = ctx.glGetUniformLocation(prog, "uMVP");
    GLint uTex = ctx.glGetUniformLocation(prog, "uTex");

    // Vertex Shader: Pass Pos and UV
    ctx.getProgram()->vertexShader = [uMVP, &ctx](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        Vec4 pos = attribs[0];
        Vec4 uv  = attribs[1];
        outCtx.varyings[0] = uv; // Pass UV to FS

        // Read Uniform
        Mat4 mvp = Mat4::Identity();
        if (auto* p = ctx.getProgram()) {
             // Safe copy
             std::memcpy(mvp.m, p->uniforms[uMVP].data.mat, 16*sizeof(float));
        }
        
        pos.w = 1.0f;
        return mvp * pos;
    };

    // Fragment Shader: Sample Texture
    ctx.getProgram()->fragmentShader = [uTex, &ctx](const ShaderContext& inCtx) -> Vec4 {
        Vec4 uv = inCtx.varyings[0];
        int unit = 0;
        if (auto* p = ctx.getProgram()) unit = p->uniforms[uTex].data.i;
        
        if (auto* tex = ctx.getTexture(unit)) {
            return tex->sample(uv.x, uv.y);
        }
        return Vec4(1, 0, 1, 1); // Error color
    };

    // 2. Texture Data
    GLuint tex;
    ctx.glGenTextures(1, &tex);
    ctx.glActiveTexture(0x84C0); // Texture0
    ctx.glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<uint32_t> pixels(256*256);
    for(int y=0; y<256; y++) {
        for(int x=0; x<256; x++) {
            bool check = ((x/32) + (y/32)) % 2 == 0;
            pixels[y*256+x] = check ? 0xFFFFFFFF : 0xFF0000FF; // White or Red (AABBGGRR)
        }
    }
    ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // 3. Geometry (Quad)
    float vertices[] = {
        -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
         0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
         0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
        -0.5f,  0.5f, 0.0f,   0.0f, 1.0f
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

    // Setup Layout
    // 0: Pos (3 floats)
    ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 5*sizeof(float), (void*)0);
    ctx.glEnableVertexAttribArray(0);
    // 1: UV (2 floats)
    ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, 5*sizeof(float), (void*)(3*sizeof(float)));
    ctx.glEnableVertexAttribArray(1);

    // 4. Draw
    ctx.glUseProgram(prog);
    ctx.glUniform1i(uTex, 0);
    Mat4 mvp = Mat4::Identity();
    // Simple scale
    mvp.m[0] = 1.5f; mvp.m[5] = 1.5f;
    ctx.glUniformMatrix4fv(uMVP, 1, false, mvp.m);

    // Pass offset 0 as (void*)0
    ctx.glDrawElements(GL_TRIANGLES, 6, 0, (void*)0);

    ctx.savePPM("result.ppm");
    return 0;
}