# TinyGL 资产烘焙与加载系统 (Asset Baking & Loading System)

TinyGL 实现了一套简单的资产管线，用于将通用的 3D 模型 (.obj, .fbx, .gltf) 和纹理 (.png, .jpg) 转换为引擎专有的二进制格式 (`.tmodel`, `.ttex`)。这种机制通常被称为“烘焙 (Cooking)”或“打包”。

## 1. 核心流程 (Workflow)

`AssetManager` 负责统一管理所有资产的加载。当你请求加载一个原始文件时，系统会自动检查是否需要进行格式转换。

### 自动烘焙机制
当调用 `AssetManager::Load<Prefab>("model.obj")` 或 `AssetManager::Load<TextureResource>("image.png")` 时：

1.  **路径解析**：系统将源文件扩展名替换为对应的二进制扩展名（例如 `image.png` -> `image.ttex`）。
2.  **过时检查 (Stale Check)**：
    *   如果二进制文件不存在，**需要烘焙**。
    *   如果二进制文件存在，但源文件 (`.obj`) 的最后修改时间晚于二进制文件，说明源文件已更新，**需要重新烘焙**。
    *   否则，直接使用现有的二进制文件。
3.  **烘焙 (Cooking)**：
    *   使用第三方库 (`Assimp` 用于模型, `stb_image` 用于纹理) 解析原始文件。
    *   提取引擎所需的数据（顶点、索引、材质属性、像素数据）。
    *   将数据序列化为紧凑的二进制格式并写入磁盘。
4.  **加载 (Loading)**：
    *   直接读取二进制文件到内存。
    *   创建 RHI (Render Hardware Interface) 资源（GPU Buffer, Texture）。
    *   构建引擎层面的资源对象 (`MeshResource`, `MaterialResource`, `Prefab`)。

## 2. 数据格式 (Data Formats)

所有二进制文件都包含一个标准头部，用于校验和版本控制。

### 2.1 通用头部
```cpp
struct BinaryAssetHeader {
    uint32_t magic;    // 文件标识符 (Magic Number)
    uint32_t version;  // 版本号 (ASSET_VERSION)
    uint64_t dataSize; // 数据载荷大小
};
```

### 2.2 纹理格式 (.ttex)
*   **Magic**: `0x58455454` ("TTEX")
*   **结构**:
    1.  `TextureMetadata` (继承自头部): 包含宽、高、MipLevels、Format。
    2.  **Raw Pixel Data**: 紧接着头部，存储未压缩的像素数据 (RGBA8)。

### 2.3 模型/预制体格式 (.tmodel)
*   **Magic**: `0x4C444D54` ("TMDL")
*   **结构**:
    1.  `ModelHeader` (继承自头部): 包含 `meshCount` (子网格数量)。
    2.  **SubMesh Block** (重复 `meshCount` 次):
        *   `SubMeshHeader`: 顶点数、索引数、纹理数量等。
        *   `MaterialDataPOD`: 材质属性 (Diffuse, Specular, Shininess, Opacity 等) 的纯数据结构。
        *   **Texture Paths**: 紧接着写入 `textureCount` 个字符串（先写入长度，再写入字符），分别对应 Diffuse, Specular, Normal, Ambient, Emissive, Opacity 槽位。
        *   **Vertex Data**: `vertexCount` 个 `VertexPacked` 结构 (Pos, Norm, UV)。
        *   **Index Data**: `indexCount` 个 `uint32_t` 索引。

## 3. 槽位映射 (Slot Mapping)

在模型烘焙阶段 (`AssetManager::CookModel`)，Assimp 的材质纹理被自动映射到 TinyGL 的标准槽位：

| 槽位 (Slot) | 用途 | Assimp 类型 | 备注 |
| :--- | :--- | :--- | :--- |
| **0** | Diffuse (漫反射) | `aiTextureType_DIFFUSE` | 基础颜色 |
| **1** | Specular (高光) | `aiTextureType_SPECULAR` | 高光强度/颜色 |
| **2** | Normal (法线) | `aiTextureType_NORMALS` | 优先使用法线贴图，无则尝试 `HEIGHT` |
| **3** | Ambient (环境光) | `aiTextureType_AMBIENT` | AO 或环境光遮蔽 |
| **4** | Emissive (自发光) | `aiTextureType_EMISSIVE` | 自发光颜色 |
| **5** | Opacity (不透明度) | `aiTextureType_OPACITY` | Alpha 通道或遮罩 |

## 4. 优缺点分析

### 优点 (Pros)
1.  **加载速度快 (Fast Loading)**：直接将二进制数据 `read` 进内存，无需解析文本 (如 OBJ) 或复杂的树状结构 (如 FBX/GLTF)。这对于大型场景至关重要。
2.  **去除运行时依赖 (Runtime Independence)**：理论上，发布的版本可以只包含 `.ttex` 和 `.tmodel` 文件，从而无需在最终构建中包含庞大的 `Assimp` 库（虽然目前 AssetManager 代码中仍包含它用于开发阶段的自动转换）。
3.  **数据归一化 (Normalization)**：无论源文件是 OBJ 还是 GLTF，进入引擎后都是统一的内存布局。
4.  **预处理 (Pre-processing)**：可以在烘焙阶段做耗时操作，如计算切线空间、网格优化、纹理压缩（尚未实现）等。

### 缺点 (Cons)
1.  **磁盘空间 (Disk Usage)**：`.ttex` 目前存储未压缩的 RGBA 数据，比 `.png` 或 `.jpg` 大得多。
2.  **开发阶段等待 (Wait Time)**：初次运行或修改原始资源后，需要等待烘焙过程。
3.  **灵活性限制**：二进制格式一旦改变 (`ASSET_VERSION` 变更)，所有旧缓存必须失效/删除，否则会导致加载错误或崩溃。目前尚未实现复杂的向后兼容性。

## 5. 常见问题

*   **修改了纹理但没生效？**
    *   检查 `.ttex` 文件的修改时间。系统只会比较源文件和缓存文件的时间戳。你可以手动删除 `assets` 目录下的 `.ttex` 文件强制重新烘焙。
*   **如何添加新的材质属性？**
    *   需要修改 `MaterialDataPOD` 结构体，增加 `ASSET_VERSION`，并更新 `CookModel` 和 `LoadPrefabBin` 函数。
