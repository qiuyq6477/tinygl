# tinygl

A C++20 software rendering project featuring a minimal application framework and a header-only software rasterizer that emulates a subset of the OpenGL 1.x API.

This project was significantly refactored and enhanced from its original C-based version by the Gemini code assistant.

## Features

- **`tinygl.h` Renderer**: A header-only library implementing a subset of the OpenGL API with a programmable pipeline (using C++ lambdas).
- **Primitives**: `GL_TRIANGLES`, `GL_LINES`, `GL_POINTS`.
- **Advanced Texturing**: Mipmap generation, multiple filter modes, and support for `GL_RGB` and `GL_RGBA` source formats.
- **Modern C++**: Uses C++20.
- **Framework**: A minimal SDL-based application framework in `vc/`.
- **FPS Counter**: An FPS counter is overlayed on all demos for performance monitoring.

## Building

The project uses CMake to build all demos.

```bash
# 1. Create a build directory
mkdir -p build && cd build

# 2. Configure and build
cmake ..
make
```

## Running the Demos

Executables are created in the `build/` directory.

```bash
# From the build directory, run any of the available demos:
./demo_cube
./demo_draw_array
./demo_draw_elements
./demo_primitives
./triangle
./triangle3d
./dots3d
```

### Demos

- `demo_cube`: Renders an animated, textured cube.
- `demo_draw_array`: Renders a quad using `glDrawArrays`.
- `demo_draw_elements`: Renders a quad using `glDrawElements`.
- `demo_primitives`: Renders points and lines.
- `triangle`, `triangle3d`, `dots3d`: The original C demos, now ported to C++.

## Dependencies

- A C++20 compatible compiler (e.g., Clang, GCC).
- **SDL2**: Required for windowing and input.
  - on macOS: `brew install sdl2`
  - on Ubuntu: `sudo apt-get install libsdl2-dev`