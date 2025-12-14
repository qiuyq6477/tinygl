#include "../vc/tinygl.h" 
#include "../vc/vc.cpp"

#define DEMO_WIDTH 800
#define DEMO_HEIGHT 600

std::unique_ptr<SoftRenderContext> g_ctx;
GLuint g_vao, g_vbo, g_ebo;
GLuint g_tex;
float g_rotationAngle = 0.0f;
static uint32_t g_cubeIndices[36]; 

struct CubeShader {
    TextureObject* texture = nullptr;
    SimdMat4 mvp; 
    Vec4 colorMod = {1.0f, 1.0f, 1.0f, 1.0f};

    inline Vec4 vertex(const Vec4* attribs, ShaderContext& outCtx) {
        outCtx.varyings[0] = attribs[2]; // UV
        outCtx.varyings[1] = attribs[1]; // Color

        float posArr[4] = {attribs[0].x, attribs[0].y, attribs[0].z, 1.0f};
        Simd4f pos = Simd4f::load(posArr);
        Simd4f res = mvp.transformPoint(pos);
        
        float outArr[4];
        res.store(outArr);
        return Vec4(outArr[0], outArr[1], outArr[2], outArr[3]);
    }

    inline uint32_t fragment(const ShaderContext& inCtx) {
        uint32_t texColor = 0xFFFFFFFF;
        if (texture) {
            texColor = texture->sampleNearestFast(inCtx.varyings[0].x, inCtx.varyings[0].y);
        }

        uint32_t r = texColor & 0xFF;
        uint32_t g = (texColor >> 8) & 0xFF;
        uint32_t b = (texColor >> 16) & 0xFF;

        float vr = inCtx.varyings[1].x * colorMod.x;
        float vg = inCtx.varyings[1].y * colorMod.y;
        float vb = inCtx.varyings[1].z * colorMod.z;

        uint32_t fr = (uint32_t)(r * vr);
        uint32_t fg = (uint32_t)(g * vg);
        uint32_t fb = (uint32_t)(b * vb);

        return (255 << 24) | (fb << 16) | (fg << 8) | fr;
    }
};

struct Vertex {
    float pos[3];
    uint8_t color[3];
    uint8_t padding; 
    float uv[2];
};

void vc_init(void) {
    g_ctx = std::unique_ptr<SoftRenderContext>(new SoftRenderContext(DEMO_WIDTH, DEMO_HEIGHT));
    
    // Create Texture
    g_ctx->glGenTextures(1, &g_tex); 
    g_ctx->glActiveTexture(GL_TEXTURE0); 
    g_ctx->glBindTexture(GL_TEXTURE_2D, g_tex);
    std::vector<uint32_t> pixels(256 * 256);
    for(int i=0; i<256*256; i++) {
        int x = i % 256;
        int y = i / 256;
        bool isWhite = ((x / 32) + (y / 32)) % 2 != 0;
        pixels[i] = isWhite ? 0xFFFFFFFF : 0xFF000000; 
    }
    g_ctx->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 256, 256, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // Upload Vertex Data
    Vertex vertices[] = {
        // Front
        {{-1,-1, 1}, {255,255,255}, 0, {0,0}}, {{ 1,-1, 1}, {255,255,255}, 0, {1,0}},
        {{ 1, 1, 1}, {255,255,255}, 0, {1,1}}, {{-1, 1, 1}, {255,255,255}, 0, {0,1}},
        // Back
        {{ 1,-1,-1}, {255,255,255}, 0, {0,0}}, {{-1,-1,-1}, {255,255,255}, 0, {1,0}},
        {{-1, 1,-1}, {255,255,255}, 0, {1,1}}, {{ 1, 1,-1}, {255,255,255}, 0, {0,1}},
        // Left
        {{-1,-1,-1}, {255,255,255}, 0, {0,0}}, {{-1,-1, 1}, {255,255,255}, 0, {1,0}},
        {{-1, 1, 1}, {255,255,255}, 0, {1,1}}, {{-1, 1,-1}, {255,255,255}, 0, {0,1}},
        // Right
        {{ 1,-1, 1}, {255,255,255}, 0, {0,0}}, {{ 1,-1,-1}, {255,255,255}, 0, {1,0}},
        {{ 1, 1,-1}, {255,255,255}, 0, {1,1}}, {{ 1, 1, 1}, {255,255,255}, 0, {0,1}},
        // Top
        {{-1, 1, 1}, {255,255,255}, 0, {0,0}}, {{ 1, 1, 1}, {255,255,255}, 0, {1,0}},
        {{ 1, 1,-1}, {255,255,255}, 0, {1,1}}, {{-1, 1,-1}, {255,255,255}, 0, {0,1}},
        // Bottom
        {{-1,-1,-1}, {255,255,255}, 0, {0,0}}, {{ 1,-1,-1}, {255,255,255}, 0, {1,0}},
        {{ 1,-1, 1}, {255,255,255}, 0, {1,1}}, {{-1,-1, 1}, {255,255,255}, 0, {0,1}}
    };

    uint32_t *indices = g_cubeIndices;
    for(int i=0; i<6; i++) {
        uint32_t base = i*4;
        indices[i*6+0]=base; indices[i*6+1]=base+1; indices[i*6+2]=base+2;
        indices[i*6+3]=base+2; indices[i*6+4]=base+3; indices[i*6+5]=base;
    }

    g_ctx->glGenVertexArrays(1, &g_vao); 
    g_ctx->glBindVertexArray(g_vao);
    g_ctx->glGenBuffers(1, &g_vbo); 
    g_ctx->glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    g_ctx->glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    g_ctx->glGenBuffers(1, &g_ebo); 
    g_ctx->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ebo);
    g_ctx->glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(g_cubeIndices), g_cubeIndices, GL_STATIC_DRAW);

    GLsizei stride = sizeof(Vertex);
    g_ctx->glVertexAttribPointer(0, 3, GL_FLOAT, false, stride, (void*)offsetof(Vertex, pos));
    g_ctx->glEnableVertexAttribArray(0);
    g_ctx->glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, true, stride, (void*)offsetof(Vertex, color));
    g_ctx->glEnableVertexAttribArray(1);
    g_ctx->glVertexAttribPointer(2, 2, GL_FLOAT, false, stride, (void*)offsetof(Vertex, uv));
    g_ctx->glEnableVertexAttribArray(2);
}

void vc_input(SDL_Event *event) {
    if (event->type == SDL_KEYDOWN && event->key.keysym.sym == SDLK_SPACE) g_rotationAngle = 0.0f;
}

Olivec_Canvas vc_render(float dt, void* pixels) {
    if (!g_ctx) return olivec_canvas(nullptr, DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
    g_ctx->setExternalBuffer((uint32_t*)pixels);
    
    // Clear entire screen
    g_ctx->glViewport(0, 0, DEMO_WIDTH, DEMO_HEIGHT);
    g_ctx->glClearColor(0.2f, 0.2f, 0.2f, 1.0f); 
    g_ctx->glClear(BufferType::COLOR | BufferType::DEPTH);

    g_rotationAngle += 45.0f * dt;

    int halfW = DEMO_WIDTH / 2;

    // 1. Draw Left View
    {
        g_ctx->glViewport(0, 0, halfW, DEMO_HEIGHT);
        
        CubeShader shader;
        shader.texture = g_ctx->getTextureObject(g_tex); 
        shader.colorMod = {1.0f, 1.0f, 0.5f, 1.0f}; // Yellowish tint

        // Rotate Y
        Mat4 model = Mat4::Translate(0, 0, -3.0f) * Mat4::RotateY(g_rotationAngle);
        // Correct aspect ratio for half width
        Mat4 proj = Mat4::Perspective(90.0f, (float)halfW/DEMO_HEIGHT, 0.1f, 100.0f);
        Mat4 mvp = proj * Mat4::Identity() * model;
        shader.mvp.load(mvp);

        g_ctx->glDrawElements(shader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0);
    }

    // 2. Draw Right View
    {
        g_ctx->glViewport(halfW, 0, halfW, DEMO_HEIGHT);
        
        CubeShader shader;
        shader.texture = g_ctx->getTextureObject(g_tex); 
        shader.colorMod = {0.5f, 0.5f, 1.0f, 1.0f}; // Blueish tint

        // Rotate X
        Mat4 model = Mat4::Translate(0, 0, -3.0f) * Mat4::RotateX(g_rotationAngle);
        Mat4 proj = Mat4::Perspective(90.0f, (float)halfW/DEMO_HEIGHT, 0.1f, 100.0f);
        Mat4 mvp = proj * Mat4::Identity() * model;
        shader.mvp.load(mvp);

        g_ctx->glDrawElements(shader, GL_TRIANGLES, 36, GL_UNSIGNED_INT, (void *)0);
    }

    return olivec_canvas(g_ctx->getColorBuffer(), DEMO_WIDTH, DEMO_HEIGHT, DEMO_WIDTH);
}
