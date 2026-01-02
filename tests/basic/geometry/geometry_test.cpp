#include "../../ITestCase.h"
#include "../../test_registry.h"
#include <tinygl/framework/geometry.h>
#include <tinygl/core/gl_shader.h>
#include <tinygl/framework/camera.h>
#include <vector>
#include <cmath>
#include <cstdio>

using namespace tinygl;

struct GeometryShader {
    Mat4 mvp;
    Mat4 model;

    Vec4 vertex(Vec4* attribs, ShaderContext& ctx) {
        Vec4 pos = attribs[0];
        Vec4 norm = attribs[1];
        // attribs[2] is tangent
        // attribs[3] is bitangent
        Vec4 uv = attribs[4]; // UV is now at index 4

        ctx.varyings[0] = model * norm; // World Normal
        ctx.varyings[1] = uv;

        return mvp * pos;
    }

    Vec4 fragment(const ShaderContext& in) {
        Vec4 normal = normalize(in.varyings[0]);
        Vec4 uv = in.varyings[1];

        Vec4 lightDir = normalize(Vec4(1, 1, 1, 0));
        float diff = std::max(dot(normal, lightDir), 0.2f);

        // Checkerboard pattern
        float check = (mod(uv.x * 10.0f, 1.0f) > 0.5f) ^ (mod(uv.y * 10.0f, 1.0f) > 0.5f) ? 1.0f : 0.5f;
        return Vec4(diff * check, diff * check, diff * check, 1.0f);
    }
};

class GeometryTest : public ITestCase {
    GLuint vao = 0, vbo = 0, ebo = 0;
    int currentShapeIdx = 1; // Start with Cube
    float rotation = 0.0f;
    float rotationSpeed = 30.0f; // Degrees per second
    size_t indexCount = 0;
    int lastShape = -1;
    Camera camera;

    void updateGeometry(SoftRenderContext& ctx) {
        if (vao) ctx.glDeleteVertexArrays(1, &vao);
        if (vbo) ctx.glDeleteBuffers(1, &vbo);
        if (ebo) ctx.glDeleteBuffers(1, &ebo);

        ctx.glGenVertexArrays(1, &vao);
        ctx.glGenBuffers(1, &vbo);
        ctx.glGenBuffers(1, &ebo);

        Geometry geo;
        switch(currentShapeIdx) {
            case 0: geo = geometry::createPlane(1.0f, 1.0f); break;
            case 1: geo = geometry::createCube(1.0f); break;
            case 2: geo = geometry::createSphere(1.0f, 24); break;
            case 3: geo = geometry::createTorus(0.3f, 0.7f, 24, 24); break;
            case 4: geo = geometry::createCylinder(1.0f, 0.5f, 24); break;
        }

        ctx.glBindVertexArray(vao);
        
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        // Use allAttributes directly
        ctx.glBufferData(GL_ARRAY_BUFFER, geo.allAttributes.size() * sizeof(float), geo.allAttributes.data(), GL_STATIC_DRAW);

        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, geo.indices.size() * sizeof(uint32_t), geo.indices.data(), GL_STATIC_DRAW);

        indexCount = geo.indices.size();

        // Stride = 15 floats (4+3+3+3+2)
        size_t stride = 15 * sizeof(float);

        // Pos (0) - 4 floats
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, stride, (void*)0);
        
        // Normal (1) - 3 floats
        ctx.glEnableVertexAttribArray(1);
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));
        
        // Tangent (2) - 3 floats
        ctx.glEnableVertexAttribArray(2);
        ctx.glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(7 * sizeof(float)));

        // Bitangent (3) - 3 floats
        ctx.glEnableVertexAttribArray(3);
        ctx.glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, (void*)(10 * sizeof(float)));

        // UV (4) - 2 floats
        ctx.glEnableVertexAttribArray(4);
        ctx.glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, stride, (void*)(13 * sizeof(float)));
    }

public:
    GeometryTest() : camera(Vec4(0, 0, 4, 1)) {}

    void init(SoftRenderContext& ctx) override {
        updateGeometry(ctx);
    }

    void destroy(SoftRenderContext& ctx) override {
        ctx.glDeleteVertexArrays(1, &vao);
        ctx.glDeleteBuffers(1, &vbo);
        ctx.glDeleteBuffers(1, &ebo);
    }

    void onEvent(const SDL_Event& e) override {
        camera.ProcessEvent(e);
    }

    void onUpdate(float dt) override {
        camera.Update(dt);
        rotation += rotationSpeed * dt;
    }

    void onGui(mu_Context* ctx, const Rect& rect) override {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        if (mu_header_ex(ctx, "Geometry Select", MU_OPT_EXPANDED)) {
            static const char* shapes[] = { "Plane", "Cube", "Sphere", "Torus", "Cylinder" };
            
            for (int i=0; i<5; ++i) {
                if (mu_button(ctx, shapes[i])) {
                    currentShapeIdx = i;
                }
            }
        }
        
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_label(ctx, "Rotation Speed:");
        mu_slider(ctx, &rotationSpeed, 0.0f, 360.0f);
        
        char buf[64];
        snprintf(buf, sizeof(buf), "Vertices: %zu", indexCount);
        mu_label(ctx, buf);

        snprintf(buf, sizeof(buf), "Pos: %.2f, %.2f, %.2f", camera.position.x, camera.position.y, camera.position.z);
        mu_label(ctx, buf);
    }

    void onRender(SoftRenderContext& ctx) override {
        if (lastShape != currentShapeIdx) {
            updateGeometry(ctx);
            lastShape = currentShapeIdx;
        }

        GeometryShader shader;
        shader.model = Mat4::RotateY(rotation) * Mat4::RotateX(30.0f);
        
        float aspect = (float)ctx.getWidth() / (float)ctx.getHeight();
        Mat4 view = camera.GetViewMatrix();
        Mat4 proj = Mat4::Perspective(camera.fov, aspect, camera.zNear, camera.zFar);
        shader.mvp = proj * view * shader.model;
        
        ctx.glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        ctx.glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        
        ctx.glEnable(GL_DEPTH_TEST);
        ctx.glBindVertexArray(vao);
        ctx.glDrawElements(shader, GL_TRIANGLES, (GLsizei)indexCount, GL_UNSIGNED_INT, 0);
    }
};

static TestRegistrar registry("Basic", "Geometry", []() { return new GeometryTest(); });
