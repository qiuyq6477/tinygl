#include <vector>
#include <unordered_map>
#include <iostream>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <functional>
#include <algorithm>
#include <string>

// ==========================================
// 1. 基础数学库 (Minimal Math Library)
// ==========================================
struct Vec2 { float x, y; };
struct Vec3 { float x, y, z; };
struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(1) {}
    Vec4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
    
    Vec4 operator+(const Vec4& v) const { return {x+v.x, y+v.y, z+v.z, w+v.w}; }
    Vec4 operator-(const Vec4& v) const { return {x-v.x, y-v.y, z-v.z, w-v.w}; }
    Vec4 operator*(float s) const { return {x*s, y*s, z*s, w*s}; }
};

struct Mat4 {
    float m[4][4];
    static Mat4 Identity() {
        Mat4 res = {0};
        for(int i=0; i<4; i++) res.m[i][i] = 1.0f;
        return res;
    }
    Vec4 operator*(const Vec4& v) const {
        return Vec4(
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z + m[0][3]*v.w,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z + m[1][3]*v.w,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z + m[2][3]*v.w,
            m[3][0]*v.x + m[3][1]*v.y + m[3][2]*v.z + m[3][3]*v.w
        );
    }
};

// ==========================================
// 2. OpenGL 常量与类型
// ==========================================
using GLenum = uint32_t;
using GLuint = uint32_t;
using GLint = int32_t;
using GLsizei = int32_t;
using GLboolean = bool;
using GLfloat = float;
using GLubyte = uint8_t;

const GLenum GL_ARRAY_BUFFER = 0x8892;
const GLenum GL_ELEMENT_ARRAY_BUFFER = 0x8893;
const GLenum GL_STATIC_DRAW = 0x88E4;
const GLenum GL_FLOAT = 0x1406;
const GLenum GL_TRIANGLES = 0x0004;
const GLenum GL_TEXTURE_2D = 0x0DE1;
const GLenum GL_RGBA = 0x1908;
const GLenum GL_UNSIGNED_BYTE = 0x1401;

// ==========================================
// 3. 资源对象结构
// ==========================================

// Buffer (VBO/EBO)
struct BufferObject {
    std::vector<uint8_t> data;
};

// Texture
struct TextureObject {
    GLuint id;
    GLsizei width = 0, height = 0;
    std::vector<uint32_t> pixels; // RGBA 8888 packed
    
    // 简单的最近邻采样
    Vec4 sample(float u, float v) const {
        if (pixels.empty()) return {1, 0, 1, 1}; // Magenta error color
        
        // Wrap mode: Repeat
        u = u - floor(u);
        v = v - floor(v);

        int x = static_cast<int>(u * width);
        int y = static_cast<int>(v * height);
        x = std::clamp(x, 0, width - 1);
        y = std::clamp(y, 0, height - 1);

        uint32_t pixel = pixels[y * width + x];
        // Decode RGBA -> Vec4
        float r = ((pixel >> 0) & 0xFF) / 255.0f;
        float g = ((pixel >> 8) & 0xFF) / 255.0f;
        float b = ((pixel >> 16) & 0xFF) / 255.0f;
        float a = ((pixel >> 24) & 0xFF) / 255.0f;
        return Vec4(r, g, b, a);
    }
};

// Vertex Attribute State
struct VertexAttribState {
    bool enabled = false;
    GLuint bindingBufferID = 0;
    GLint size = 4;
    GLenum type = GL_FLOAT;
    GLsizei stride = 0;
    size_t pointerOffset = 0;
};

// VAO
struct VertexArrayObject {
    VertexAttribState attributes[16];
    GLuint elementBufferID = 0;
};

// Varyings (VS -> FS 的插值数据)
// 简单的只支持最多 4 个 vec4 变量
struct ShaderContext {
    Vec4 varyings[4]; 
};

// Uniform 存储器
struct UniformValue {
    enum Type { FLOAT, INT, MAT4, VEC4 } type;
    union {
        float f;
        int i;
        float mat[16];
        float vec[4];
    } data;
};

// Program (Shader)
struct ProgramObject {
    GLuint id;
    std::unordered_map<std::string, GLint> uniformLocations;
    std::unordered_map<GLint, UniformValue> uniforms;

    // 模拟 Shader 代码：输入 Attribute/Context，输出 Position/Context
    std::function<Vec4(const std::vector<Vec4>& attribs, ShaderContext& ctx)> vertexShader;
    
    // 模拟 Shader 代码：输入 Context，输出 Color
    std::function<Vec4(const ShaderContext& ctx)> fragmentShader;
};

// ==========================================
// 4. 软渲染上下文 (SoftRenderContext)
// ==========================================
class SoftRenderContext {
private:
    // 资源池
    std::unordered_map<GLuint, BufferObject> buffers;
    std::unordered_map<GLuint, VertexArrayObject> vaos;
    std::unordered_map<GLuint, TextureObject> textures;
    std::unordered_map<GLuint, ProgramObject> programs;

    // 状态机
    GLuint boundVBO = 0;
    GLuint boundVAO = 0;
    GLuint currentProgram = 0;
    GLuint activeTextureUnit = 0; // 0 ~ 31
    GLuint boundTextures[32] = {0}; // 每个 Unit 绑定的纹理 ID

    // ID 计数器
    GLuint nextID = 1;

    // Framebuffer (简单起见，固定大小)
    GLsizei fbWidth = 800;
    GLsizei fbHeight = 600;
    std::vector<uint32_t> colorBuffer;
    std::vector<float> depthBuffer;

public:
    SoftRenderContext() {
        vaos[0] = VertexArrayObject{};
        colorBuffer.resize(fbWidth * fbHeight, 0xFF000000); // Clear Black
        depthBuffer.resize(fbWidth * fbHeight, 1.0f);       // Clear Depth
    }

    // --- Helpers ---
    VertexArrayObject& getVAO() { return vaos[boundVAO != 0 ? boundVAO : 0]; }
    ProgramObject* getProgram() { return currentProgram ? &programs[currentProgram] : nullptr; }
    
    // 获取指定 Texture Unit 绑定的 Texture 对象
    TextureObject* getTexture(GLuint unit) {
        if (unit >= 32) return nullptr;
        GLuint texID = boundTextures[unit];
        if (texID == 0) return nullptr;
        return &textures[texID];
    }

    // --- Buffer API ---
    void glGenBuffers(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) { res[i] = nextID++; buffers[res[i]]; }
    }
    void glBindBuffer(GLenum target, GLuint id) {
        if (target == GL_ARRAY_BUFFER) boundVBO = id;
        else if (target == GL_ELEMENT_ARRAY_BUFFER) getVAO().elementBufferID = id;
    }
    void glBufferData(GLenum target, GLsizei size, const void* data, GLenum usage) {
        GLuint id = (target == GL_ARRAY_BUFFER) ? boundVBO : getVAO().elementBufferID;
        if (buffers.count(id)) {
            buffers[id].data.assign((uint8_t*)data, (uint8_t*)data + size);
        }
    }

    // --- VAO API ---
    void glGenVertexArrays(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) { res[i] = nextID++; vaos[res[i]]; }
    }
    void glBindVertexArray(GLuint id) { boundVAO = id; }
    void glEnableVertexAttribArray(GLuint idx) { getVAO().attributes[idx].enabled = true; }
    void glVertexAttribPointer(GLuint idx, GLint size, GLenum type, GLboolean norm, GLsizei stride, const void* ptr) {
        auto& attr = getVAO().attributes[idx];
        attr.bindingBufferID = boundVBO; // Capture current VBO
        attr.size = size;
        attr.type = type;
        attr.stride = stride;
        attr.pointerOffset = (size_t)ptr;
    }

    // --- Texture API ---
    void glGenTextures(GLsizei n, GLuint* res) {
        for(int i=0; i<n; i++) { res[i] = nextID++; textures[res[i]].id = res[i]; }
    }
    void glActiveTexture(GLenum texture) {
        // GL_TEXTURE0 = 0x84C0
        if (texture >= 0x84C0 && texture < 0x84C0 + 32) {
            activeTextureUnit = texture - 0x84C0;
        }
    }
    void glBindTexture(GLenum target, GLuint texture) {
        if (target == GL_TEXTURE_2D) {
            boundTextures[activeTextureUnit] = texture;
        }
    }
    void glTexImage2D(GLenum target, GLint level, GLint internalformat, 
                     GLsizei width, GLsizei height, GLint border, 
                     GLenum format, GLenum type, const void* pixels) {
        TextureObject* tex = getTexture(activeTextureUnit);
        if (!tex) return;
        tex->width = width;
        tex->height = height;
        tex->pixels.resize(width * height);
        // 简单拷贝，假设输入格式总是 RGBA 8bit
        if (pixels) memcpy(tex->pixels.data(), pixels, width * height * 4);
    }

    // --- Program & Uniforms API ---
    GLuint glCreateProgram() {
        GLuint id = nextID++;
        programs[id].id = id;
        return id;
    }
    void glUseProgram(GLuint program) { currentProgram = program; }
    
    GLint glGetUniformLocation(GLuint program, const char* name) {
        // 简单模拟：哈希字符串得到一个伪 ID
        GLint loc = std::hash<std::string>{}(name) & 0x7FFFFFFF;
        programs[program].uniformLocations[name] = loc;
        return loc;
    }
    
    void glUniform1i(GLint location, GLint v0) {
        ProgramObject* prog = getProgram();
        if(prog) {
            prog->uniforms[location].type = UniformValue::INT;
            prog->uniforms[location].data.i = v0;
        }
    }
    void glUniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) {
        ProgramObject* prog = getProgram();
        if(prog) {
            prog->uniforms[location].type = UniformValue::MAT4;
            memcpy(prog->uniforms[location].data.mat, value, 16 * sizeof(float));
        }
    }

    // --- 内部：Attribute Fetch ---
    Vec4 fetchAttribute(const VertexAttribState& attr, int index) {
        if (!attr.enabled) return Vec4(0,0,0,1);
        auto& buf = buffers[attr.bindingBufferID];
        size_t stride = attr.stride ? attr.stride : attr.size * sizeof(float);
        size_t offset = attr.pointerOffset + index * stride;
        
        const float* raw = (float*)(buf.data.data() + offset);
        return Vec4(
            raw[0],
            attr.size > 1 ? raw[1] : 0,
            attr.size > 2 ? raw[2] : 0,
            attr.size > 3 ? raw[3] : 1
        );
    }

    // ==========================================
    // 5. 核心：Rasterizer (光栅化)
    // ==========================================
    void glDrawElements(GLenum mode, GLsizei count, GLenum type, const void* indices) {
        if (mode != GL_TRIANGLES) return; // 仅演示三角形
        
        ProgramObject* prog = getProgram();
        if (!prog) return;

        VertexArrayObject& vao = getVAO();
        auto& ebo = buffers[vao.elementBufferID];
        const uint32_t* idxPtr = (uint32_t*)(ebo.data.data() + (size_t)indices);

        // 1. Vertex Processing (VS)
        struct TransformedVertex {
            Vec4 pos;       // Clip Space Position
            Vec4 screenPos; // Screen Space (x, y, z, 1/w)
            ShaderContext ctx;
        };
        std::vector<TransformedVertex> transformedVerts;
        
        // 这一步在硬件中通常是流式的，这里为了简单先全部处理完
        int maxIndex = 0;
        for(int i=0; i<count; i++) maxIndex = std::max(maxIndex, (int)idxPtr[i]);
        
        transformedVerts.resize(maxIndex + 1);
        
        // 执行 Vertex Shader
        for (int i = 0; i <= maxIndex; ++i) {
            std::vector<Vec4> inputs(16);
            for(int loc=0; loc<16; loc++) {
                 if(vao.attributes[loc].enabled) inputs[loc] = fetchAttribute(vao.attributes[loc], i);
            }
            
            ShaderContext ctx;
            // Call VS
            transformedVerts[i].pos = prog->vertexShader(inputs, ctx);
            transformedVerts[i].ctx = ctx;

            // Perspective Division & Viewport Transform
            Vec4 clip = transformedVerts[i].pos;
            float w = clip.w;
            if (w == 0.0f) w = 1.0f;
            
            // NDC -> Screen
            // NDC: [-1, 1], Screen: [0, W], [0, H]
            float ndcX = clip.x / w;
            float ndcY = clip.y / w;
            float ndcZ = clip.z / w;

            transformedVerts[i].screenPos.x = (ndcX + 1.0f) * 0.5f * fbWidth;
            transformedVerts[i].screenPos.y = (ndcY + 1.0f) * 0.5f * fbHeight;
            transformedVerts[i].screenPos.z = ndcZ;
            transformedVerts[i].screenPos.w = 1.0f / w; // Store 1/w for perspective correction
        }

        // 2. Rasterization (Triangle Traversal)
        for (int i = 0; i < count; i += 3) {
            const auto& v0 = transformedVerts[idxPtr[i]];
            const auto& v1 = transformedVerts[idxPtr[i+1]];
            const auto& v2 = transformedVerts[idxPtr[i+2]];

            // Bounding Box
            int minX = std::max(0, (int)std::min({v0.screenPos.x, v1.screenPos.x, v2.screenPos.x}));
            int minY = std::max(0, (int)std::min({v0.screenPos.y, v1.screenPos.y, v2.screenPos.y}));
            int maxX = std::min(fbWidth-1, (int)std::max({v0.screenPos.x, v1.screenPos.x, v2.screenPos.x}) + 1);
            int maxY = std::min(fbHeight-1, (int)std::max({v0.screenPos.y, v1.screenPos.y, v2.screenPos.y}) + 1);

            // Edge function for barycentric
            auto edgeFunction = [](const Vec4& a, const Vec4& b, const Vec4& c) {
                return (c.x - a.x) * (b.y - a.y) - (c.y - a.y) * (b.x - a.x);
            };

            float area = edgeFunction(v0.screenPos, v1.screenPos, v2.screenPos);
            if (area == 0) continue; // Degenerate triangle

            for (int y = minY; y <= maxY; ++y) {
                for (int x = minX; x <= maxX; ++x) {
                    Vec4 pixelCenter((float)x + 0.5f, (float)y + 0.5f, 0, 0);

                    // Barycentric weights
                    float w0 = edgeFunction(v1.screenPos, v2.screenPos, pixelCenter);
                    float w1 = edgeFunction(v2.screenPos, v0.screenPos, pixelCenter);
                    float w2 = edgeFunction(v0.screenPos, v1.screenPos, pixelCenter);

                    // Back-face culling & Inside test
                    // Note: OpenGL default winding is CCW.
                    if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                        // Normalize weights
                        w0 /= area; w1 /= area; w2 /= area;

                        // 3. Perspective Correct Interpolation
                        // Formula: Attr = (Attr0*w0/z0 + Attr1*w1/z1 + Attr2*w2/z2) / (w0/z0 + w1/z1 + w2/z2)
                        // Here v.screenPos.w stores 1/z (or 1/w_clip)
                        
                        float zInv = w0 * v0.screenPos.w + w1 * v1.screenPos.w + w2 * v2.screenPos.w;
                        float z = 1.0f / zInv; // Depth

                        // Z-Buffer Test
                        int pixelIdx = y * fbWidth + x;
                        if (z < depthBuffer[pixelIdx]) { // Assuming standard GL depth (less is closer)
                             // Pass Z-test
                             depthBuffer[pixelIdx] = z;

                             // Interpolate Varyings
                             ShaderContext fsIn;
                             for(int k=0; k<4; k++) {
                                 // Interpolate attribute divided by w
                                 Vec4 attr0 = v0.ctx.varyings[k] * v0.screenPos.w;
                                 Vec4 attr1 = v1.ctx.varyings[k] * v1.screenPos.w;
                                 Vec4 attr2 = v2.ctx.varyings[k] * v2.screenPos.w;
                                 
                                 Vec4 interpolated = attr0 * w0 + attr1 * w1 + attr2 * w2;
                                 // Multiply by correct w to recover
                                 fsIn.varyings[k] = interpolated * z; 
                             }

                             // 4. Fragment Shader
                             Vec4 color = prog->fragmentShader(fsIn);

                             // 5. Write to Framebuffer (with simple blending or overwrite)
                             uint8_t r = (uint8_t)(std::clamp(color.x, 0.0f, 1.0f) * 255);
                             uint8_t g = (uint8_t)(std::clamp(color.y, 0.0f, 1.0f) * 255);
                             uint8_t b = (uint8_t)(std::clamp(color.z, 0.0f, 1.0f) * 255);
                             
                             colorBuffer[pixelIdx] = (255 << 24) | (b << 16) | (g << 8) | r; // ABGR for display usually
                        }
                    }
                }
            }
        }
    }

    // 辅助：输出 PPM 图片以验证结果
    void savePPM(const char* filename) {
        FILE* f = fopen(filename, "wb");
        fprintf(f, "P6\n%d %d\n255\n", fbWidth, fbHeight);
        for(int i=0; i<fbWidth*fbHeight; i++) {
            uint32_t p = colorBuffer[i];
            // colorBuffer is 0xAABBGGRR (Little Endian) or 0xRRGGBBAA depending on view
            // My implementation stored: (255 << 24) | (b << 16) | (g << 8) | r
            // So low byte is R.
            uint8_t r = p & 0xFF;
            uint8_t g = (p >> 8) & 0xFF;
            uint8_t b = (p >> 16) & 0xFF;
            fwrite(&r, 1, 1, f);
            fwrite(&g, 1, 1, f);
            fwrite(&b, 1, 1, f);
        }
        fclose(f);
        std::cout << "Saved " << filename << std::endl;
    }
};

// ==========================================
// 6. 用户代码 (Main)
// ==========================================
int main() {
    SoftRenderContext ctx;

    // --- 1. 创建并设置 Shader ---
    GLuint program = ctx.glCreateProgram();
    GLint uMVP = ctx.glGetUniformLocation(program, "uMVP");
    GLint uTexture = ctx.glGetUniformLocation(program, "uTexture");

    // 定义 Vertex Shader (模拟 GLSL)
    // layout(location=0) in vec3 pos;
    // layout(location=1) in vec2 uv;
    // out vec2 vUV;
    ProgramObject* progObj = ctx.getProgram();
    progObj->vertexShader = [uMVP, &ctx, program](const std::vector<Vec4>& attribs, ShaderContext& outCtx) -> Vec4 {
        Vec4 pos = attribs[0]; // loc 0
        Vec4 uv  = attribs[1]; // loc 1
        
        // Output Varying
        outCtx.varyings[0] = uv; 

        // Get Uniform MVP
        // 注意：这里需要稍微黑客一点的方式从 Program 中取值，实际中会有更封装的 getters
        // 我们为了演示直接假定数据在
        // 在实际软渲染器中，Shader是无状态的，Uniform应通过参数传入，这里为了简化 Lambda 捕获了 ctx
        const float* m = ctx.getProgram()->uniforms[uMVP].data.mat;
        Mat4 mvp; memcpy(mvp.m, m, 16*sizeof(float));
        
        // pos.w = 1.0
        pos.w = 1.0f;
        return mvp * pos;
    };

    // 定义 Fragment Shader
    // in vec2 vUV;
    // uniform sampler2D uTexture;
    progObj->fragmentShader = [uTexture, &ctx, program](const ShaderContext& inCtx) -> Vec4 {
        Vec4 uv = inCtx.varyings[0];
        
        // Get Sampler Unit ID
        int unit = ctx.getProgram()->uniforms[uTexture].data.i;
        
        TextureObject* tex = ctx.getTexture(unit);
        if (tex) {
            return tex->sample(uv.x, uv.y);
        }
        return Vec4(1, 0, 0, 1); // Red fallback
    };

    // --- 2. 准备纹理数据 (Checkerboard) ---
    GLuint texID;
    ctx.glGenTextures(1, &texID);
    ctx.glActiveTexture(0x84C0 + 0); // GL_TEXTURE0
    ctx.glBindTexture(GL_TEXTURE_2D, texID);
    
    std::vector<uint32_t> texData(256 * 256);
    for(int y=0; y<256; y++) {
        for(int x=0; x<256; x++) {
            bool c = ((x/32) + (y/32)) % 2 == 0;
            texData[y*256+x] = c ? 0xFFFFFFFF : 0xFF0000FF; // White or Blue
        }
    }
    ctx.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, texData.data());

    // --- 3. 准备顶点数据 (Quad) ---
    float vertices[] = {
        // Pos(x,y,z)    UV(u,v)
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

    // Setup Attributes
    // Loc 0: Pos
    ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 5*sizeof(float), (void*)0);
    ctx.glEnableVertexAttribArray(0);
    // Loc 1: UV
    ctx.glVertexAttribPointer(1, 2, GL_FLOAT, false, 5*sizeof(float), (void*)(3*sizeof(float)));
    ctx.glEnableVertexAttribArray(1);

    // --- 4. 设置 Uniforms 并绘制 ---
    ctx.glUseProgram(program);
    
    // 设置 Texture Unit 0
    ctx.glUniform1i(uTexture, 0);

    // 设置 MVP (简单的 Identity，因为我们的顶点本身就在 -0.5 ~ 0.5，符合屏幕显示范围)
    Mat4 mvp = Mat4::Identity();
    // 稍微缩放一下
    mvp.m[0][0] = 1.5f; mvp.m[1][1] = 1.5f; 
    ctx.glUniformMatrix4fv(uMVP, 1, false, (float*)&mvp);

    // Draw
    ctx.glDrawElements(GL_TRIANGLES, 6, 0, 0);

    // --- 5. 输出结果 ---
    ctx.savePPM("output.ppm");

    return 0;
}