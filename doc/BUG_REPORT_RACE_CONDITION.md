# RHI 多线程竞态条件 Bug 分析报告

## 1. Bug 根本原因 (Root Cause Analysis)

**现象：**
在开启多线程 (`JobSystem`) 渲染时，画面出现“碎裂”、闪烁或完全错误的裁剪。

**核心原因：** **竞态条件 (Race Condition) 与 状态污染 (State Pollution)。**

*   **设计缺陷：** `SoftRenderContext` 被设计为一个**有状态 (Stateful)** 的对象（模仿 OpenGL 状态机）。它使用成员变量（如 `m_scissorBox`, `m_blendState`, `m_depthMask` 等）来存储当前的渲染配置。
*   **并发冲突：**
    1.  `SoftDevice::Submit` 启动了多个 Worker 线程并行处理 Tile（瓦片）。
    2.  所有线程共享**同一个** `SoftRenderContext` 实例。
    3.  **线程 A** 在处理 Tile A 时，为了设置裁剪区域，调用了 `ctx.glScissor(RectA)`，修改了共享的 `m_scissorBox`。
    4.  **线程 B** 同时在处理 Tile B，调用 `ctx.glScissor(RectB)`，覆盖了 `m_scissorBox`。
    5.  **线程 A** 继续执行光栅化 (`rasterizeTriangleTemplate`)，读取 `m_scissorBox` 进行像素裁剪。此时它读到的可能是 **线程 B** 刚刚写入的 `RectB`。
*   **结果：** 线程 A 使用了错误的裁剪矩形、错误的混合模式或深度状态，导致渲染结果混乱。

## 2. 当前实施的解决方案 (Implemented Solution)

我们采用的方案是 **无状态光栅化 (Stateless Rasterization) / 状态解耦**。

**具体步骤：**

1.  **状态封装 (Encapsulation)：**
    *   在 `SoftRenderContext` 中定义了 `RasterState` 结构体，将所有与光栅化相关的易变状态（Viewport, Scissor, Blend, Depth/Stencil, CullMode 等）打包在一起。
    *   将 `SoftRenderContext` 的零散成员变量替换为 `RasterState m_state`。

2.  **API 无状态化 (Stateless API)：**
    *   重构核心光栅化函数 (`rasterizeTriangleTemplate`, `rasterizeLineTemplate` 等)。
    *   新增重载版本，接受 `const RasterState& state` 作为参数。
    *   函数内部逻辑不再读取 `this->m_state`，而是严格读取传入的参数 `state`。

3.  **管线适配 (Pipeline Integration)：**
    *   在 `SoftPipeline::RasterizeTriangle` 中（这是多线程运行的入口），不再依赖全局状态。
    *   **栈上分配 (Stack Allocation)：** 创建一个局部的 `RasterState` 实例。
    *   **即时组装：** 根据 `PipelineDesc`（不可变管线描述）和当前 Tile 的参数（如裁剪区域）填充这个局部 `RasterState`。
    *   **传参：** 将这个线程私有的 `RasterState` 传递给光栅化函数。

**为什么有效：**
每个线程在栈上拥有自己独立的 `RasterState` 副本。无论多少个线程并发运行，它们读取的都是自己栈上的配置，互不干扰，彻底消除了竞态条件。

## 3. 其他可选方案分析 (Alternative Solutions Analysis)

虽然当前的方案是最优解之一，但还有其他思路：

### 方案 A：线程局部存储 (Thread-Local Storage - TLS)
*   **思路：** 将 `SoftRenderContext` 中的状态变量声明为 `thread_local`，或者为每个线程克隆一个 Context。
*   **优点：** 现有代码改动较小，光栅化函数的签名不需要修改。
*   **缺点：**
    *   **内存开销大：** 如果 Context 包含 Framebuffer 或大缓存，复制成本极高。
    *   **生命周期管理复杂：** `JobSystem` 的线程池是复用的，需要确保每帧开始前 TLS 状态被正确重置或同步，容易引入难以调试的 Bug。

### 方案 B：加锁 (Mutex Locking)
*   **思路：** 在 `rasterizeTriangle` 内部访问 `m_scissorBox` 等变量时加锁。
*   **优点：** 逻辑简单，保证数据正确性。
*   **缺点：** **性能毁灭者。** 光栅化是高频密集计算（每个三角形甚至每个像素），加锁会导致所有线程串行化，不仅失去了多线程的意义，性能甚至可能不如单线程（因为有锁的开销）。

### 方案 C：命令桶/渲染包 (Command Buckets / Render Packets)
*   **思路：** 在 `Submit` 阶段不直接调用 `Rasterize`，而是生成包含所有状态信息的“微指令”包 (`RenderPacket`)，存入队列。Worker 线程只负责“执行”这些完全自包含的指令包。
*   **优点：** 架构最清晰，完全解耦，也是现代 GPU 驱动（Vulkan/DX12）的工作方式。我们目前的 `SoftPipeline` 其实已经稍微通过 `PipelineDesc` 靠近了这个方向。
*   **缺点：** **重构成本巨大。** 需要彻底重写 `SoftRenderContext` 的底层逻辑，不仅要传递 State，还需要将 Framebuffer 的写入逻辑也变成原子操作或分块独立（目前通过 Tiler 已经实现了 Framebuffer 的分块独立，所以现在的方案是方案 C 的轻量级变种）。

## 总结

我们选择的方案（**显式传递状态结构体**）是在**性能**（无锁、无大内存拷贝）和**重构成本**之间的最佳平衡点。它让 `SoftRenderContext` 保持了 OpenGL 风格的对外接口（方便用户使用），但在内部执行层实现了函数式编程的**纯函数 (Pure Function)** 特性，完美适配多线程渲染。
