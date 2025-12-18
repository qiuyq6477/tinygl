# tinygl Project Context

## Project Overview

**tinygl** is a C++20 software rendering project that implements a subset of the OpenGL 1.x API. It features a header-only software rasterizer and a minimal application framework. The project is designed for educational purposes, demonstrating how a graphics pipeline works by implementing it in software.

### Key Technologies
*   **Language:** C++20
*   **Build System:** CMake
*   **Dependencies:** SDL2 (for windowing and input in the framework)
*   **Core Library:** Header-only design (`tinygl.h`), emulating OpenGL functions like `glDrawArrays`, `glDrawElements`, `glVertex`, etc.

### Architecture

The project is structured into three main components:

1.  **Core Library (`include/tinygl/`)**:
    *   The heart of the renderer.
    *   **`tinygl.h`**: The main entry point.
    *   **`core/`**: Contains internal modules like buffers, geometry, texture handling, and the software context (`SoftRenderContext`).
    *   **`math_simd.h`**: SIMD optimizations (likely for vector/matrix operations).

2.  **Framework (`src/framework/`)**:
    *   Provides a platform abstraction layer.
    *   **`platform.cpp`**: Implements the main loop, window creation (via SDL2), and buffer swapping. It handles the `main()` entry point and calls the user-defined `vc_render` function.

3.  **Demos (`demos/`)**:
    *   Example applications demonstrating the usage of `tinygl`.
    *   Each demo is built as a separate executable.
    *   Examples: `demo_cube` (textured cube), `triangle` (basic rendering), `dots3d`.

## Building and Running

### Prerequisites
*   C++20 compatible compiler (Clang, GCC, or MSVC)
*   SDL2 development libraries
    *   macOS: `brew install sdl2`
    *   Ubuntu: `sudo apt-get install libsdl2-dev`

### Build Instructions

The project uses a standard CMake workflow:

```bash
# 1. Create a build directory
mkdir -p build && cd build

# 2. Configure the project
cmake ..

# 3. Build all targets
make
```

### Running Demos

After building, executables are located in the `build/` directory.

```bash
cd build
./demo_cube
# or
./triangle
```

*Note: Asset files (e.g., textures) are automatically copied to `build/demos/assets` during configuration, preserving the relative paths expected by the demos.*

## Development Conventions

*   **Code Style:** Modern C++ (C++20 features are used).
*   **Structure:**
    *   Library headers go in `include/tinygl/`.
    *   Implementation for the framework goes in `src/`.
    *   Demo sources go in `demos/`.
*   **Platform Abstraction:** The framework uses `VC_PLATFORM` macros to select the backend (e.g., `VC_SDL_PLATFORM`).
*   **Rendering Loop:** Demos typically implement a `vc_render` or similar callback that updates a framebuffer, which the framework then displays.
*   **Performance:** The build defaults to `Release` mode with optimizations (`-O3`, `-march=native`, `-ffast-math`) enabled for non-MSVC compilers.

## Important Files

*   `include/tinygl/tinygl.h`: Main header for the software renderer.
*   `src/framework/platform.cpp`: Application entry point and SDL2 integration.
*   `demos/demo_cube.cpp`: A comprehensive example showing 3D rendering and texturing.
*   `CMakeLists.txt`: Root build configuration.
