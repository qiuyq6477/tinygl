#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/core/tinygl.h>
#include <tinygl/framework/camera.h>
#include <vector>
#include <cmath>

using namespace tinygl;

// Vertex Structure
struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
};

// Material (Maps)
struct Material {
    TextureObject* diffuseMap = nullptr;
    TextureObject* specularMap = nullptr;
    float shininess = 32.0f;
};

// Light Structures
struct DirLight {
    bool enabled = true;
    Vec4 direction = {-0.2f, -1.0f, -0.3f, 0.0f};
    Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    float ambient = 0.05f;
    float diffuse = 0.4f;
    float specular = 0.5f;
};

struct PointLight {
    bool enabled = true;
    Vec4 position = {0.7f, 0.2f, 2.0f, 1.0f};
    Vec4 color = {1.0f, 0.0f, 0.0f, 1.0f}; // Reddish
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    float ambient = 0.05f;
    float diffuse = 0.8f;
    float specular = 1.0f;
};

struct SpotLight {
    bool enabled = true;
    Vec4 position = {0.0f, 0.0f, 0.0f, 1.0f};
    Vec4 direction = {0.0f, 0.0f, -1.0f, 0.0f};
    Vec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
    float cutOff = 12.5f; // degrees
    float outerCutOff = 15.0f; // degrees
    float constant = 1.0f;
    float linear = 0.09f;
    float quadratic = 0.032f;
    float ambient = 0.0f;
    float diffuse = 1.0f;
    float specular = 1.0f;
};

// Shader
struct LightingMapsShader : public ShaderBuiltins {
    Mat4 model;
    Mat4 view;
    Mat4 projection;
    Vec4 viewPos;

    Material material;
    DirLight dirLight;
    PointLight pointLight;
    SpotLight spotLight;

    // Vertex Shader
    inline void vertex(const Vec4* attribs, ShaderContext& outCtx) {
        Vec4 localPos = attribs[0];
        localPos.w = 1.0f;
        Vec4 worldPos = model * localPos;
        outCtx.varyings[0] = worldPos;

        // Normal (assuming uniform scaling)
        Vec4 localNormal = attribs[1];
        localNormal.w = 0.0f;
        Vec4 worldNormal = model * localNormal;
        outCtx.varyings[1] = normalize(worldNormal);

        // UV
        outCtx.varyings[2] = attribs[2];

        gl_Position = projection * view * worldPos;
    }

    // Helper: Directional Light
    Vec4 CalcDirLight(const DirLight& light, const Vec4& normal, const Vec4& viewDir, const Vec4& diffMap, const Vec4& specMap) {
        if (!light.enabled) return Vec4(0,0,0,0);
        
        Vec4 lightDir = normalize(light.direction * -1.0f);
        // Diffuse
        float diff = std::max(dot(normal, lightDir), 0.0f);
        // Specular
        Vec4 reflectDir = reflect(lightDir * -1.0f, normal);
        float spec = std::pow(std::max(dot(viewDir, reflectDir), 0.0f), material.shininess);
        
        // Combine
        Vec4 ambient  = light.color * diffMap * light.ambient;
        Vec4 diffuse  = light.color * diffMap * diff * light.diffuse;
        Vec4 specular = light.color * specMap * spec * light.specular;
        return ambient + diffuse + specular;
    }

    // Helper: Point Light
    Vec4 CalcPointLight(const PointLight& light, const Vec4& normal, const Vec4& fragPos, const Vec4& viewDir, const Vec4& diffMap, const Vec4& specMap) {
        if (!light.enabled) return Vec4(0,0,0,0);

        Vec4 lightDir = normalize(light.position - fragPos);
        // Diffuse
        float diff = std::max(dot(normal, lightDir), 0.0f);
        // Specular
        Vec4 reflectDir = reflect(lightDir * -1.0f, normal);
        float spec = std::pow(std::max(dot(viewDir, reflectDir), 0.0f), material.shininess);
        
        // Attenuation
        float distance = tinygl::distance(light.position, fragPos);
        float attenuation = 1.0f / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
        
        // Combine
        Vec4 ambient  = light.color * diffMap * light.ambient * attenuation;
        Vec4 diffuse  = light.color * diffMap * diff * light.diffuse * attenuation;
        Vec4 specular = light.color * specMap * spec * light.specular * attenuation;
        return ambient + diffuse + specular;
    }

    // Helper: Spot Light
    Vec4 CalcSpotLight(const SpotLight& light, const Vec4& normal, const Vec4& fragPos, const Vec4& viewDir, const Vec4& diffMap, const Vec4& specMap) {
        if (!light.enabled) return Vec4(0,0,0,0);

        Vec4 lightDir = normalize(light.position - fragPos);
        
        // Diffuse
        float diff = std::max(dot(normal, lightDir), 0.0f);
        // Specular
        Vec4 reflectDir = reflect(lightDir * -1.0f, normal);
        float spec = std::pow(std::max(dot(viewDir, reflectDir), 0.0f), material.shininess);
        
        // Attenuation
        float distance = tinygl::distance(light.position, fragPos);
        float attenuation = 1.0f / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
        
        // Intensity
        float theta = dot(lightDir, normalize(light.direction * -1.0f)); 
        float epsilon = std::cos(radians(light.cutOff)) - std::cos(radians(light.outerCutOff));
        float intensity = std::clamp((theta - std::cos(radians(light.outerCutOff))) / epsilon, 0.0f, 1.0f);
        
        // Combine
        // Note: Ambient is usually not affected by spotlight cone, but let's keep it simple
        Vec4 ambient  = light.color * diffMap * light.ambient * attenuation;
        Vec4 diffuse  = light.color * diffMap * diff * light.diffuse * attenuation * intensity;
        Vec4 specular = light.color * specMap * spec * light.specular * attenuation * intensity;
        return ambient + diffuse + specular;
    }

    // Fragment Shader
    void fragment(const ShaderContext& inCtx) {
        Vec4 fragPos = inCtx.varyings[0];
        Vec4 normal = normalize(inCtx.varyings[1]);
        Vec4 uv = inCtx.varyings[2];
        Vec4 viewDir = normalize(viewPos - fragPos);

        // Sample Textures
        Vec4 diffMap = material.diffuseMap ? material.diffuseMap->sample(uv.x, uv.y) : Vec4(1,1,1,1);
        Vec4 specMap = material.specularMap ? material.specularMap->sample(uv.x, uv.y) : Vec4(0.5,0.5,0.5,1);

        Vec4 result = Vec4(0,0,0,0);
        
        // Phase 1: Directional lighting
        result = result + CalcDirLight(dirLight, normal, viewDir, diffMap, specMap);
        // Phase 2: Point lights (Single)
        result = result + CalcPointLight(pointLight, normal, fragPos, viewDir, diffMap, specMap);
        // Phase 3: Spot light
        result = result + CalcSpotLight(spotLight, normal, fragPos, viewDir, diffMap, specMap);    
        
        result.w = 1.0f;
        gl_FragColor = result;
    }
};

class LightingMapsTest : public ITestCase {
public:
    Camera m_camera;
    GLuint m_vao = 0, m_vbo = 0, m_ebo = 0;
    GLuint m_diffuseTex = 0;
    GLuint m_specularTex = 0;
    LightingMapsShader m_shader;
    
    // Cube positions
    std::vector<Vec4> cubePositions = {
        { 0.0f,  0.0f,  0.0f, 1.0f},
        { 2.0f,  5.0f, -15.0f, 1.0f},
        {-1.5f, -2.2f, -2.5f, 1.0f},
        {-3.8f, -2.0f, -12.3f, 1.0f},
        { 2.4f, -0.4f, -3.5f, 1.0f},
        {-1.7f,  3.0f, -7.5f, 1.0f},
        { 1.3f, -2.0f, -2.5f, 1.0f},
        { 1.5f,  2.0f, -2.5f, 1.0f},
        { 1.5f,  0.2f, -1.5f, 1.0f},
        {-1.3f,  1.0f, -1.5f, 1.0f}
    };

    void init(SoftRenderContext& ctx) override {
        m_camera = Camera({.position = Vec4(0, 0, 3, 1)});

        // Load Textures 
        ctx.glGenTextures(1, &m_diffuseTex);
        tinygl::LoadTextureFromFile(ctx, "lighting/lighting_maps/assets/container2.png", GL_TEXTURE0, m_diffuseTex); 

        ctx.glGenTextures(1, &m_specularTex);
        tinygl::LoadTextureFromFile(ctx, "lighting/lighting_maps/assets/container2_specular.png", GL_TEXTURE1, m_specularTex); 

        // Setup Geometry
        setupCube(ctx);
        
        // ctx.glEnable(GL_DEPTH_TEST); // Not supported directly but good for intent? No, tinygl manages it via glClear
    }

    void setupCube(SoftRenderContext& ctx) {
         // Positions, Normals, UVs
         float vertices[] = {
            // positions          // normals           // texture coords
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
             0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,

            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
            -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
            -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
            -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
             0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
             0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
             0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
            -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
            -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
             0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
             0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
            -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
            -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
        };
        
        ctx.glGenVertexArrays(1, &m_vao);
        ctx.glBindVertexArray(m_vao);
        ctx.glGenBuffers(1, &m_vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        // Pos
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 8*sizeof(float), (void*)0);
        ctx.glEnableVertexAttribArray(0);
        // Normal
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, 8*sizeof(float), (void*)(3*sizeof(float)));
        ctx.glEnableVertexAttribArray(1);
        // UV
        ctx.glVertexAttribPointer(2, 2, GL_FLOAT, false, 8*sizeof(float), (void*)(6*sizeof(float)));
        ctx.glEnableVertexAttribArray(2);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteBuffers(1, &m_vbo);
        ctx.glDeleteVertexArrays(1, &m_vao);
        ctx.glDeleteTextures(1, &m_diffuseTex);
        ctx.glDeleteTextures(1, &m_specularTex);
    }

    void onEvent(const SDL_Event& e) override {
        m_camera.ProcessEvent(e);
    }

    void onUpdate(float dt) override {
        m_camera.Update(dt);
        // Update Spotlight to follow camera
        m_shader.spotLight.position = m_camera.position;
        m_shader.spotLight.direction = m_camera.front;
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_label(ctx, "Lighting Maps Controls");
        
        if (mu_header_ex(ctx, "Directional Light", MU_OPT_EXPANDED)) {
            int enabled = m_shader.dirLight.enabled;
            if (mu_checkbox(ctx, "Enabled", &enabled)) m_shader.dirLight.enabled = enabled;
            mu_label(ctx, "Direction");
            mu_slider(ctx, &m_shader.dirLight.direction.x, -1.0f, 1.0f);
            mu_slider(ctx, &m_shader.dirLight.direction.y, -1.0f, 1.0f);
            mu_slider(ctx, &m_shader.dirLight.direction.z, -1.0f, 1.0f);
            mu_label(ctx, "Color");
            mu_slider(ctx, &m_shader.dirLight.color.x, 0, 1);
            mu_slider(ctx, &m_shader.dirLight.color.y, 0, 1);
            mu_slider(ctx, &m_shader.dirLight.color.z, 0, 1);
        }

        if (mu_header_ex(ctx, "Point Light", MU_OPT_EXPANDED)) {
            int enabled = m_shader.pointLight.enabled;
            if (mu_checkbox(ctx, "Enabled", &enabled)) m_shader.pointLight.enabled = enabled;
            mu_label(ctx, "Position");
            mu_slider(ctx, &m_shader.pointLight.position.x, -5, 5);
            mu_slider(ctx, &m_shader.pointLight.position.y, -5, 5);
            mu_slider(ctx, &m_shader.pointLight.position.z, -5, 5);
            mu_label(ctx, "Color");
            mu_slider(ctx, &m_shader.pointLight.color.x, 0, 1);
            mu_slider(ctx, &m_shader.pointLight.color.y, 0, 1);
            mu_slider(ctx, &m_shader.pointLight.color.z, 0, 1);
        }

        if (mu_header_ex(ctx, "Spot Light", MU_OPT_EXPANDED)) {
            int enabled = m_shader.spotLight.enabled;
            if (mu_checkbox(ctx, "Enabled", &enabled)) m_shader.spotLight.enabled = enabled;
            mu_label(ctx, "Cutoff");
            mu_slider(ctx, &m_shader.spotLight.cutOff, 0, 90);
            mu_label(ctx, "Outer Cutoff");
            mu_slider(ctx, &m_shader.spotLight.outerCutOff, 0, 90);
            mu_label(ctx, "Color");
            mu_slider(ctx, &m_shader.spotLight.color.x, 0, 1);
            mu_slider(ctx, &m_shader.spotLight.color.y, 0, 1);
            mu_slider(ctx, &m_shader.spotLight.color.z, 0, 1);
        }
    }

    void onRender(SoftRenderContext& ctx) override {
        ctx.glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        m_shader.view = m_camera.GetViewMatrix();
        m_shader.projection = m_camera.GetProjectionMatrix();
        m_shader.viewPos = m_camera.position;

        m_shader.material.diffuseMap = ctx.getTextureObject(m_diffuseTex);
        m_shader.material.specularMap = ctx.getTextureObject(m_specularTex);

        // Draw multiple cubes
        for (int i = 0; i < 10; i++) {
            Mat4 model = Mat4::Translate(cubePositions[i].x, cubePositions[i].y, cubePositions[i].z);
            float angle = 20.0f * i;
            model = model * Mat4::RotateX(angle) * Mat4::RotateY(angle);
            m_shader.model = model;
            
            ctx.glDrawArrays(m_shader, GL_TRIANGLES, 0, 36);
        }
    }
};

static TestRegistrar registrar("Lighting", "LightingMaps", []() { return new LightingMapsTest(); });
