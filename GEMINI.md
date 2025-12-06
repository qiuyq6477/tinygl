# Gemini Project Analysis: tinygl Refactor

## Project Overview

This project is a C++20 software rendering framework. It consists of a minimal application framework and a header-only software rasterizer library that emulates a subset of the OpenGL 1.x API.

*   **Architecture**: The project is structured into a `vc` framework, a `tinygl.h` rendering library, and a collection of `demos`.
    *   **`vc/vc.cpp`**: A minimal, C++-based application framework that provides an SDL window, an event loop, and callback hooks (`vc_init`, `vc_input`, `vc_render`) that the demos must implement.
    *   **`vc/tinygl.h`**: A header-only C++ software rasterizer that implements a subset of the OpenGL API. It is included by the demos to perform 3D rendering.
    *   **`demos/`**: A collection of C++ demo applications that showcase the features of the `tinygl.h` library.
*   **Technology**: C++20, SDL

## Build System

The project uses CMake to discover and build all `.cpp` files in the `demos/` directory as separate executables.

```bash
# 1. Create a build directory
mkdir -p build && cd build

# 2. Configure the project
cmake ..

# 3. Build all demos
make
```
Executables for each demo will be located in the `build/` directory.

## `tinygl.h` Feature Set

The renderer has been significantly enhanced to support a wider range of OpenGL features, including:
*   **Primitives**: `GL_TRIANGLES`, `GL_LINES`, `GL_POINTS`.
*   **Vertex Data**: `GL_FLOAT` and normalized `GL_UNSIGNED_BYTE` attributes.
*   **Index Data**: `GL_UNSIGNED_INT`, `GL_UNSIGNED_SHORT`, `GL_UNSIGNED_BYTE` for `glDrawElements`.
*   **Textures**: `glTexImage2D` supports `GL_RGB` and `GL_RGBA` source data formats, and mipmap generation.
*   **Matrix Uniforms**: `glUniformMatrix4fv` supports matrix transposition.

## Gemini's Work

This project was significantly refactored and enhanced by the Gemini assistant. The work included:
1.  **Migration to C++**: The original C framework and demos were migrated to C++20.
2.  **Library Refactoring**: The C++ rasterizer logic was extracted from a single `main.cpp` file into the header-only `tinygl.h` library.
3.  **Feature Implementation**: Many previously unimplemented OpenGL API parameters were formally implemented, resolving warnings and adding significant new features (e.g., multiple primitive types, index buffer types, texture formats).
4.  **New Demos**: Several new demos were created to test and showcase the new features, including `demo_cube`, `demo_draw_array`, `demo_draw_elements`, and `demo_primitives`.
5.  **Bug Fixing & Performance**:
    *   Fixed numerous runtime errors and build issues.
    *   Implemented `glClearColor` to fix a screen clearing bug.
    *   Added an FPS counter to the framework for performance monitoring.
    *   Addressed a significant performance degradation issue by re-architecting the rendering pipeline to avoid memory allocations in the main loop.
6.  **Documentation**: Updated the `README.md` and this document to reflect the current state of the project.