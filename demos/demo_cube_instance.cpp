#include "../vc/tinygl.h" 
#include "../vc/vc.cpp"

#define DEMO_WIDTH 800
#define DEMO_HEIGHT 600

// ==========================================
// 1. 数据定义
// ==========================================

// 立方体顶点 (Local Space)
// 为了简单，不使用法线和UV，只用位置
float cubeVertices[] = {
    // Front face
    -0.5f, -0.5f,  0.5f,
     0.5f, -0.5f,  0.5f,
     0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    // Back face
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
     0.5f,  0.5f, -0.5f,
     0.5f, -0.5f, -0.5f,
};

// 立方体索引
uint32_t cubeIndices[] = {
    0, 1, 2, 2, 3, 0, // Front
    4, 5, 6, 6, 7, 4, // Back
    3, 2, 6, 6, 5, 3, // Top
    0, 4, 7, 7, 1, 0, // Bottom
    1, 7, 6, 6, 2, 1, // Right
    4, 0, 3, 3, 5, 4  // Left
};

// 实例数据结构 (Instance Data)
// 我们将把这些数据传给 VBO，并设置 Divisor = 1
struct InstanceData {
    float x, y, z;       // Offset (位置)
    float r, g, b;       // Color
    float spdX, spdY;    // Rotation Speed (X轴速度, Y轴速度)
    float phase;         // Initial Phase (初始角度偏移)
};

// ==========================================
// 2. Shader 定义
// ==========================================

struct InstancedCubeShader {
    // Uniforms
    Mat4 viewProj;
    float time;

    // Vertex Shader
    // attribs[0]: Local Position (Divisor 0)
    // attribs[1]: Instance Offset (Divisor 1)
    // attribs[2]: Instance Color  (Divisor 1)
    // attribs[3]: Rotation Params (Divisor 1) -> (SpeedX, SpeedY, Phase, padding)
    Vec4 vertex(const Vec4* attribs, ShaderContext& ctx) {
        Vec4 localPos = attribs[0];
        Vec4 offset   = attribs[1];
        Vec4 color    = attribs[2];
        Vec4 rotParams= attribs[3];

        // 1. 将颜色传递给 Fragment Shader
        ctx.varyings[0] = color;

        // 2. 在 Shader 中动态计算旋转矩阵
        // 这样我们不需要每帧在 CPU 更新 100 个矩阵
        float angleX = rotParams.x * time + rotParams.z; // speed * time + phase
        float angleY = rotParams.y * time + rotParams.z;

        // 简单的旋转矩阵组合 (Y * X)
        Mat4 matRot = Mat4::RotateY(angleY * 50.0f) * Mat4::RotateX(angleX * 50.0f);
        
        // 3. 应用旋转
        Vec4 rotatedPos = matRot * localPos;

        // 4. 应用实例偏移 (World Space)
        Vec4 worldPos = rotatedPos + offset;
        worldPos.w = 1.0f;

        // 5. 应用 View Projection
        return viewProj * worldPos;
    }

    // Fragment Shader
    uint32_t fragment(const ShaderContext& ctx) {
        // 直接输出插值后的颜色
        Vec4 c = ctx.varyings[0];
        
        // 简单的漫反射模拟 (这里为了性能直接输出纯色)
        return OLIVEC_RGBA(
            (int)(c.x * 255),
            (int)(c.y * 255),
            (int)(c.z * 255),
            255
        );
    }
};

// ==========================================
// 3. 全局变量与初始化
// ==========================================

static SoftRenderContext* g_ctx = nullptr;
static InstancedCubeShader g_shader;
static std::vector<InstanceData> g_instances;

// Buffer IDs
GLuint g_vao, g_vbo, g_ebo;
static GLuint g_vbo_instance;

const int INSTANCE_COUNT = 100;

void vc_init(void) {
    // 初始化上下文 (800x600)
    g_ctx = new SoftRenderContext(800, 600);

    // 1. 创建并上传 Cube Mesh
    g_ctx->glGenBuffers(1, &g_vbo);
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

    g_ctx->glGenBuffers(1, &g_ebo);
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

    // 2. 生成 100 个实例的随机数据
    g_instances.resize(INSTANCE_COUNT);
    for (int i = 0; i < INSTANCE_COUNT; ++i) {
        // 在 -4 到 4 的空间内随机分布
        g_instances[i].x = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;
        g_instances[i].y = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;
        g_instances[i].z = ((float)rand() / RAND_MAX) * 8.0f - 6.0f; // 稍微拉开一点深度

        // 随机颜色
        g_instances[i].r = (float)rand() / RAND_MAX;
        g_instances[i].g = (float)rand() / RAND_MAX;
        g_instances[i].b = (float)rand() / RAND_MAX;

        // 随机旋转参数
        g_instances[i].spdX = ((float)rand() / RAND_MAX) * 2.0f + 0.5f; // 0.5 ~ 2.5
        g_instances[i].spdY = ((float)rand() / RAND_MAX) * 2.0f + 0.5f;
        g_instances[i].phase = ((float)rand() / RAND_MAX) * 3.14f * 2.0f;
    }

    // 3. 创建并上传 Instance Buffer
    g_ctx->glGenBuffers(1, &g_vbo_instance);
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo_instance);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, g_instances.size() * sizeof(InstanceData), g_instances.data(), GL_STATIC_DRAW);

    // 4. 配置 VAO 属性
    // ------------------------------------------------
    // 属性 0: Local Position (vec3) -> 数据源: g_vbo
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(float), (void*)0);
    g_ctx->glEnableVertexAttribArray(0);
    g_ctx->glVertexAttribDivisor(0, 0); // Divisor 0: 每个顶点更新 (Mesh)

    // 属性 1: Instance Offset (vec3) -> 数据源: g_vbo_instance
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo_instance);
    // stride = sizeof(InstanceData), offset = 0
    g_ctx->glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, x));
    g_ctx->glEnableVertexAttribArray(1);
    g_ctx->glVertexAttribDivisor(1, 1); // Divisor 1: 每个实例更新 (Instance Data)

    // 属性 2: Instance Color (vec3) -> 数据源: g_vbo_instance
    // stride = sizeof(InstanceData), offset = 12 (3 floats)
    g_ctx->glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, r));
    g_ctx->glEnableVertexAttribArray(2);
    g_ctx->glVertexAttribDivisor(2, 1); // Divisor 1

    // 属性 3: Rotation Params (vec3) -> 数据源: g_vbo_instance
    // stride = sizeof(InstanceData), offset = 24 (6 floats)
    g_ctx->glVertexAttribPointer(3, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, spdX));
    g_ctx->glEnableVertexAttribArray(3);
    g_ctx->glVertexAttribDivisor(3, 1); // Divisor 1
    
    // 绑定 EBO
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
}

void vc_input(SDL_Event *) {
}

// 简单的相机参数
float camYaw = 0.0f;
float totalTime = 0.0f;

Olivec_Canvas vc_render(float dt, void* pixels) {
    // 1. 设置外部 Buffer (SDL 纹理内存)
    g_ctx->setExternalBuffer((uint32_t*)pixels);
    
    // 2. 清屏
    g_ctx->glClearColor(0.1f, 0.1f, 0.15f, 1.0f); // 深蓝色背景
    g_ctx->glClear(COLOR | DEPTH);

    // 3. 更新时间
    totalTime += dt;
    g_shader.time = totalTime;

    // 4. 计算 View Projection 矩阵
    // 简单的环绕相机
    // float radius = 10.0f;
    // float camX = sin(totalTime * 0.5f) * radius;
    // float camZ = cos(totalTime * 0.5f) * radius;
    
    // View Matrix (LookAt 0,0,0)
    // 这里简单模拟一个 LookAt，实际应使用完整的 LookAt 函数
    // 为了简化，我们只做一个平移 + 简单的透视
    
    Mat4 proj = Mat4::Perspective(60.0f, 800.0f/600.0f, 0.1f, 100.0f);
    
    // 简单的 View: 向后退 Z=12
    Mat4 view = Mat4::Translate(0.0f, 0.0f, -12.0f); 

    // 添加一点相机的摆动
    view = view * Mat4::RotateZ(sin(totalTime) * 5.0f); 

    g_shader.viewProj = proj * view;

    // 5. 绘制调用
    // 使用 glDrawElementsInstanced 一次性绘制 100 个 Cube
    // 36 indices, GL_UNSIGNED_INT, offset 0, 100 instances
    g_ctx->glDrawElementsInstanced(g_shader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0, INSTANCE_COUNT);

    // 6. 返回 Canvas (TinyGL 的 buffer 指针包装)
    return olivec_canvas((uint32_t*)pixels, 800, 600, 800);
}