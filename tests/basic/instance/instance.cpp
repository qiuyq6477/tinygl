#include <tinygl/core/tinygl.h>
#include <tinygl/framework/application.h>
#include <tinygl/framework/camera.h>
#include "../../ITestCase.h"
#include "../../test_registry.h"
// 实例数据结构 (Instance Data)
// 我们将把这些数据传给 VBO，并设置 Divisor = 1
struct InstanceData {
    float x, y, z;       // Offset (位置)
    float r, g, b;       // Color
    float spdX, spdY;    // Rotation Speed (X轴速度, Y轴速度)
    float phase;         // Initial Phase (初始角度偏移)
};


struct InstancedCubeShader : public tinygl::ShaderBuiltins {
    // Uniforms
    Mat4 viewProj;
    float time;

    // Vertex Shader
    // attribs[0]: Local Position (Divisor 0)
    // attribs[1]: Instance Offset (Divisor 1)
    // attribs[2]: Instance Color  (Divisor 1)
    // attribs[3]: Rotation Params (Divisor 1) -> (SpeedX, SpeedY, Phase, padding)
    void vertex(const Vec4* attribs, ShaderContext& ctx) {
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
        gl_Position = viewProj * worldPos;
    }

    // Fragment Shader
    void fragment(const ShaderContext& ctx) {
        // 直接输出插值后的颜色
        gl_FragColor = ctx.varyings[0];
    }
};



class InstanceTest : public ITestCase {
public:
    void init(SoftRenderContext& ctx) override {
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
        // Create VAO
        ctx.glGenVertexArrays(1, &vao);
        ctx.glBindVertexArray(vao);

        // 1. 创建并上传 Cube Mesh
        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

        // 2. 生成 100 个实例的随机数据
        instances.resize(INSTANCE_COUNT);
        for (int i = 0; i < INSTANCE_COUNT; ++i) {
            // 在 -4 到 4 的空间内随机分布
            instances[i].x = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;
            instances[i].y = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;
            instances[i].z = ((float)rand() / RAND_MAX) * 8.0f - 6.0f; // 稍微拉开一点深度

            // 随机颜色
            instances[i].r = (float)rand() / RAND_MAX;
            instances[i].g = (float)rand() / RAND_MAX;
            instances[i].b = (float)rand() / RAND_MAX;

            // 随机旋转参数
            instances[i].spdX = ((float)rand() / RAND_MAX) * 2.0f + 0.5f; // 0.5 ~ 2.5
            instances[i].spdY = ((float)rand() / RAND_MAX) * 2.0f + 0.5f;
            instances[i].phase = ((float)rand() / RAND_MAX) * 3.14f * 2.0f;
        }

        // 3. 创建并上传 Instance Buffer
        ctx.glGenBuffers(1, &vbo_instance);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo_instance);
        ctx.glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData), instances.data(), GL_STATIC_DRAW);

        // 4. 配置 VAO 属性
        // ------------------------------------------------
        // 属性 0: Local Position (vec3) -> 数据源: vbo
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(float), (void*)0);
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribDivisor(0, 0); // Divisor 0: 每个顶点更新 (Mesh)

        // 属性 1: Instance Offset (vec3) -> 数据源: vbo_instance
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo_instance);
        // stride = sizeof(InstanceData), offset = 0
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, x));
        ctx.glEnableVertexAttribArray(1);
        ctx.glVertexAttribDivisor(1, 1); // Divisor 1: 每个实例更新 (Instance Data)

        // 属性 2: Instance Color (vec3) -> 数据源: vbo_instance
        // stride = sizeof(InstanceData), offset = 12 (3 floats)
        ctx.glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, r));
        ctx.glEnableVertexAttribArray(2);
        ctx.glVertexAttribDivisor(2, 1); // Divisor 1

        // 属性 3: Rotation Params (vec3) -> 数据源: vbo_instance
        // stride = sizeof(InstanceData), offset = 24 (6 floats)
        ctx.glVertexAttribPointer(3, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, spdX));
        ctx.glEnableVertexAttribArray(3);
        ctx.glVertexAttribDivisor(3, 1); // Divisor 1
        
        // 绑定 EBO
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteBuffers(1, &ebo);
        ctx.glDeleteBuffers(1, &vbo_instance);
        ctx.glDeleteVertexArrays(1, &vao);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        // Add specific controls for this test
    }

    void onUpdate(float dt) override {
        // 更新时间
        total_time += dt;
        shader.time = total_time;
    }

    void onRender(SoftRenderContext& ctx) override {
        // View Matrix (LookAt 0,0,0)
        // 这里简单模拟一个 LookAt，实际应使用完整的 LookAt 函数
        // 为了简化，我们只做一个平移 + 简单的透视
        const auto& vp = ctx.glGetViewport();
        float aspect = (float)vp.w / (float)vp.h;
        Mat4 proj = Mat4::Perspective(60.0f, aspect, 0.1f, 100.0f);
        
        // 简单的 View: 向后退 Z=12
        Mat4 view = Mat4::Translate(0.0f, 0.0f, -12.0f); 

        // 添加一点相机的摆动
        view = view * Mat4::RotateZ(sin(total_time) * 5.0f); 

        shader.viewProj = proj * view;

        // 5. 绘制调用
        // 使用 glDrawElementsInstanced 一次性绘制 100 个 Cube
        // 36 indices, GL_UNSIGNED_INT, offset 0, 100 instances
        ctx.glDrawElementsInstanced(shader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0, INSTANCE_COUNT);

    }

private:
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;
    GLuint vbo_instance = 0;
    int INSTANCE_COUNT = 100;
    std::vector<InstanceData> instances;
    InstancedCubeShader shader;

    // 简单的相机参数
    float cam_yaw = 0.0f;
    float total_time = 0.0f;
};

// Register the test
static TestRegistrar registrar("Basic", "Instance", []() { return new InstanceTest(); });
