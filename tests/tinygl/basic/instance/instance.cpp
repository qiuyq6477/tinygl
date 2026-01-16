#include <tinygl/tinygl.h>
#include <framework/application.h>
#include <framework/camera.h>
#include <ITestCase.h>
#include <test_registry.h>
// 瀹炰緥鏁版嵁缁撴瀯 (Instance Data)
// 鎴戜滑灏嗘妸杩欎簺鏁版嵁浼犵粰 VBO锛屽苟璁剧疆 Divisor = 1
struct InstanceData {
    float x, y, z;       // Offset (浣嶇疆)
    float r, g, b;       // Color
    float spdX, spdY;    // Rotation Speed (X杞撮€熷害, Y杞撮€熷害)
    float phase;         // Initial Phase (鍒濆瑙掑害鍋忕Щ)
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

        // 1. 灏嗛鑹蹭紶閫掔粰 Fragment Shader
        ctx.varyings[0] = color;

        // 2. 鍦?Shader 涓姩鎬佽绠楁棆杞煩闃?
        // 杩欐牱鎴戜滑涓嶉渶瑕佹瘡甯у湪 CPU 鏇存柊 100 涓煩闃?
        float angleX = rotParams.x * time + rotParams.z; // speed * time + phase
        float angleY = rotParams.y * time + rotParams.z;

        // 绠€鍗曠殑鏃嬭浆鐭╅樀缁勫悎 (Y * X)
        Mat4 matRot = Mat4::RotateY(angleY * 50.0f) * Mat4::RotateX(angleX * 50.0f);
        
        // 3. 搴旂敤鏃嬭浆
        Vec4 rotatedPos = matRot * localPos;

        // 4. 搴旂敤瀹炰緥鍋忕Щ (World Space)
        Vec4 worldPos = rotatedPos + offset;
        worldPos.w = 1.0f;

        // 5. 搴旂敤 View Projection
        gl_Position = viewProj * worldPos;
    }

    // Fragment Shader
    void fragment(const ShaderContext& ctx) {
        // 鐩存帴杈撳嚭鎻掑€煎悗鐨勯鑹?
        gl_FragColor = ctx.varyings[0];
    }
};



class InstanceTest : public ITinyGLTestCase {
public:
    void init(SoftRenderContext& ctx) override {
        // 绔嬫柟浣撻《鐐?(Local Space)
        // 涓轰簡绠€鍗曪紝涓嶄娇鐢ㄦ硶绾垮拰UV锛屽彧鐢ㄤ綅缃?
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
        // 绔嬫柟浣撶储寮?
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

        // 1. 鍒涘缓骞朵笂浼?Cube Mesh
        ctx.glGenBuffers(1, &vbo);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), cubeVertices, GL_STATIC_DRAW);

        ctx.glGenBuffers(1, &ebo);
        ctx.glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        ctx.glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW);

        // 2. 鐢熸垚 100 涓疄渚嬬殑闅忔満鏁版嵁
        instances.resize(INSTANCE_COUNT);
        for (int i = 0; i < INSTANCE_COUNT; ++i) {
            // 鍦?-4 鍒?4 鐨勭┖闂村唴闅忔満鍒嗗竷
            instances[i].x = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;
            instances[i].y = ((float)rand() / RAND_MAX) * 8.0f - 4.0f;
            instances[i].z = ((float)rand() / RAND_MAX) * 8.0f - 6.0f; // 绋嶅井鎷夊紑涓€鐐规繁搴?

            // 闅忔満棰滆壊
            instances[i].r = (float)rand() / RAND_MAX;
            instances[i].g = (float)rand() / RAND_MAX;
            instances[i].b = (float)rand() / RAND_MAX;

            // 闅忔満鏃嬭浆鍙傛暟
            instances[i].spdX = ((float)rand() / RAND_MAX) * 2.0f + 0.5f; // 0.5 ~ 2.5
            instances[i].spdY = ((float)rand() / RAND_MAX) * 2.0f + 0.5f;
            instances[i].phase = ((float)rand() / RAND_MAX) * 3.14f * 2.0f;
        }

        // 3. 鍒涘缓骞朵笂浼?Instance Buffer
        ctx.glGenBuffers(1, &vbo_instance);
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo_instance);
        ctx.glBufferData(GL_ARRAY_BUFFER, instances.size() * sizeof(InstanceData), instances.data(), GL_STATIC_DRAW);

        // 4. 閰嶇疆 VAO 灞炴€?
        // ------------------------------------------------
        // 灞炴€?0: Local Position (vec3) -> 鏁版嵁婧? vbo
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo);
        ctx.glVertexAttribPointer(0, 3, GL_FLOAT, false, 3 * sizeof(float), (void*)0);
        ctx.glEnableVertexAttribArray(0);
        ctx.glVertexAttribDivisor(0, 0); // Divisor 0: 姣忎釜椤剁偣鏇存柊 (Mesh)

        // 灞炴€?1: Instance Offset (vec3) -> 鏁版嵁婧? vbo_instance
        ctx.glBindBuffer(GL_ARRAY_BUFFER, vbo_instance);
        // stride = sizeof(InstanceData), offset = 0
        ctx.glVertexAttribPointer(1, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, x));
        ctx.glEnableVertexAttribArray(1);
        ctx.glVertexAttribDivisor(1, 1); // Divisor 1: 姣忎釜瀹炰緥鏇存柊 (Instance Data)

        // 灞炴€?2: Instance Color (vec3) -> 鏁版嵁婧? vbo_instance
        // stride = sizeof(InstanceData), offset = 12 (3 floats)
        ctx.glVertexAttribPointer(2, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, r));
        ctx.glEnableVertexAttribArray(2);
        ctx.glVertexAttribDivisor(2, 1); // Divisor 1

        // 灞炴€?3: Rotation Params (vec3) -> 鏁版嵁婧? vbo_instance
        // stride = sizeof(InstanceData), offset = 24 (6 floats)
        ctx.glVertexAttribPointer(3, 3, GL_FLOAT, false, sizeof(InstanceData), (void*)offsetof(InstanceData, spdX));
        ctx.glEnableVertexAttribArray(3);
        ctx.glVertexAttribDivisor(3, 1); // Divisor 1
        
        // 缁戝畾 EBO
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
        // 鏇存柊鏃堕棿
        total_time += dt;
        shader.time = total_time;
    }

    void onRender(SoftRenderContext& ctx) override {
        // View Matrix (LookAt 0,0,0)
        // 杩欓噷绠€鍗曟ā鎷熶竴涓?LookAt锛屽疄闄呭簲浣跨敤瀹屾暣鐨?LookAt 鍑芥暟
        // 涓轰簡绠€鍖栵紝鎴戜滑鍙仛涓€涓钩绉?+ 绠€鍗曠殑閫忚
        const auto& vp = ctx.glGetViewport();
        float aspect = (float)vp.w / (float)vp.h;
        Mat4 proj = Mat4::Perspective(60.0f, aspect, 0.1f, 100.0f);
        
        // 绠€鍗曠殑 View: 鍚戝悗閫€ Z=12
        Mat4 view = Mat4::Translate(0.0f, 0.0f, -12.0f); 

        // 娣诲姞涓€鐐圭浉鏈虹殑鎽嗗姩
        view = view * Mat4::RotateZ(sin(total_time) * 5.0f); 

        shader.viewProj = proj * view;

        // 5. 缁樺埗璋冪敤
        // 浣跨敤 glDrawElementsInstanced 涓€娆℃€х粯鍒?100 涓?Cube
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

    // 绠€鍗曠殑鐩告満鍙傛暟
    float cam_yaw = 0.0f;
    float total_time = 0.0f;
};

// Register the test
static TestRegistrar registrar("Basic", "Instance", []() { return new InstanceTest(); });
