# tinygl Context for Gemini

## Project Overview

**tinygl** is a C++20 software rendering project that implements a subset of the OpenGL 1.x API. It features a header-only software rasterizer and a minimal application framework.

*   **Language:** C++20
*   **Build System:** CMake
*   **Dependencies:** SDL2
*   **Core Feature:** Programmable pipeline via C++ lambdas, SIMD optimizations (ARM NEON/x86), and `glDraw*` emulation.

## Architecture

The project is divided into three main components:

1.  **Core Renderer (`include/tinygl/tinygl.h`):**
    *   A header-only, template-heavy software rasterizer.
    *   Implements the graphics pipeline: Vertex Processing -> Clipping -> Rasterization -> Fragment Processing.
    *   State management mimics OpenGL (VAOs, Textures, Buffers).

2.  **Framework (`src/framework` & `src/core`):**
    *   Compiled as a shared library: `tinygl_framework`.
    *   Handles window creation, input management (via SDL2), and UI rendering (MicroUI).
    *   Provides the `Application` base class.

3.  **Tests & Demos (`tests/`):**
    *   A single executable `test_runner` runs all registered test cases.
    *   Test cases are static libraries that self-register using a global registry pattern.
    *   Tests verify specific OpenGL features (e.g., `glDrawArrays`, Texturing, Mipmaps).

## Building and Running

### Prerequisites
*   C++20 Compiler (Clang, GCC, or MSVC)
*   CMake 3.10+
*   SDL2 (Development libraries)
    *   macOS: `brew install sdl2`
    *   Ubuntu: `sudo apt-get install libsdl2-dev`

### Build Commands

```bash
mkdir -p build
cd build
cmake ..
make -j$(nproc)
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
*   Format: `<type>(<scope>): <subject>`
*   Types: `feat`, `fix`, `docs`, `style`, `refactor`, `perf`, `test`, `build`, `ci`, `chore`, `revert`.
*   Example: `feat(renderer): add support for glScissor`

### Adding a New Test
1.  Create a new directory in `tests/<category>/<test_name>`.
2.  Add a `CMakeLists.txt`:
    ```cmake
    add_tinygl_test(test_name_lib test_source.cpp)
    ```
3.  Implement the test class inheriting from `ITestCase`.
4.  Register the test using the macro (typically found in `test_registry.h` or similar pattern).

### Coding Style
*   **Standard:** Modern C++20.
*   **Formatting:** Follow existing indentation (looks like 4 spaces).
*   **Naming:** snake_case for files, mixedCamelCase or snake_case for functions (check local context).

## Key Files & Directories

*   `include/tinygl/tinygl.h`: **The Brain.** The software rasterizer implementation.
*   `src/framework/application.cpp`: Main application loop and SDL integration.
*   `tests/test_runner.cpp`: Entry point for the test suite.
*   `API_STATUS.md`: Tracks implemented vs. missing OpenGL features.
*   `CMakeLists.txt`: Root build configuration.
