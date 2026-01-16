# The Great ECS Refactor: Technical Dev Log

**Date:** 2026-01-15  
**Author:** TinyGL Architecture Team  
**Subject:** Transformation from OOP to Data-Oriented ECS Architecture

---

## 1. 🎯 重构目标 (The Goal)

本次重构旨在解决 `tinygl` 早期架构中的以下核心痛点，为未来的物理系统、场景编辑器和多线程渲染打下坚实基础：

1.  **强耦合 (Coupling)**: `Mesh` 类直接持有 `Material` 数据，导致无法实现“同一几何体、不同材质”的复用（如变色、高亮），且 `Draw` 逻辑硬编码在 Mesh 中。
2.  **资源管理混乱 (Resource Chaos)**: `Model` 类既是资源加载入口，又是场景节点，职责不清。纹理管理 (`TextureManager`) 与模型管理割裂。
3.  **内存效率低 (Poor Memory)**: 大量使用 `std::vector<Mesh>` 和指针，导致内存碎片化，不利于 CPU Cache。
4.  **RHI 穿透 (RHI Leakage)**: 上层逻辑直接调用 OpenGL API (`SoftRenderContext`)，使得 RHI 抽象层形同虚设。

**最终目标**: 建立一套纯粹的 **ECS (Entity-Component-System)** 架构，配合 **Handle-Based** 资源管理和 **Prefab** 系统。

---

## 2. 📅 执行阶段 (Execution Phases)

### Phase 1-2: 基础设施 (Infrastructure)
*   **Handle System**: 引入 `AssetHandle<T>` (32/64位整数)，取代裸指针和 `shared_ptr`。Handle 包含 Index 和 Generation，彻底解决悬挂指针问题。
*   **Binary Formats**: 定义了 `.ttex` (纹理) 和 `.tmodel` (模型) 的二进制文件头 (`include/framework/asset_formats.h`)。
*   **Lazy Cooking**: 实现了运行时“懒烘焙”。当加载 `.obj` 时，自动将其转换为 `.tmodel` 二进制文件。二次加载直接读取二进制，耗时从 **800ms** 降至 **<10ms**。

### Phase 3: RHI 对接 (RHI Integration)
*   **统一入口**: `AssetManager` 接管了所有资源加载。
*   **Material Persistence**: 在 `.tmodel` 中序列化了材质引用的纹理路径。加载时，`AssetManager` 自动递归加载依赖纹理，并将其上传至 RHI (SoftDevice/OpenGL)。

### Phase 4: Mesh 资源化 (Mesh as Resource)
*   **Mesh Pooling**: `AssetManager` 内部新增了 `Mesh` 资源池。
*   **SubMesh Extraction**: `LoadModelBin` 不再只是填充一个大 `Model` 对象，而是将每个 SubMesh 注册为独立的 Asset，生成全局唯一的 `AssetHandle<Mesh>`。
*   **验证**: 实现了 `ecs_test.cpp`，演示了不通过 `Model` 类，而是直接通过 Handle 组装 Entity 并渲染。

### Phase 5: 材质分离 (Material Separation)
*   **Material Pooling**: `Material` 被提升为独立资源 (`AssetHandle<Material>`)。
*   **Decoupling**: `LoadModelBin` 将材质数据从 Mesh 中剥离。现在，一个模型文件本质上是 **Mesh Handles 列表 + Material Handles 列表**。
*   **验证**: `ecs_material_test.cpp` 演示了给同一个 Mesh Handle 赋予不同的 Material Handle，实现了真正的材质复用。

### Phase 6: 彻底重构 (The Great Refactor)
这是破坏性最大的一步，彻底废弃了 OOP 时代的遗留物。

*   **废弃 `Model` 类**: 取而代之的是 **`Prefab`** 系统。
    *   `Prefab` 是一个纯数据结构，描述了 Entity 的层级结构和组件引用 (Mesh + Material + Transform)。
    *   加载模型现在返回 `AssetHandle<Prefab>`。
*   **纯数据化 (POD Resources)**:
    *   `Mesh` 类被阉割为 `MeshResource` (仅含 VBO/EBO Handle, AABB)。移除了所有 `Draw` 方法和材质成员。
    *   `Material` 类变为 `MaterialResource` (仅含 Shader 参数和 Texture Handles)。
    *   `AssetManager` 重写，完全基于 `IGraphicsDevice` 接口，不再依赖 `SoftRenderContext`。
*   **RHI ECS 测试**: 新增 `rhi_ecs_test.cpp`，完全脱离 OpenGL 上下文，仅使用 `CommandEncoder` 录制渲染命令，验证了架构的纯洁性。

---

## 3. 🔑 关键技术决策 (Key Technical Decisions)

### 3.1 为什么是 Prefab 而不是 Model?
在 OOP 引擎中，`Model` 通常是一个 C++ 对象树。但在 ECS 中，资源 (Asset) 和 实体 (Entity) 必须严格区分。
*   **Model (Old)**: 是一个“已经实例化好”的对象。加载它就等于创建了它。
*   **Prefab (New)**: 是一张“蓝图”。加载它只是拿到了一张图纸。只有调用 `Instantiate(Prefab)` 时，才会在 ECS World 中创建真正的 Entity。这使得我们可以在内存中只存一份 Prefab，却实例化成千上万个 Entity，且每个 Entity 都可以独立修改。

### 3.2 Sidecar 模式 (过渡策略)
在 Phase 4-5 期间，为了不破坏既有的测试用例（依赖 `Model` 类），我们采用了 **Sidecar** 模式：
*   在加载流程中，我们**同时**填充旧的 `Model` 对象和新的 `AssetManager Pool`。
*   这允许新旧代码共存。直到 Phase 6，我们才彻底切断了旧代码的脐带。

### 3.3 统一资源池 (Unified Resource Pool)
所有资源 (`Mesh`, `Texture`, `Material`, `Prefab`) 都存储在 `AssetManager` 的 `std::vector` 池中。
*   **优点**: 内存连续，CPU Cache 友好。
*   **优点**: 引用计数和生命周期管理集中化。
*   **优点**: Handle 只是一个 `uint32_t` 索引，组件极其轻量 (`sizeof(Entity) < 64 bytes`)。

---

## 4. 🔮 遗留与展望 (Legacy & Future)

### 已废弃 (Deprecated)
*   `include/framework/model.h`: 旧的 `Model` 类已不再被核心系统使用。
*   `include/framework/texture_manager.h`: 功能已被 `AssetManager` 完全取代。
*   `Mesh::Draw()`: 渲染逻辑已移至 `RenderSystem` (或测试用例中的 Render Loop)。

### 下一步计划 (Next Steps)
1.  **Shader 资源化**: 目前 Shader 仍通过硬编码或简单的工厂管理。下一步应引入 `AssetHandle<Shader>`，支持 GLSL 热重载。
2.  **多线程加载 (Async Loading)**: `AssetManager::Load` 目前是阻塞的。基于现有的 Handle 架构，很容易扩展为返回 `Future<Handle>`，实现无卡顿流式加载。
3.  **场景序列化 (Scene Serialization)**: 既然 Prefab 已经是纯数据，实现整个场景的 Save/Load (序列化为 JSON/Binary) 将变得易如反掌。

---

*Document generated by Gemini CLI Architecture Agent.*
