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
    static Mat4 Identity() {
        Mat4 res = {0};
        res.m[0] = res.m[5] = res.m[10] = res.m[15] = 1.0f;
        return res;
    }
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

// Buffer
struct BufferObject {
    std::vector<uint8_t> data;
    
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
    std::vector<uint32_t> pixels; 

    Vec4 sample(float u, float v) const {
        if (pixels.empty()) return {1, 0, 1, 1}; // Magenta
        u -= std::floor(u); v -= std::floor(v);
        int x = std::clamp((int)(u * width), 0, width - 1);
        int y = std::clamp((int)(v * height), 0, height - 1);
        uint32_t p = pixels[y * width + x];
        float r = (p & 0xFF) / 255.0f;
        float g = ((p >> 8) & 0xFF) / 255.0f;
        float b = ((p >> 16) & 0xFF) / 255.0f;
        float a = ((p >> 24) & 0xFF) / 255.0f;
        return Vec4(r, g, b, a);
    }
};

// VAO Components
struct VertexAttribState {
    bool enabled = false;
    GLuint bindingBufferID = 0;
    GLint size = 3;
    GLenum type = GL_FLOAT;
    GLsizei stride = 0;
    size_t pointerOffset = 0;
};

struct VertexArrayObject {
    VertexAttribState attributes[16];
    GLuint elementBufferID = 0;
};

// Shader & Program
struct ShaderContext {
    Vec4 varyings[8];
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
    GLuint m_boundTextures[32] = {0};

    GLuint m_nextID = 1;
    GLsizei fbWidth = 800;
    GLsizei fbHeight = 600;
    
    std::vector<uint32_t> colorBuffer;
    std::vector<float> depthBuffer;

public:
    SoftRenderContext() {
        vaos[0] = VertexArrayObject{}; 
        colorBuffer.resize(fbWidth * fbHeight, 0xFF000000); // 黑色背景
        // 【Fix 1】: Depth 初始化为极大值，确保 z=1.0 的物体能通过测试
        depthBuffer.resize(fbWidth * fbHeight, 1e9f);       
        LOG_INFO("Context Initialized (800x600)");
    }

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
        if (index >= 16) return;
        if (m_boundArrayBuffer == 0) {
            LOG_ERROR("No VBO bound!");
            return;
        }
        VertexAttribState& attr = getVAO().attributes[index];
        attr.bindingBufferID = m_boundArrayBuffer;
        attr.size = size;
        attr.type = type;
        attr.stride = stride;
        attr.pointerOffset = reinterpret_cast<size_t>(pointer);
        LOG_INFO("Attrib " + std::to_string(index) + " bound to VBO " + std::to_string(m_boundArrayBuffer));
    }

    void glEnableVertexAttribArray(GLuint index) {
        if (index < 16) getVAO().attributes[index].enabled = true;
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
        if (texture >= 0x84C0 && texture < 0x84C0 + 32) m_activeTextureUnit = texture - 0x84C0;
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

    void glTexImage2D(GLenum target, GLint level, GLint internal, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* pixels) {
        TextureObject* tex = getTexture(m_activeTextureUnit);
        if (!tex) {
            LOG_ERROR("No texture bound to unit " + std::to_string(m_activeTextureUnit));
            return;
        }
        tex->width = w; tex->height = h;
        tex->pixels.resize(w * h);
        if (pixels) std::memcpy(tex->pixels.data(), pixels, w*h*4);
        LOG_INFO("Loaded Texture: " + std::to_string(w) + "x" + std::to_string(h));
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
    void glUniformMatrix4fv(GLint loc, GLsizei count, GLboolean transpose, const GLfloat* value) {
        if (auto* p = getCurrentProgram()) {
            p->uniforms[loc].type = UniformValue::MAT4;
            std::memcpy(p->uniforms[loc].data.mat, value, 16*sizeof(float));
        }
    }

    // --- Rasterizer ---
    Vec4 fetchAttribute(const VertexAttribState& attr, int idx) {
        if (!attr.enabled) return Vec4(0,0,0,1);
        auto it = buffers.find(attr.bindingBufferID);
        if (it == buffers.end()) return Vec4(0,0,0,1);
        
        size_t stride = attr.stride ? attr.stride : attr.size * sizeof(float);
        size_t offset = attr.pointerOffset + idx * stride;
        
        float raw[4] = {0,0,0,1};
        for(int i=0; i<attr.size; ++i) {
            it->second.readSafe<float>(offset + i*sizeof(float), raw[i]);
        }
        return Vec4(raw[0], raw[1], raw[2], raw[3]);
    }

    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
        LOG_INFO("DrawElements Count: " + std::to_string(count));
    
        ProgramObject* prog = getCurrentProgram();
        if (!prog || !prog->vertexShader || !prog->fragmentShader) {
            LOG_ERROR("Invalid Program State");
            return;
        }

        VertexArrayObject& vao = getVAO();
        if (vao.elementBufferID == 0) { LOG_ERROR("No EBO Bound"); return; }
        
        // 1. Indices
        std::vector<uint32_t> idxCache(count);
        size_t idxOffset = reinterpret_cast<size_t>(indices);
        for(int i=0; i<count; ++i) {
            buffers[vao.elementBufferID].readSafe<uint32_t>(idxOffset + i*4, idxCache[i]);
        }

        // 2. Vertex Shader
        uint32_t maxIdx = 0;
        for(auto i : idxCache) maxIdx = std::max(maxIdx, i);
        
        struct VOut { Vec4 pos; Vec4 scn; ShaderContext ctx; };
        std::vector<VOut> vOuts(maxIdx + 1);

        for(uint32_t i=0; i<=maxIdx; ++i) {
            std::vector<Vec4> ins(16);
            for(int k=0; k<16; ++k) {
                if(vao.attributes[k].enabled) ins[k] = fetchAttribute(vao.attributes[k], i);
            }
            ShaderContext ctx;
            vOuts[i].pos = prog->vertexShader(ins, ctx);
            vOuts[i].ctx = ctx;

            float w = vOuts[i].pos.w;
            // 防止除以0
            if (std::abs(w) < 1e-5) w = 1.0f;
            float rhw = 1.0f / w;
            
            // Viewport Transform
            // 【Fix 1】: Y轴翻转 (Viewport Transform)
            // 原公式: (y + 1) * 0.5 -> 导致 Y=-1(底) 映射到 0(图顶)
            // 新公式: (1 - y) * 0.5 -> 导致 Y=+1(顶) 映射到 0(图顶)，Y=-1(底) 映射到 Height
            vOuts[i].scn = { 
                (vOuts[i].pos.x*rhw+1.0f)*0.5f*fbWidth, 
                (1.0f - vOuts[i].pos.y*rhw)*0.5f*fbHeight, // 翻转 Y
                vOuts[i].pos.z*rhw, 
                rhw 
            };
        }

        // 3. Rasterization
        for(int i=0; i<count; i+=3) {
            auto &v0 = vOuts[idxCache[i]], &v1 = vOuts[idxCache[i+1]], &v2 = vOuts[idxCache[i+2]];
            
            int minX = std::max(0, (int)std::min({v0.scn.x, v1.scn.x, v2.scn.x}));
            int minY = std::max(0, (int)std::min({v0.scn.y, v1.scn.y, v2.scn.y}));
            int maxX = std::min(fbWidth-1, (int)std::max({v0.scn.x, v1.scn.x, v2.scn.x}) + 1);
            int maxY = std::min(fbHeight-1, (int)std::max({v0.scn.y, v1.scn.y, v2.scn.y}) + 1);

            // 【Fix 2】: Edge Function 修正
            // 由于 Y 轴翻转了，叉乘的方向也变了。
            // 为了保持 CCW 三角形为正面积 (Area > 0)，我们需要交换相减的顺序或者对整体取反。
            // 旧公式: (b.x - a.x)*(p.y - a.y) - (b.y - a.y)*(p.x - a.x)
            // 新公式: 下面交换了减数和被减数，适配 Y-Down 坐标系
            auto edge = [](const Vec4& a, const Vec4& b, const Vec4& p) {
                return (b.y - a.y) * (p.x - a.x) - (b.x - a.x) * (p.y - a.y);
            };

            float area = edge(v0.scn, v1.scn, v2.scn);
            
            // 如果想看背面，可以注释掉这行。但在标准 OpenGL 中 CCW 为正。
            if (area <= 0) continue; 

            for(int y=minY; y<=maxY; ++y) {
                for(int x=minX; x<=maxX; ++x) {
                    Vec4 p((float)x+0.5f, (float)y+0.5f, 0, 0);
                    
                    // 计算重心坐标权重
                    float w0 = edge(v1.scn, v2.scn, p);
                    float w1 = edge(v2.scn, v0.scn, p);
                    float w2 = edge(v0.scn, v1.scn, p);
                    
                    // 如果点在三角形内 (所有权重符号一致)
                    if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                        w0 /= area; w1 /= area; w2 /= area;
                        
                        // 透视矫正插值深度 1/z
                        float zInv = w0*v0.scn.w + w1*v1.scn.w + w2*v2.scn.w;
                        float z = 1.0f/zInv;
                        
                        int pix = y*fbWidth+x;
                        // 【Fix 3】: 深度测试改为 < (因为depth初始值极大)
                        if (z < depthBuffer[pix]) {
                            depthBuffer[pix] = z;
                            
                            ShaderContext fsIn;
                            for(int k=0; k<8; ++k) {
                                // 透视矫正属性插值
                                Vec4 a0 = v0.ctx.varyings[k]*v0.scn.w;
                                Vec4 a1 = v1.ctx.varyings[k]*v1.scn.w;
                                Vec4 a2 = v2.ctx.varyings[k]*v2.scn.w;
                                fsIn.varyings[k] = (a0*w0 + a1*w1 + a2*w2) * z;
                            }
                            
                            Vec4 c = prog->fragmentShader(fsIn);
                            uint8_t R = (uint8_t)(std::clamp(c.x,0.0f,1.0f)*255);
                            uint8_t G = (uint8_t)(std::clamp(c.y,0.0f,1.0f)*255);
                            uint8_t B = (uint8_t)(std::clamp(c.z,0.0f,1.0f)*255);
                            // AABBGGRR 格式
                            colorBuffer[pix] = (255<<24) | (B<<16) | (G<<8) | R;
                        }
                    }
                }
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

int main() {
    SoftRenderContext ctx;

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
    ctx.glActiveTexture(0x84C0);
    ctx.glBindTexture(GL_TEXTURE_2D, tex);
    std::vector<uint32_t> pixels(256*256);
    for(int y=0; y<256; ++y) for(int x=0; x<256; ++x) {
        pixels[y*256+x] = (((x/32)+(y/32))%2) ? 0xFFFFFFFF : 0xFF0000FF;
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
    return 0;
}