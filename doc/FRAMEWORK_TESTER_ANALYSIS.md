# Framework Tester Logic Analysis

This document analyzes the implementation of `tests/framework_tester.cpp`, which serves as the runner for RHI (Render Hardware Interface) and Framework-level tests in the TinyGL project.

## 1. Architecture Overview

`FrameworkRunnerApp` inherits from the `Application` class. Its primary purpose is to validate the RHI abstraction layer by allowing the same test code to run on two different backends interchangeably:
1.  **Software Backend (`SoftDevice`)**: A pure CPU-based implementation of the graphics device.
2.  **OpenGL Backend (`GLDevice`)**: A wrapper around native OpenGL 3.3+.

## 2. Backend Switching Mechanism

The ability to switch backends at runtime is a key feature for debugging and verifying the software renderer against a reference (OpenGL).

### Logic (`doSetBackend`)
1.  **Teardown**: When switching, the app first shuts down the `UIRenderer` and destroys the current `m_device` (smart pointer reset).
2.  **Instantiation**:
    *   If **OpenGL** is selected: Instantiates `rhi::GLDevice`. Updates window title to `[Backend: OpenGL]`.
    *   If **Software** is selected: Instantiates `rhi::SoftDevice` passing the software `m_context`. Updates window title to `[Backend: Software]`.
3.  **Re-initialization**: The `UIRenderer` is re-initialized with the new device pointer. This ensures the UI is drawn using the active backend.
4.  **Test Reload**: If a test was running, it is destroyed and re-initialized with the new device to ensure resources (Buffers, Textures) are created on the correct device.

## 3. Screen Rendering & Display Logic

The `renderFrame()` function handles the display, but it diverges significantly based on the active backend.

### Scenario A: OpenGL Backend
The pipeline is standard hardware acceleration.
1.  **Execution**: `m_device->Submit(encoder.GetBuffer())` executes GL commands directly on the GPU driver.
2.  **Presentation**: `SDL_GL_SwapWindow(getWindow())` swaps the hardware front/back buffers.

### Scenario B: Software Backend (The "Blit" Trick)
Since `SoftDevice` writes to a CPU memory array (`std::vector<u32>` or similar), it cannot directly appear on the OS window (which is managed by SDL/OpenGL).
1.  **Execution**: `m_device->Submit(...)` runs the rasterizer on the CPU, filling `m_context->getColorBuffer()`.
2.  **Texture Upload**: The CPU buffer is uploaded to a dedicated OpenGL texture (`m_blitTexture`) using `glTexSubImage2D`.
3.  **Full-Screen Quad**: A special shader (`m_blitProgram`) draws a full-screen quad (2 triangles) covering the entire window.
4.  **Sampling**: This shader samples the uploaded texture, effectively "blitting" the software-rendered image onto the actual window.
5.  **Presentation**: Finally, `SDL_GL_SwapWindow` is called to show the quad.

## 4. Test Case Discovery & Filtering

### Registry
Tests are retrieved via `TestCaseRegistry::get().getRHITests()`. These are statically registered using the `TestRegistrar` helper, typically with the macro `TINYGL_TEST_GROUP` (which resolves to the file path, e.g., `framework/rhi/basic_triangle`).

### UI Logic (`onGUI`)
The UI organizes tests into a folder tree structure.
1.  **Fetching**: Iterates over all registered RHI tests.
2.  **Flattening**:
    *   It detects paths starting with `framework/`.
    *   It strips `framework/` and the *last* directory component.
    *   *Example*: `framework/rhi/basic_triangle/Test` becomes `rhi -> Test`.
    *   *Reason*: To reduce nested folders in the UI, making navigation faster (1 click vs 2 clicks).
3.  **Rendering**: Uses `MicroUI` (`mu_begin_treenode`, `mu_button`) to draw the folder tree. Clicking a button triggers `switchTest`.

## 5. Comparison: `framework_tester` vs `tinygl_tester`

| Feature | `tinygl_tester.cpp` | `framework_tester.cpp` |
| :--- | :--- | :--- |
| **Primary Goal** | Test the **Core Renderer** (`tinygl::tinygl`) implementation of the OpenGL API (glDraw, etc.). | Test the **RHI** (`IGraphicsDevice`) abstraction and Framework utilities. |
| **Test Interface** | `ITinyGLTestCase` | `ITestCase` |
| **Abstraction Level** | High-level (emulating OpenGL state machine). | Low-level (Buffers, Pipelines, Command Encoders). |
| **Backends** | Primarily runs on the internal `SoftwareRenderer`. | Explicitly toggles between `SoftDevice` and `GLDevice` for A/B comparison. |
| **Rendering** | Direct framebuffer manipulation / basic blit. | Dual-path: Native GL execution OR Software Blit. |
| **Use Case** | "Does my `glDrawArrays` implementation work?" | "Does my `SoftDevice` behave exactly like a real GPU?" |

## Summary
`framework_tester` is the validation harness for the engine's lower layers. Its ability to swap backends instantly makes it the primary tool for verifying that the Software Rasterizer correctly implements the RHI specification defined by the project.