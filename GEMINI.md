# tinygl Context for Gemini

## Project Overview

**tinygl** is a high-performance C++20 software rendering project aiming to implement a subset of the **OpenGL 3.3+ API**. It features a template-driven core rendering pipeline and a minimal application framework, designed for educational purposes and to demonstrate modern C++ techniques in graphics programming.

* **Goal:** Implement a functional subset of OpenGL 3.3 (and potentially higher versions).
* **Core:** A software rasterizer with a template-driven pipeline for maximum performance.
* **Key Features:** 
    * **Programmable Pipeline:** Vertex and Fragment shaders implemented via C++ lambdas, with a header-resident core for inlining.
    * **High Performance:** SIMD optimizations (ARM NEON & x86 SSE) and tiled rasterization.
    * **API Emulation:** Supports `glDraw*`, VAOs, VBOs, and Texture units.
* **Framework:** Built on SDL2 for platform abstraction and MicroUI for immediate mode GUI.

## Architecture

The project follows a layered architecture divided into three main components:

1. **Core Renderer (`include/tinygl/core/` & `src/core/`):**
    * **Type:** Hybrid (Template Pipeline + Compiled State Management).
    * **Design:** High-performance template-driven pipeline defined in `tinygl.h` to allow compiler inlining of shader stages.
    * **Pipeline:** Vertex Processing -> Primitive Assembly & Clipping (Sutherland-Hodgman) -> Rasterization (Barycentric) -> Fragment Processing (Early-Z, Texturing, Blending).
    * **State Management:** Implemented in `.cpp` files, mimicking OpenGL state machine (VAOs, Textures, Buffers, Render State).

2. **Framework (`src/framework/`):**
    * **Type:** Shared Library (`tinygl_framework`).
    * **Responsibility:** Provides the runtime environment for the renderer.
    * **Features:** Window creation, Input event handling (via SDL2), Resource Loading (Textures/Models), and Debug UI rendering (MicroUI).
    * **Interface:** Exposes the `Application` base class for users to inherit from.

3. **Tests & Demos (`tests/`):**
    * **Type:** Executable (`test_runner`) + Static Libraries linking against `tinygl_framework`.
    * **Structure:** Each test case is compiled as a static library that self-registers into a global registry.
    * **Purpose:** Verification of implemented OpenGL features (e.g., `glDrawArrays`, Texturing, Mipmaps, Instancing, Lighting).
    * **Runner:** A unified GUI application to browse and run different graphical tests.

## Building and Running

### Prerequisites
* C++20 Compiler (Clang, GCC, or MSVC)
* CMake 3.10+
* SDL2 (Development libraries)
    * macOS: `brew install sdl2`
    * Ubuntu: `sudo apt-get install libsdl2-dev`

### Build Commands

```bash
# 1. Create a build directory
mkdir -p build && cd build

# 2. Configure (using Ninja)
cmake -G Ninja ..

# 3. Build
ninja
```

### Running Tests

The primary executable is `test_runner` located in `tests/`:

```bash
# Run the test runner (it will likely open a window with a menu to select tests)
./tests/test_runner
```

## Development Conventions

### Git Commit Guidelines
Strictly adhere to **Conventional Commits**:
* Format: `<type>(<scope>): <subject>`
* Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`.
* Example: `feat(renderer): add support for glScissor`

### Adding a New Test

1. **Directory Structure:** Create a new directory in `tests/<category>/<test_name>`.
2. **Build Configuration:** Add a `CMakeLists.txt`:
    ```cmake
    add_tinygl_test(test_name_lib test_source.cpp)
    ```
3. **Implementation:** Implement the test class inheriting from `ITestCase`.
4. **Registration:** Register the test using the `TestRegistrar` helper at the end of your cpp file:
    ```cpp
    static TestRegistrar registry("Category", "TestName", []() { return new MyTest(); });
    ```

#### Camera Integration (Mandatory)
All 3D test cases must include a camera to allow user navigation.
1. **Include:** `#include <tinygl/camera.h>`
2. **Member:** Add `tinygl::Camera camera;` to your class.
3. **Event:** Forward events in `onEvent`: `camera.ProcessEvent(e);`
4. **Update:** Update camera physics in `onUpdate`: `camera.Update(dt);`
5. **Render:** Use `camera.GetViewMatrix()` and `camera.GetProjectionMatrix()` to compute MVP matrices.

#### Lifecycle Methods & Responsibilities
* `init(ctx)`: **Setup.** Create resources like VAOs, VBOs, Textures, and Shaders. Called once when the test is selected.
* `destroy(ctx)`: **Cleanup.** Delete OpenGL resources. Called when switching tests or closing.
* `onEvent(e)`: **Input.** Handle SDL events (key presses, mouse movement). **Pass events to `camera` here.**
* `onUpdate(dt)`: **Logic.** Update simulation state, rotation angles, camera position, and animations. **Do NOT put logic or state updates in `onRender`.**
* `onRender(ctx)`: **Draw.** Execute rendering commands (`glClear`, `glUseProgram`, `glDraw*`). Compute matrices here but rely on state updated in `onUpdate`.
* `onGui(ctx, rect)`: **UI.** Render debug UI (sliders, buttons) using MicroUI. Use this to tweak parameters in real-time.

### Coding Style
* **Standard:** Modern C++20.
* **Formatting:** Follow existing indentation (looks like 4 spaces).
* **Naming:** 
    * Files: `snake_case`.
    * Functions: `mixedCamelCase` or `snake_case` (follow local context).
    * Variables: **NEVER** use `m_` prefix for any variables, including class members. Prefer `camelCase` or `snake_case` based on existing patterns in the file.

## Key Files & Directories

* `include/tinygl/core/tinygl.h`: **The Brain.** The software rasterizer implementation.
* `src/framework/application.cpp`: Main application loop and SDL integration.
* `tests/test_runner.cpp`: Entry point for the test suite.
* `API_STATUS.md`: Tracks implemented vs. missing OpenGL features.
* `CMakeLists.txt`: Root build configuration.

## Cross-Platform Development Lessons

### SIMD Portability
* **Problem:** Hardcoding ARM NEON (`arm_neon.h`) causes build failure on x86 Windows.
* **Solution:** Use abstraction classes (`Simd4f`) and platform macros (`__ARM_NEON`, `__SSE__`). Always provide an x86 SSE fallback for SIMD code.
* **Header:** Include `<immintrin.h>` for x86 and `<arm_neon.h>` for ARM.

### DLL Symbol Export (Windows)
* **Problem:** Windows DLLs hide all symbols by default, causing `undefined reference` errors during linking of tests.
* **Solution:** Use an export macro (`TINYGL_API`).
    ```cpp
    #if defined(_WIN32)
        #ifdef TINYGL_EXPORTS
            #define TINYGL_API __declspec(dllexport)
        #else
            #define TINYGL_API __declspec(dllimport)
        #endif
    #else
        #define TINYGL_API __attribute__((visibility("default")))
    #endif
    ```
* **Rule:** Every public class, struct, and function in `tinygl_framework` must be marked with `TINYGL_API`.

### C++ Syntax Compatibility
* **Problem:** C99 array designated initializers (e.g., `[index] = value`) are not standard C++ and fail on strict compilers (MSVC/MinGW GCC).
* **Solution:**
    * Use C++ Lambda-based initialization for complex static maps.
    * Move large data tables (like UI atlases) to `.c` files and compile them as C code.

### Encapsulation & DLL Boundaries
* **Problem:** Test cases directy calling third-party functions (like `stbi_load`) hidden inside the framework DLL.
* **Solution:** Create exported wrapper functions in the framework to provide controlled access to private dependencies.

### RHI Handle vs. GL ID Safety
* **Problem:** Passing RHI Handle IDs (from ResourcePool) directly to Core functions expecting GL Object IDs. This caused a critical bug where a VBO (Handle 1) was bound as an EBO (GL ID 1), leading to `fetchAttribute OOB` as float data was misinterpreted as integer indices.
* **Solution:** Always resolve RHI Handles to their underlying GL IDs in the `Submit` layer using `m_buffers.Get(handle)->glId` before passing them to the rendering backend.


## Gemini Added Memories
- Implemented Slot-Based Material System (Scheme B) for tinygl: Decoupled Mesh::Draw from specific texture types using std::vector<GLuint> slots. Slot 0 = Diffuse, Slot 1 = Specular. Abstracted Texture class for RAII resource management.
- Implemented TextureManager for tinygl using weak_ptr for automatic cache management. Model class now uses TextureManager to load textures and maintains a keepAlive list to ensure textures persist during model lifetime. This prevents duplicate texture loading across models.
- Enhanced Material System: Expanded `tinygl::Material` to support more properties (Ambient, Specular, Emissive, Opacity, Shininess) and flags (AlphaTest, DoubleSided). `Mesh::Draw` now automatically binds up to 6 texture slots (Diffuse, Specular, Normal, Ambient, Emissive, Opacity) and passes material data to compatible shaders using C++20 `if constexpr (requires)`. Added `glIsEnabled` to `SoftRenderContext`.
- Implemented Shader Pass Architecture (Scheme 1).
    - Added `RenderState` for global uniform data.
    - Added `IShaderPass` and `ShaderPass<T>` to decouple Mesh from specific Shader types.
    - Refactored `Material` to hold `std::shared_ptr<IShaderPass>`.
    - Updated `Mesh::Draw` and `Model::Draw` to use `IShaderPass` instead of templates.
    - Updated `ModelLoadingTest` to demonstrate assigning `ShaderPass` to materials at runtime.
- Implemented Data-Driven RHI (Render Hardware Interface):
    - **Architecture:** Adopted "Command Bucket" pattern (Scheme B) for high-performance and future Vulkan compatibility.
    - **Core Interfaces:** Defined `IGraphicsDevice` (Abstract), `SoftDevice` (Backend), `CommandEncoder` (Frontend), and `RenderCommand` (POD).
    - **Material Refactor:** Extracted POD `MaterialData` from `Material` class to be "Uniform Buffer Ready" (std140 layout compatible). Updated `ShaderPass` to support direct injection of `MaterialData`.
    - **SoftRender Adapter:** Created `ISoftPipeline` bridge to support runtime switching of template-based shaders. Implemented `RegisterShaderFactory` mechanism to link runtime handles to compile-time types.
    - **Dynamic Vertex Layout:** Implemented `VertexInputState` in `PipelineDesc`. Removed hardcoded attribute setup in favor of dynamic `glVertexAttribPointer` configuration in `SoftPipeline::SetupInputLayout`.
    - **Features:** Handle-based resource management (Buffers, Textures, Pipelines), linear allocator for transient uniform data, and slot-based uniform injection.
    - **Validation:** Added `tests/rhi/basic_triangle` to verify the full RHI pipeline.
- Implemented **DSA-Forward Rendering Pipeline** (Direct State Access) for performance and architecture modernization:
    - **Concept:** Decoupled Vertex Attribute Format from Buffer Binding to eliminate redundant API calls ("API Thrashing"). Introduced "Draw-time Baking" to pre-calculate pointer addresses, enabling O(1) attribute fetching.
    - **Core Changes:**
        - Refactored `VertexArrayObject` to split `VertexAttribFormat` and `VertexBufferBinding`.
        - Implemented `ResolvedAttribute` structure to store baked pointers and metadata.
        - Added `prepareDraw()` helper to bake attributes when `isDirty` is set.
        - Refactored `fetchAttribute` to use baked `ResolvedAttribute` directly, removing indirect lookups.
    - **API Additions:** Added `glCreateVertexArrays`, `glCreateBuffers`, `glNamedBufferStorage`, `glVertexArrayAttribFormat`, `glVertexArrayAttribBinding`, `glVertexArrayVertexBuffer`, `glVertexArrayElementBuffer`, and `glEnableVertexArrayAttrib`.
    - **Compatibility:** Updated legacy `glVertexAttribPointer` to map to the new DSA internal structure transparently.
    - **RHI Update:** Refactored `SoftPipeline` to create and configure an internal VAO using DSA functions at construction time. `Draw` calls now simply bind the VAO and update the VBO binding via `glVertexArrayVertexBuffer`, eliminating the attribute setup loop.
    - **Safety:** Implemented robust bounds checking in `fetchAttribute` using `limitPointer`. 
    - **Instancing Fix:** Added `divisor` support to `ResolvedAttribute` and `fetchAttribute` to restore instanced rendering functionality.
- **RHI Optimization:**
    - **Resource Pooling (Scheme A):** Replaced `std::map` with vector-based `ResourcePool` in `SoftDevice` for O(1) resource access and better cache locality.
    - **State Deduplication (Scheme B):** Implemented state tracking in `SoftDevice::Submit` to skip redundant pipeline, buffer, and texture binding commands.
- **RHI Fix:** Resolved RHI Handle to GL ID mismatch in `SoftDevice::Submit`. Fixed a bug where internal handle indices were incorrectly passed as OpenGL object IDs, which caused corrupted index buffer bindings and `fetchAttribute OOB` errors.
