# tinygl Project Analysis & Capabilities

## 🚀 Project Overview
**tinygl** is a modern C++20 software rasterizer designed to emulate a subset of the OpenGL 3.3+ API. It serves as a high-performance educational platform for understanding graphics pipelines, SIMD optimizations, and driver-level architecture (RHI).

## ✅ Implemented Features

### 1. Core Rendering Pipeline
- **Software Rasterizer**: Barycentric triangle rasterization with tiled optimizations.
- **Programmable Shaders**: Vertex and Fragment shaders implemented as C++20 lambdas/structs, allowing compiler inlining and full debugging capability.
- **Math & SIMD**: Custom math library (`Vec4`, `Mat4`) with SIMD optimizations (SSE/NEON) for heavy lifting (Matrix multiplication, Vertex transformation).
- **Clipping**: Full Sutherland-Hodgman clipping in homogeneous clip space.
- **Depth & Stencil**: Early-Z rejection, Depth testing/writing, and Stencil operations.

### 2. Render Hardware Interface (RHI)
- **Backend Abstraction**: `IGraphicsDevice` interface decoupling the frontend from the backend (supporting `SoftDevice` and `GLDevice`).
- **DSA-Forward Design**: Implements Direct State Access (DSA) concepts (e.g., `glCreateVertexArrays`, `glNamedBufferStorage`) to modernize internal state management.
- **Stateless Render Passes**: explicit `BeginRenderPass`/`EndRenderPass` architecture for robust state management and future Vulkan-like optimization.
- **Resource Management**: Slot-based binding for Buffers and Textures, ensuring O(1) resource access during draw calls.

### 3. OpenGL API Subset
- **Draw Commands**: `glDrawArrays`, `glDrawElements`, and Instanced variants.
- **Vertex Specification**: VAO (`glGenVertexArrays`), VBO, EBO, and dynamic attribute formatting.
- **Texturing**: 
  - `GL_TEXTURE_2D` support.
  - Bilinear filtering (`GL_LINEAR`), Nearest neighbor (`GL_NEAREST`).
  - Mipmap generation and sampling (`GL_LINEAR_MIPMAP_LINEAR`).
  - Texture Wrapping (`REPEAT`, `CLAMP_TO_EDGE`).
- **State**: Blending (`glBlendFunc`), Scissor Test (`glScissor`), Culling (`glCullFace`), Polygon Mode (Fill/Line/Point).

### 4. Framework & Tools
- **Windowing**: SDL2 integration for cross-platform window creation and input handling.
- **UI**: Immediate mode GUI integration via **MicroUI** for debugging and parameter tuning.
- **Asset Loading**: Integrated `stb_image` for texture loading and basic OBJ model parsing.
- **Unified Asset System**:
  - **Handle-Based**: Uses `AssetHandle<T>` for safe, zero-overhead resource referencing in ECS.
  - **Asset Cooking**: Automatic runtime conversion of `.obj` and `.png` to optimized binary formats (`.tmodel`, `.ttex`) for sub-50ms loading times.
  - **Lazy Cooking**: Checks timestamps and re-cooks only when source files change.
  - **Material Persistence**: Automatically serializes and restores texture references (Diffuse/Specular/Normal) within binary model files.
  - **RHI Integration**: Seamlessly loads resources into the rendering backend (SoftGL/OpenGL) via the `AssetManager`.
  - **ECS Ready**: Decoupled `Mesh` management allowing entities to reference individual submeshes via `AssetHandle<Mesh>`, enabling lightweight instantiation and composition.
  - **Material Separation**: `Material` data is now a first-class Asset (`AssetHandle<Material>`), allowing runtime swapping of materials on the same geometry without duplication.
  - **Prefab System**: Replaced monolithic `Model` class with `Prefab` assets, enabling hierarchical instantiation of entities with pure data components.
  - **Pure ECS Architecture**: Rendering pipeline is now fully data-driven, decoupling Geometry (`MeshResource`), Appearance (`MaterialResource`), and Transform.
  - **RHI ECS Test**: Validated the full stack (Asset -> ECS -> RHI) with `rhi_ecs_test`. Implemented `ECSShader` with `BindUniforms` and `BindResources` to correctly map slot-based data (Diffuse/Specular textures) from `SoftPipeline` to shader variables, proving the RHI abstraction works for complex data flows.
- **Test Suite**: A comprehensive test runner (`tinygl_tester`) verifying features from basic geometry to lighting and blending.

---

## 🔮 Future Roadmap (Missing Features)

To evolve **tinygl** into a more capable engine, the following features are recommended:

### 🟥 High Priority
1.  **Framebuffer Objects (FBOs)**:
    -   *Why*: Essential for off-screen rendering.
    -   *Unlocks*: Shadow Mapping, Post-processing effects (Bloom, Blur), Dynamic reflections.
2.  **Cube Maps (`GL_TEXTURE_CUBE_MAP`)**:
    -   *Why*: Required for Skyboxes and Environment Mapping (Reflections).
    -   *Current State*: Only 2D textures are supported.

### 🟨 Medium Priority
1.  **GLSL-to-C++ Transpiler or Parser**:
    -   *Why*: Currently shaders are C++ code. A parser would allow loading standard `.vert`/`.frag` files.
2.  **Geometry Shaders**:
    -   *Why*: Allows generating geometry on GPU (e.g., for particle systems or wireframe overlay).
3.  **Shadow Mapping**:
    -   *Prerequisite*: Needs FBO and Depth Texture support.

### 🟦 Low Priority / "Cool" Extras
1.  **Compute Shaders**: Emulating `glDispatchCompute` for GPGPU tasks on CPU (e.g., Physics simulation).
2.  **MSAA (Multi-Sample Anti-Aliasing)**: Improve visual quality of edges.

---

## 💡 Cool Applications You Can Build (Now & Future)

### 1. Retro FPS Engine (Quake-style) 🔫
*   **Feasibility**: **High** (Current features are sufficient).
*   **Tech Stack**: 
    -   Use `glDrawElements` for level geometry (BSP/Map).
    -   Use `glBlendFunc` for transparency.
    -   Implement "Lightmaps" using Multi-texturing (supported via Shader slots).
    -   *Challenge*: Optimizing occlusion culling for large levels.

### 2. 3D Model Viewer with PBR 🎨
*   **Feasibility**: **Medium** (Needs Cubemaps for IBL).
*   **Tech Stack**:
    -   Implement Physically Based Rendering (PBR) shader in C++.
    -   Use `MicroUI` for material editing (Roughness/Metalness sliders).
    -   *Missing*: Needs Cube Maps for realistic environment lighting.

### 3. Voxel World (Minecraft Clone) ⛏️
*   **Feasibility**: **High**.
*   **Tech Stack**:
    -   Heavy use of `glDrawArrays` with dynamic chunk mesh generation.
    -   Face culling is already implemented.
    -   Simple 2D textures for blocks.
    -   *Cool Factor*: Implement real-time day/night cycle using shader uniforms.

### 4. Software Raytracer / Path Tracer Hybrid 🕯️
*   **Feasibility**: **Medium/Hard**.
*   **Tech Stack**:
    -   Instead of `glDraw*`, write a "Compute" pass that fills a texture buffer pixel-by-pixel.
    -   Draw a full-screen quad displaying this texture.
    -   Great for learning ray-triangle intersection math without GPU constraints.

### 5. Educational "How GPU Works" Visualizer 🎓
*   **Feasibility**: **High**.
*   **Tech Stack**:
    -   Slow down the rasterizer intentionally!
    -   Visualize the "Triangle Setup" and "Pixel Traversal" steps step-by-step on screen using the UI.
    -   Show the depth buffer as a grayscale heatmap.
