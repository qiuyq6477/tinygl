#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/tinygl.h>
#include <vector>
#include <cmath>

using namespace tinygl;
using namespace framework;

struct Vertex {
    float pos[3];
    float normal[3];
};

struct Material {
    Vec4 ambient = {1.0f, 0.5f, 0.31f, 1.0f};
    Vec4 diffuse = {1.0f, 0.5f, 0.31f, 1.0f};
    Vec4 specular = {0.5f, 0.5f, 0.5f, 1.0f};
    float shininess = 32.0f;
};

struct Light {
    Vec4 position = {1.2f, 1.0f, 2.0f, 1.0f};
    Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    float ambientStrength = 0.1f;
};

struct BlinnPhongShader : public ShaderBuiltins {
    // Uniforms
    Mat4 model;
    Mat4 view;
    Mat4 projection;
    Vec4 viewPos;
    
    Material material;
    Light light;
    bool useBlinnPhong = true;

    // Vertex Shader
    // Input: attribs[0] = pos, attribs[1] = normal
    inline void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        // Transform Position to World Space
        Vec4 localPos = attribs[0];
        localPos.w = 1.0f; // Ensure w is 1
        Vec4 worldPos = model * localPos;
        
        // Pass World Position to Fragment Shader
        outCtx.varyings[0] = worldPos;

        // Transform Normal to World Space
        // Note: For correct normal transformation with non-uniform scaling, 
        // we need the normal matrix (transpose of inverse of model).
        // For simple rigid body transforms, model matrix is sufficient.
        Vec4 localNormal = attribs[1];
        localNormal.w = 0.0f; // Vectors have w=0
        Vec4 worldNormal = model * localNormal;
        
        // Pass Normal to Fragment Shader
        outCtx.varyings[1] = normalize(worldNormal);

        // Transform to Clip Space
        gl_Position = projection * view * worldPos;
    }

    // Fragment Shader
    // Input: inCtx.varyings[0] = FragPos, inCtx.varyings[1] = Normal
    void fragment(const ShaderContext& inCtx) {
        // 0. Data Preparation
        Vec4 fragPos = inCtx.varyings[0];
        Vec4 normal = normalize(inCtx.varyings[1]);
        Vec4 lightDir = normalize(light.position - fragPos);
        Vec4 viewDir = normalize(viewPos - fragPos);

        // 1. Ambient
        Vec4 ambient = light.color * material.ambient * light.ambientStrength;

        // 2. Diffuse
        float diff = std::max(dot(normal, lightDir), 0.0f);
        Vec4 diffuse = light.color * material.diffuse * diff;

        // 3. Specular
        Vec4 specular = {0.0f, 0.0f, 0.0f, 0.0f};
        if (diff > 0.0f) {
            float spec = 0.0f;
            if (useBlinnPhong) {
                Vec4 halfwayDir = normalize(lightDir + viewDir);
                spec = std::pow(std::max(dot(normal, halfwayDir), 0.0f), material.shininess);
            } else {
                Vec4 reflectDir = reflect(lightDir * -1.0f, normal);
                spec = std::pow(std::max(dot(viewDir, reflectDir), 0.0f), material.shininess);
            }
            specular = light.color * material.specular * spec;
        }

        Vec4 result = ambient + diffuse + specular;
        result.w = 1.0f;
        gl_FragColor = result;
    }
};

struct LightCubeShader : public ShaderBuiltins {
    Mat4 mvp;
    Vec4 lightColor;

    inline void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        Vec4 localPos = attribs[0];
        localPos.w = 1.0f; 
        gl_Position = mvp * localPos;
    }

    void fragment(const ShaderContext& inCtx) {
        gl_FragColor = lightColor;
    }
};

class BlinnPhongTest : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // Define a cube with normals
        // Each face needs its own vertices because normals are different
        std::vector<Vertex> vertices = {
            // Front face (Normal 0, 0, 1)
            {{-0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},

            // Back face (Normal 0, 0, -1)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, -1.0f}},

            // Left face (Normal -1, 0, 0)
            {{-0.5f, -0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f, -0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f,  0.5f,  0.5f}, {-1.0f, 0.0f, 0.0f}},
            {{-0.5f,  0.5f, -0.5f}, {-1.0f, 0.0f, 0.0f}},

            // Right face (Normal 1, 0, 0)
            {{ 0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {1.0f, 0.0f, 0.0f}},

            // Top face (Normal 0, 1, 0)
            {{-0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{ 0.5f,  0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},

            // Bottom face (Normal 0, -1, 0)
            {{-0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{ 0.5f, -0.5f, -0.5f}, {0.0f, -1.0f, 0.0f}},
            {{ 0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}},
            {{-0.5f, -0.5f,  0.5f}, {0.0f, -1.0f, 0.0f}},
        };
        
        // Indices for 2 triangles per face
        // 0,1,2, 2,3,0
        std::vector<uint32_t> indices;
        for (int i = 0; i < 6; ++i) {
            uint32_t start = i * 4;
            indices.push_back(start);
            indices.push_back(start + 1);
            indices.push_back(start + 2);
            indices.push_back(start + 2);
            indices.push_back(start + 3);
            indices.push_back(start);
        }

        ctx.glGenVertexArrays(1, &m_vao);
        ctx.glBindVertexArray(m_vao);

        ctx.glGenBuffers(1, &m_vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &m_ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), indices.data(), GL_STATIC_DRAW);

        // Vertex Attributes
        // 0: Position
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, pos));
        ctx.glEnableVertexAttribArray(0);
        
        // 1: Normal
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        ctx.glEnableVertexAttribArray(1);

        // ctx.glEnable(GL_DEPTH_TEST);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteBuffers(1, &m_ebo);
        ctx.glDeleteVertexArrays(1, &m_vao);
    }

    void onUpdate(float dt) override {
        m_rotationAngle += m_rotationSpeed * dt;
        if (m_rotationAngle > 360.0f) m_rotationAngle -= 360.0f;
        m_shader.model = Mat4::RotateY(m_rotationAngle);
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_label(ctx, "Lighting Model");
        int model = m_shader.useBlinnPhong ? 1 : 0;
        if (mu_header_ex(ctx, "Model Selection", MU_OPT_EXPANDED)) {
             if (mu_checkbox(ctx, "Use Blinn-Phong", &model)) {
                 m_shader.useBlinnPhong = model ? true : false;
             }
        }

        if (mu_header_ex(ctx, "Rotation", MU_OPT_EXPANDED)) {
            mu_label(ctx, "Speed (deg/s)");
            mu_slider(ctx, &m_rotationSpeed, 0.0f, 360.0f);
        }

        if (mu_header_ex(ctx, "Light Properties", MU_OPT_EXPANDED)) {
            mu_label(ctx, "Position");
            mu_slider(ctx, &m_shader.light.position.x, -5.0f, 5.0f);
            mu_slider(ctx, &m_shader.light.position.y, -5.0f, 5.0f);
            mu_slider(ctx, &m_shader.light.position.z, -5.0f, 5.0f);
            
            mu_label(ctx, "Color");
            mu_slider(ctx, &m_shader.light.color.x, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.light.color.y, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.light.color.z, 0.0f, 1.0f);
            
            mu_label(ctx, "Ambient Strength");
            mu_slider(ctx, &m_shader.light.ambientStrength, 0.0f, 1.0f);
        }

        if (mu_header_ex(ctx, "Material Properties", MU_OPT_EXPANDED)) {
            mu_label(ctx, "Ambient");
            mu_slider(ctx, &m_shader.material.ambient.x, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.material.ambient.y, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.material.ambient.z, 0.0f, 1.0f);

            mu_label(ctx, "Diffuse");
            mu_slider(ctx, &m_shader.material.diffuse.x, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.material.diffuse.y, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.material.diffuse.z, 0.0f, 1.0f);
            
            mu_label(ctx, "Specular");
            mu_slider(ctx, &m_shader.material.specular.x, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.material.specular.y, 0.0f, 1.0f);
            mu_slider(ctx, &m_shader.material.specular.z, 0.0f, 1.0f);
            
            mu_label(ctx, "Shininess");
            mu_slider(ctx, &m_shader.material.shininess, 2.0f, 256.0f);
        }
    }

    void onRender(SoftRenderContext& ctx) override {
        // Setup View/Projection
        const auto& vp = ctx.glGetViewport();
        float aspect = (float)vp.w / (float)vp.h;
        
        m_shader.projection = Mat4::Perspective(45.0f, aspect, 0.1f, 100.0f);
        
        // Simple camera orbiting around 0,0,0
        Vec4 eye = {2.0f, 3.0f, 5.0f, 1.0f}; 
        Vec4 center = {0.0f, 0.0f, 0.0f, 1.0f};
        Vec4 up = {0.0f, 1.0f, 0.0f, 0.0f};
        m_shader.view = Mat4::LookAt(eye, center, up);
        m_shader.viewPos = eye;

        // Draw
        ctx.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); 
        
        ctx.glDrawElements(m_shader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0);

        // Draw Light Cube
        m_lightShader.lightColor = m_shader.light.color;
        
        Mat4 lightModel = Mat4::Translate(m_shader.light.position.x, m_shader.light.position.y, m_shader.light.position.z) * Mat4::Scale(0.2f, 0.2f, 0.2f);
        m_lightShader.mvp = m_shader.projection * m_shader.view * lightModel;

        ctx.glDrawElements(m_lightShader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void*)0);
    }

private:
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    BlinnPhongShader m_shader;
    LightCubeShader m_lightShader;
    float m_rotationAngle = 0.0f;
    float m_rotationSpeed = 45.0f;
};

static TestRegistrar registrar("Lighting", "BlinnPhong", []() { return new BlinnPhongTest(); });
