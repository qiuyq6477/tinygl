# Gemini Project Analysis: tinygl

## Project Overview

This repository contains two distinct software rendering projects created for educational purposes.

### 1. C-based Graphics Framework (`vc/` + `demos/`)

This is a minimal, portable framework for creating simple 2D and 3D graphics demos.

*   **Architecture**: It follows an "stb-style" single-file library approach. Demo applications (e.g., `demos/triangle.c`) are self-contained C files that directly include the application framework (`#include "../vc/vc.c"`). The framework file `vc.c` then includes the actual rendering logic from `olive.c`.
*   **Framework (`vc/vc.c`)**: Provides a cross-platform abstraction layer for windowing (SDL, terminal) and the main application loop. It expects the demo to provide a `vc_render` function.
*   **Graphics Library (`vc/olive.c`)**: A dependency-free, pure C software rendering library. It handles 2D primitives and, most importantly, perspective-correct texture mapping for 3D triangles.
*   **Technology**: C, SDL (optional)

### 2. C++ Software Rasterizer (`main.cpp`)

This is a more advanced, standalone software rasterizer that emulates a subset of the legacy OpenGL 1.x fixed-function pipeline, but with a modern twist of using programmable shaders.

*   **Architecture**: A single, large C++ file (`main.cpp`) that implements a full 3D rendering pipeline from scratch. It is completely independent of the C framework.
*   **Features**:
    *   Programmable pipeline using C++ lambdas for vertex and fragment shaders.
    *   Sutherland-Hodgman algorithm for 3D clipping against the view frustum.
    *   Advanced texturing with automatic mipmap generation, multiple filter modes (including trilinear), and per-pixel Level of Detail (LOD) calculation.
*   **Output**: Renders the final image to a `.ppm` file, it does not open a window.
*   **Technology**: C++

## Building and Running

The primary build system is CMake, but it **only** builds the C-based demos. The C++ file `main.cpp` must be compiled manually.

### Building the C Demos

The `CMakeLists.txt` is configured to find all `.c` files in the `demos/` directory and build each one as a separate executable.

```bash
# 1. Create a build directory
mkdir -p build && cd build

# 2. Configure the project
cmake ..

# 3. Build all demos
make
```

After building, the executables will be located in the `build/` directory (e.g., `build/triangle`, `build/triangle3d`).

### Compiling and Running the C++ Rasterizer

The `main.cpp` file is not part of the CMake build. It can be compiled with a C++ compiler like `g++` or `clang++`.

```bash
# 1. Compile the file (using g++)
g++ main.cpp -o softrender -std=c++11

# 2. Run the executable
./softrender

# 3. Check the output
# This will create an 'output.ppm' file in the root directory.
# You can open it with an image viewer that supports the PPM format.
```

## Development Conventions

*   **C Framework**: The C code is written in a highly portable, C89-style. The architecture is intentionally simple, relying on file inclusion rather than a complex build system linkage, which is a common pattern for small, easy-to-distribute C libraries.
*   **C++ Rasterizer**: The C++ code is self-contained and demonstrates a full rendering pipeline. It makes use of C++11 features like lambdas for modern API design. The code is heavily commented to explain the concepts being implemented.
