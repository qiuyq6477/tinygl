# tinygl Context for Gemini

## Project Overview

**tinygl** is a C++20 software rendering project that implements a subset of the OpenGL 1.x API. It features a header-only software rasterizer and a minimal application framework.

* **Language:** C++20
* **Build System:** CMake
* **Dependencies:** SDL2
* **Core Feature:** Programmable pipeline via C++ lambdas, SIMD optimizations (ARM NEON/x86), and `glDraw*` emulation.

## Architecture

The project is divided into three main components:

1. **Core Renderer (`include/tinygl/core/tinygl.h`):**
    * A header-only, template-heavy software rasterizer.
    * Implements the graphics pipeline: Vertex Processing -> Clipping -> Rasterization -> Fragment Processing.
    * State management mimics OpenGL (VAOs, Textures, Buffers).

2. **Framework (`src/framework` & `src/core`):**
    * Compiled as a shared library: `tinygl_framework`.
    * Handles window creation, input management (via SDL2), and UI rendering (MicroUI).
    * Provides the `Application` base class.

3. **Tests & Demos (`tests/`):**
    * A single executable `test_runner` runs all registered test cases.
    * Test cases are static libraries that self-register using a global registry pattern.
    * Tests verify specific OpenGL features (e.g., `glDrawArrays`, Texturing, Mipmaps).

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
* **Solution:** Create exported wrapper functions in the framework (e.g., `tinygl::LoadImageRaw`) to provide controlled access to private dependencies.


