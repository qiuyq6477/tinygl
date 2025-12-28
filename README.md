# tinygl

A C++20 software rendering project featuring a header-only software rasterizer that emulates a subset of the OpenGL 1.x API, combined with a minimal application framework.

This project implements a complete graphics pipeline (Vertex Processing -> Clipping -> Rasterization -> Fragment Processing) in software, with optimizations like SIMD (ARM NEON) and Tiled/Swizzled texture layouts.

## Features

- **`tinygl.h` Renderer**: A header-only library with a programmable pipeline (using C++ lambdas for Shaders).
- **Core Pipeline**: Support for `GL_TRIANGLES`, `GL_LINES`, and `GL_POINTS` with perspective-correct interpolation.
- **Advanced Texturing**: 
  - **4x4 Tiled/Swizzled Layout**: Optimized for cache locality during rasterization.
  - **Mipmapping**: Automatic mipmap generation (Box filter).
  - **Filtering**: Nearest, Bilinear, and Trilinear (Mipmap) filtering.
- **SIMD Optimized**: Math library with ARM NEON support.
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

## Building

The project uses CMake and Ninja (recommended) for builds.

```bash
# 1. Create a build directory
mkdir -p build && cd build

# 2. Configure (using Ninja)
cmake -G Ninja ..

# 3. Build
ninja
```

## Running

### Test Runner (GUI)
The primary entry point to explore features and visual tests:
```bash
./tests/test_runner
```

### Performance Benchmarks
Run automated performance tests using CTest:
```bash
cd build
ctest --verbose
```

Specific benchmarks can also be run directly:
```bash
./tests/perf/swizzle_compare/swizzle_compare_bench
./tests/perf/texture_perf/texture_perf_bench
```

## Dependencies

- A C++20 compatible compiler (Clang 12+, GCC 10+, MSVC 2019+).
- **SDL2**: Required for windowing and input.
  - macOS: `brew install sdl2`
  - Ubuntu: `sudo apt-get install libsdl2-dev`

## License
This project is for educational purposes.