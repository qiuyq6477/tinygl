# tinygl

A C++20 software rendering project featuring a header-only software rasterizer that emulates a subset of the OpenGL 1.x API, combined with a minimal application framework.

This project implements a complete graphics pipeline (Vertex Processing -> Clipping -> Rasterization -> Fragment Processing) in software, with optimizations like SIMD (ARM NEON) and Tiled/Swizzled texture layouts.

## Building and Running

### Prerequisites

You need a **C++20 compliant compiler** and **CMake 3.15+**.

#### Windows (MSYS2 / MinGW)
1. Install [MSYS2](https://www.msys2.org/).
2. Open the **MSYS2 UCRT64** terminal and install dependencies:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja
   pacman -S mingw-w64-ucrt-x86_64-SDL2 mingw-w64-ucrt-x86_64-assimp
   ```

#### macOS
1. Install [Homebrew](https://brew.sh/).
2. Install dependencies:
   ```bash
   brew install cmake ninja sdl2 assimp
   ```

#### Linux (Ubuntu/Debian)
1. Install dependencies via apt:
   ```bash
   sudo apt-get update
   sudo apt-get install build-essential cmake ninja-build libsdl2-dev libassimp-dev
   ```

### Compilation

```bash
# 1. Create a build directory
mkdir -p build && cd build

# 2. Configure (using Ninja is recommended for speed)
cmake -G Ninja ..

# 3. Build all targets
ninja
```

### Running Tests

The primary executable is `test_runner`.

**Windows Note:** Ensure `SDL2.dll`, `libassimp-*.dll`, and other runtime dependencies are in the same directory as the executable or in your system PATH. The build system attempts to copy internal DLLs automatically, but you may need to copy MinGW runtime DLLs manually if running outside the MSYS2 shell.

```bash
# Run the test runner (opens a GUI window)
./tests/test_runner
```

## Features

- **`tinygl.h` Renderer**: A header-only library with a programmable pipeline (using C++ lambdas for Shaders).
- **Core Pipeline**: Support for `GL_TRIANGLES`, `GL_LINES`, `GL_POINTS` and other primitives with perspective-correct interpolation.
- **Instancing**: Support for `glDrawArraysInstanced`, `glDrawElementsInstanced` and `glVertexAttribDivisor`.
- **Render State**: Support for `glPolygonMode` (Fill/Line/Point), `glCullFace`, `glDepthFunc`, etc.
- **Advanced Texturing**: 
  - **4x4 Tiled/Swizzled Layout**: Optimized for cache locality during rasterization.
  - **Mipmapping**: Automatic mipmap generation (Box filter).
  - **Filtering**: Nearest, Bilinear, and Trilinear (Mipmap) filtering.
- **SIMD Optimized**: Math library with hand-written **ARM NEON** and **x86 SSE** backends.
- **Modern C++20**: Clean, template-heavy architecture.
- **UI Integration**: Integrated with [MicroUI](https://github.com/rxi/microui) for on-screen controls.
- **Comprehensive Testing**: Integrated test runner and performance benchmarks.

## Project Structure

- `include/tinygl/`: Core header files.
  - `tinygl.h`: The main software rasterizer.
  - `math_simd.h`: SIMD-accelerated math primitives.
- `src/`: Framework and core implementations.
- `tests/`: 
  - `test_runner`: A GUI-based test explorer.
  - `perf/`: Performance benchmarks (Swizzling, Texture sampling).

## License
This project is for educational purposes.