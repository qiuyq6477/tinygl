# tinygl — AI 代码助手使用说明（精简版）

此文件为 AI 编码代理（Copilot / 自动化助手）在本仓库中工作时的快速参考。目标是在不查看大量背景文档的情况下，立刻变得高效。

核心要点
- **项目类型**：C++20 软件光栅渲染（header-only 渲染器 + 最小 SDL 框架）。关键代码在 `vc/tinygl.h`（渲染器实现）和 `vc/`（运行时/框架）。
- **示例与入口**：所有可运行演示位于 `demos/`。这些 demo 通常通过 `#include "../vc/vc.cpp"` 拉入运行时（`vc.cpp` 提供 `main`），并通过 `vc_init` / `vc_render` / `vc_input` 三个函数与框架交互。
- **构建系统**：CMake。默认 `VC_PLATFORM=VC_SDL_PLATFORM`。构建命令（macOS）：
```
mkdir -p build && cd build
cmake ..
make
```
可通过 `cmake -DVC_PLATFORM=VC_TERM_PLATFORM ..` 切换平台；调试构建可指定 `-DCMAKE_BUILD_TYPE=Debug`。

重要设计与约定（请遵守）
- 渲染器为“部分 OpenGL 1.x”兼容的软光栅器，提供类似 `glGenBuffers` / `glBufferData` / `glVertexAttribPointer` / `glDrawElements` 的 API，位于 `vc/tinygl.h`。
- 颜色格式：内存中颜色为 0xAABBGGRR（Alpha = 高字节）。纹理数据请确保 Alpha=0xFF（参见 `demos/demo_cube.cpp` 中 checkerboard 的构建）。
- 矩阵约定：`Mat4` 使用列主（column-major）布局，透视矩阵实现约定见 `Mat4::Perspective`。调用方通常构造 `mvp = proj * model` 并通过 `SimdMat4::load(mvp)` 加载以加速顶点变换。
- SIMD 支持：有 `Simd4f` / `SimdMat4` 辅助类型（针对 ARM NEON 做了封装），请在需要时复用这些类型以匹配现有优化模式。
- 性能偏好：CMake 默认打开 `-O3 -march=native -ffast-math -funroll-loops`。仅在确实需要逐步调试时才创建 Debug 构建或移除某些优化。

常见代码模式（示例参考）
- 创建上下文：
  - `g_ctx = std::unique_ptr<SoftRenderContext>(new SoftRenderContext(W, H));`
- 纹理使用：
  - `g_ctx->glGenTextures(1, &tex); g_ctx->glBindTexture(GL_TEXTURE_2D, tex); g_ctx->glTexImage2D(..., pixels);`
  - 示例见 `demos/demo_cube.cpp`。
- 画法（shader 模板）：
  - demo 中以 POD/struct 作为 Shader，提供 `vertex(const Vec4* attribs, ShaderContext& out)` 与 `fragment(const ShaderContext&) -> uint32_t`，并将 Shader 对象传给 `g_ctx->glDrawElements(shader, ...)`。

调试与修改要点
- 日志：`tinygl.h` 定义了 `Logger`，但 `LOG_INFO` 宏被注释掉（性能考量）。如果需要更多运行时信息，可启用或临时修改该宏。
- 主入口位置：`vc/vc.cpp` 在不同 `VC_PLATFORM` 下包含平台特有 `main` 实现。Demos 通过包含 `vc.cpp` 来取得入口，因此修改 `vc.cpp` 会影响所有 demo。
- 平台切换：使用 `-DVC_PLATFORM=VC_TERM_PLATFORM` 或 `VC_WASM_PLATFORM` 在 CMake 配置阶段切换；不要手动在单个 demo 中同时包含多个入口。

文件与目录建议参考（快速导航）
- `vc/tinygl.h` — 渲染器与 API（阅读此文件以了解内部数据结构、Texture/Buffer 对象、SoftRenderContext 方法）。
- `vc/vc.cpp`, `vc/olive.cpp` — 运行时、窗口/终端适配与主循环。查看 `vc.cpp` 的 `VC_SDL_PLATFORM` 分支了解 SDL 渲染上传逻辑。
- `demos/*.cpp` — 使用示例；优先按这些文件的用法实现新功能或修复兼容性问题。
- `CMakeLists.txt` — 构建选项、默认平台与优化标志。

工作约束（给 AI 的明确规则）
- 不要在没有用户确认的情况下修改默认优化选项或移除 `-march=native`、`-ffast-math`，除非为调试且附带重现步骤。
- 不要改变全局的颜色/字节序约定；如果需要兼容性层，请在 `SoftRenderContext` 或纹理读写处增加明确定义的转换函数。
- 对于 API 兼容性改动（例如修改 `glDrawElements` 签名），请同时更新所有 `demos/` 示例并在 `vc/tinygl.h` 顶部写明兼容层说明。

如果有不清楚或遗漏的地方，请告诉我想要补充的细节（例如：你想让 AI 帮忙修改哪个 demo、添加新的渲染功能，或生成特定的测试案例）。
