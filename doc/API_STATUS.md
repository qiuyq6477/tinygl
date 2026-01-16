# tinygl API 实现状态

本文档列举 tinygl 中已实现和未实现的 OpenGL API，与 OpenGL 3.3 对标。

---

## 📊 概览

- **已实现**：核心绘制管线、基础纹理、顶点处理、基础状态管理、实例化绘制
- **部分实现**：纹理参数、Mipmap 生成
- **未实现**：着色器编译、DSA (Direct State Access)、帧缓冲对象 (FBO)

---

## ✅ 已实现的 API

### 缓冲区管理 (Buffer Management)

| API | 状态 | 说明 |
|-----|------|------|
| `glGenBuffers` | ✅ | 创建缓冲区对象 |
| `glDeleteBuffers` | ✅  | 删除缓冲区 |
| `glBindBuffer` | ✅ | 绑定缓冲区到目标 |
| `glBufferData` | ✅ | 上传缓冲区数据 |
| `glBufferSubData` | ✅ | 部分更新缓冲区 |
| `glCopyBufferSubData` | ✅ | 缓冲区间复制数据 |
| `glMapBuffer` / `glUnmapBuffer` | ✅ | 内存映射缓冲区 |
| `glGetBufferSubData` | ❌ | 读取缓冲区数据 |

**实现位置**：`vc/tinygl.h` 中 `SoftRenderContext` 类

### 顶点数组对象 (VAO)

| API | 状态 | 说明 |
|-----|------|------|
| `glGenVertexArrays` | ✅ | 创建 VAO |
| `glDeleteVertexArrays` | ✅ | 删除 VAO |
| `glBindVertexArray` | ✅ | 绑定 VAO |
| `glVertexAttribPointer` | ✅ | 设置顶点属性指针 |
| `glEnableVertexAttribArray` | ✅ | 启用顶点属性 |
| `glDisableVertexAttribArray` | ❌ | 禁用顶点属性 |
| `glVertexAttribIPointer` | ❌ | 整数属性指针 |
| `glVertexAttribDivisor` | ✅ | 实例化属性分频 |

**实现位置**：`vc/tinygl.h` 中 `VertexArrayObject` 和相关方法

### 渲染状态管理 (Render State)

| API | 状态 | 说明 |
|-----|------|------|
| `glEnable` / `glDisable` | ✅ | 启用/禁用功能（深度测试、背面剔除等） |
| `glDepthFunc` | ✅ | 设置深度测试函数 (GL_LESS, GL_ALWAYS 等) |
| `glCullFace` / `glFrontFace` | ✅ | 背面剔除控制 (GL_BACK/GL_FRONT, CCW/CW) |
| `glPolygonMode` | ✅ | 多边形填充模式 (FILL, LINE, POINT) |
| `glDepthMask` | ✅ | 控制深度写入 |
| `glColorMask` | ❌ | 颜色写入掩码 |
| `glBlendFunc` | ✅ | 颜色混合 |

### 视口与裁剪 (Viewport & Scissor)

| API | 状态 | 说明 |
|-----|------|------|
| `glViewport` | ✅ | 设置视口 |
| `glScissor` | ✅ | 裁剪矩形 |

### 纹理管理 (Texture Management)

| API | 状态 | 说明 |
|-----|------|------|
| `glGenTextures` | ✅ | 创建纹理对象 |
| `glDeleteTextures` | ❌ | 删除纹理 |
| `glBindTexture` | ✅ | 绑定纹理 |
| `glTexImage2D` | ✅ | 上传纹理数据 |
| `glTexSubImage2D` | ❌ | 部分更新纹理 |
| `glTexParameteri` | ✅ | 设置纹理参数（采样模式、包装模式） |
| `glTexParameterf` | ❌ | 浮点纹理参数 |
| `glGenerateMipmap` | ✅ | 生成 Mipmap（在 glTexImage2D 中隐式调用） |
| `glGetTexImage` | ❌ | 读取纹理数据 |
| `glCompressedTexImage2D` | ❌ | 加载压缩纹理 |
| `glTexStorage2D` | ❌ | 不可变纹理存储 |
| `glCopyTexImage2D` | ❌ | 从帧缓冲复制纹理 |

**支持的纹理功能**：
- ✅ 2D 纹理 (`GL_TEXTURE_2D`)
- ✅ Mipmap 支持（Box Filter 下采样）
- ✅ 纹理过滤：`GL_NEAREST`, `GL_LINEAR`, `GL_*_MIPMAP_*`
- ✅ 纹理包装：`GL_REPEAT`, `GL_CLAMP_TO_EDGE`, `GL_MIRRORED_REPEAT`
- ❌ 纹理数组、立方体贴图、3D 纹理

**实现位置**：`vc/tinygl.h` 中 `TextureObject` 结构

### 活动纹理单元 (Texture Units)

| API | 状态 | 说明 |
|-----|------|------|
| `glActiveTexture` | ✅ | 激活纹理单元（0-15） |
| `glBindSampler` | ❌ | 绑定采样器对象 |
| `glBindTextureUnit` (DSA) | ❌ | 直接绑定到纹理单元 |

**实现位置**：`vc/tinygl.h` 中 `glActiveTexture` 方法

### 绘制命令 (Draw Commands)

| API | 状态 | 说明 |
|-----|------|------|
| `glDrawElements` | ✅ | 使用索引绘制（模板实现） |
| `glDrawArrays` | ✅ | 直接绘制（模板实现） |
| `glDrawArraysInstanced` | ✅ | 实例化绘制 |
| `glDrawElementsInstanced` | ✅ | 索引实例化绘制 |
| `glDrawElementsBaseVertex` | ❌ | 带基础顶点偏移的索引绘制 |
| `glDrawElementsIndirect` | ❌ | 间接绘制 |
| `glDrawArraysInstancedBaseInstance` | ❌ | 带基础实例的实例化 |

**支持的图元类型**：
- ✅ `GL_TRIANGLES`
- ✅ `GL_LINES`
- ✅ `GL_POINTS`
- ✅ `GL_TRIANGLE_STRIP`
- ✅ `GL_TRIANGLE_FAN`
- ✅ `GL_LINE_STRIP`
- ✅ `GL_LINE_LOOP`

**支持的索引类型**：
- ✅ `GL_UNSIGNED_INT`
- ✅ `GL_UNSIGNED_SHORT`
- ✅ `GL_UNSIGNED_BYTE`

**实现位置**：`vc/tinygl.h` 中 `glDrawElements`, `glDrawArrays` 模板方法

### 着色器与程序（当前为模板方式）

| API | 状态 | 说明 |
|-----|------|------|
| `glCreateShader` | ❌ | 创建着色器对象 |
| `glShaderSource` | ❌ | 加载着色器源码 |
| `glCompileShader` | ❌ | 编译着色器 |
| `glCreateProgram` | ❌ | 创建程序对象 |
| `glAttachShader` | ❌ | 附加着色器到程序 |
| `glLinkProgram` | ❌ | 链接程序 |
| `glUseProgram` | ⚠️ | 隐式通过模板 shader 参数实现 |
| `glGetUniformLocation` | ❌ | 获取 Uniform 位置 |
| `glUniform*` | ⚠️ | 通过模板 shader 的成员变量实现 |
| `glGetAttribLocation` | ❌ | 获取属性位置 |

**当前着色器架构**：
- ✅ C++ 模板 Shader 类（Vertex + Fragment）
- ✅ POD 结构体模式（见 `demos/demo_cube.cpp`）
- ✅ Vertex Shader：`Vec4 vertex(const Vec4* attribs, ShaderContext& out)`
- ✅ Fragment Shader：`uint32_t fragment(const ShaderContext& in)`

**实现位置**：`vc/tinygl.h` 中 `drawElementsTemplate`, `glDrawArrays` 方法和 demo 示例

### 清除与缓冲区操作

| API | 状态 | 说明 |
|-----|------|------|
| `glClear` | ✅ | 清除颜色和深度缓冲区 |
| `glClearColor` | ✅ | 设置清除颜色 |
| `glClearDepth` | ❌ | 设置清除深度值 |
| `glFlush` / `glFinish` | ❌ | 命令刷新与同步 |

**支持清除标志**：
- ✅ `GL_COLOR_BUFFER_BIT`
- ✅ `GL_DEPTH_BUFFER_BIT`
- ❌ `GL_STENCIL_BUFFER_BIT`（已声明但未实现）

**实现位置**：`vc/tinygl.h` 中 `glClear`, `glClearColor` 方法

### 矩阵与数学库

| 类型 | 状态 | 说明 |
|------|------|------|
| `Vec2` | ✅ | 2D 向量 |
| `Vec4` | ✅ | 4D 向量（齐次坐标） |
| `Mat4` | ✅ | 4x4 矩阵（列主序） |
| `Simd4f` | ✅ | SIMD 4 浮点数（ARM NEON） |
| `SimdMat4` | ✅ | SIMD 矩阵（列存储） |

**矩阵操作**：
- ✅ `Mat4::Identity`
- ✅ `Mat4::Translate`
- ✅ `Mat4::Scale`
- ✅ `Mat4::RotateX` / `RotateY` / `RotateZ`
- ✅ `Mat4::Perspective`
- ✅ `Mat4::Transpose`
- ✅ `Mat4::operator*` (矩阵乘法)
- ✅ 矩阵-向量乘法
- ❌ `Mat4::Inverse` (矩阵求逆)
- ❌ `Mat4::Orthogonal` (正交投影)
- ❌ `Mat4::LookAt` (观察矩阵)

**实现位置**：`vc/tinygl.h` 中 `Mat4` 和 SIMD 结构

### 光栅化与管线

| 功能 | 状态 | 说明 |
|------|------|------|
| 顶点处理 | ✅ | Vertex Shader 调用 |
| 裁剪 | ✅ | Sutherland-Hodgman 算法（6 个视锥体平面） |
| 透视除法 | ✅ | w 分量除法 |
| 视口变换 | ✅ | NDC 到屏幕空间转换 |
| 三角形光栅化 | ✅ | 边界函数法（扫描线优化） |
| 深度测试 | ✅ | `z < depthBuffer[pix]` 测试 (可配置函数) |
| 背面剔除 | ✅ | 可配置 `glCullFace` |
| 线条光栅化 | ✅ | Bresenham 算法 |
| 点光栅化 | ✅ | 单像素绘制 |
| 属性插值 | ✅ | 透视校正的重心坐标插值 |
| 深度线性化 | ✅ | 1/w 透视修正 |
| 多边形模式 | ✅ | 支持 FILL, LINE, POINT |

**未实现的光栅化特性**：
- ❌ 多重采样抗锯齿 (MSAA)
- ✅ 颜色混合 (`glBlendFunc`)
- ❌ 颜色掩码 (`glColorMask`)
- ✅ 模板测试
- ❌ 线宽控制 (`glLineWidth`)
- ❌ 点大小控制 (`glPointSize`)

**实现位置**：`vc/tinygl.h` 中 `rasterizeTriangleTemplate`, `rasterizeLine`, `rasterizePoint` 方法

---

## ❌ 未实现的 API

### 模板测试 (Stencil Test)

| API | 状态 | 说明 |
|-----|--------|------|
| `glStencilFunc` | ✅ | 模板函数 |
| `glStencilOp` | ✅ | 模板操作 |
| `glStencilMask` | ✅ | 模板掩码 |
| `glClearStencil` | ✅ | 清除模板 |

### 着色器编译与链接

| API | 优先级 | 说明 |
|-----|--------|------|
| `glCreateShader` | 🔴 高 | 创建着色器对象 |
| `glShaderSource` | 🔴 高 | 加载着色器源码 |
| `glCompileShader` | 🔴 高 | 编译着色器 |
| `glDeleteShader` | 🔴 高 | 删除着色器 |
| `glCreateProgram` | 🔴 高 | 创建程序对象 |
| `glAttachShader` / `glDetachShader` | 🔴 高 | 附加/分离着色器 |
| `glLinkProgram` | 🔴 高 | 链接程序 |
| `glDeleteProgram` | 🔴 高 | 删除程序 |
| `glUseProgram` | 🔴 高 | 使用程序（当前模板化） |
| `glGetProgramInfoLog` | 🔴 高 | 链接错误日志 |
| `glGetShaderInfoLog` | 🔴 高 | 编译错误日志 |
| `glValidateProgram` | 🟠 中 | 程序验证 |

### Uniform 管理

| API | 优先级 | 说明 |
|-----|--------|------|
| `glGetUniformLocation` | 🔴 高 | 获取 Uniform 位置 |
| `glUniform1f` / `glUniform1i` | 🔴 高 | 设置标量 Uniform |
| `glUniform2f` / `glUniform2i` | 🔴 高 | 设置向量 Uniform |
| `glUniform3f` / `glUniform3i` | 🔴 高 | 设置向量 Uniform |
| `glUniform4f` / `glUniform4i` | 🔴 高 | 设置向量 Uniform |
| `glUniformMatrix2fv` / `glUniformMatrix3fv` | 🔴 高 | 设置矩阵 Uniform |
| `glUniformMatrix4fv` | 🔴 高 | 设置 4x4 矩阵 Uniform |
| `glUniform*v` | 🔴 高 | 数组 Uniform |

### 属性与顶点格式

| API | 优先级 | 说明 |
|-----|--------|------|
| `glGetAttribLocation` | 🔴 高 | 获取属性位置 |
| `glVertexAttribIPointer` | 🟠 中 | 整数属性指针 |
| `glVertexAttribLPointer` | 🟠 中 | 双精度属性指针 |
| `glDisableVertexAttribArray` | 🟡 低 | 禁用属性 |

### 纹理高级特性

| API | 优先级 | 说明 |
|-----|--------|------|
| `glTexSubImage2D` | 🟠 中 | 部分纹理更新 |
| `glCopyTexImage2D` | 🟠 中 | 从帧缓冲复制 |
| `glCopyTexSubImage2D` | 🟠 中 | 部分纹理复制 |
| `glCompressedTexImage2D` | 🟠 中 | 压缩纹理加载 |
| `glCompressedTexSubImage2D` | 🟡 低 | 压缩纹理部分更新 |
| `glTexStorage2D` | 🟠 中 | 不可变纹理存储 |
| `glGetTexImage` | 🟡 低 | 读取纹理数据 |
| `glGetTexParameteriv` | 🟡 低 | 查询纹理参数 |
| `glTextureView` | 🟡 低 | 纹理视图 (DSA) |
| `glTextureBarrier` | 🟡 低 | 纹理屏障同步 |

### 纹理类型扩展

| 特性 | 优先级 | 说明 |
|------|--------|------|
| `GL_TEXTURE_3D` | 🟠 中 | 3D 纹理 |
| `GL_TEXTURE_CUBE_MAP` | 🟠 中 | 立方体贴图 |
| `GL_TEXTURE_1D_ARRAY` | 🟡 低 | 1D 纹理数组 |
| `GL_TEXTURE_2D_ARRAY` | 🟡 低 | 2D 纹理数组 |
| `GL_TEXTURE_BUFFER` | 🟡 低 | 缓冲区纹理 |

### 采样器对象

| API | 优先级 | 说明 |
|-----|--------|------|
| `glGenSamplers` | 🟠 中 | 创建采样器 |
| `glDeleteSamplers` | 🟠 中 | 删除采样器 |
| `glBindSampler` | 🟠 中 | 绑定采样器 |
| `glSamplerParameteri` | 🟠 中 | 设置采样器参数 |
| `glSamplerParameterf` | 🟠 中 | 设置采样器浮点参数 |

### 查询与状态获取

| API | 优先级 | 说明 |
|-----|--------|------|
| `glGetIntegerv` | 🟠 中 | 获取整数状态 |
| `glGetFloatv` | 🟠 中 | 获取浮点状态 |
| `glGetBooleanv` | 🟠 中 | 获取布尔状态 |
| `glGetString` | 🟠 中 | 获取版本、供应商等信息 |
| `glGetError` | 🟠 中 | 获取错误码 |
| `glIsEnabled` | 🟡 低 | 检查功能是否启用 |

### 性能与同步

| API | 优先级 | 说明 |
|-----|--------|------|
| `glFlush` | 🟠 中 | 刷新命令队列 |
| `glFinish` | 🟠 中 | 等待 GPU 完成 |
| `glFenceSync` | 🟡 低 | GPU 同步栅栏 |
| `glClientWaitSync` | 🟡 低 | CPU 等待同步 |
| `glWaitSync` | 🟡 低 | GPU 等待同步 |
| `glDeleteSync` | 🟡 低 | 删除同步对象 |
| `glMemoryBarrier` | 🟡 低 | 内存屏障 |

### 查询对象

| API | 优先级 | 说明 |
|-----|--------|------|
| `glGenQueries` | 🟡 低 | 创建查询对象 |
| `glDeleteQueries` | 🟡 低 | 删除查询 |
| `glBeginQuery` | 🟡 低 | 开始查询 |
| `glEndQuery` | 🟡 低 | 结束查询 |
| `glGetQueryObjectui64v` | 🟡 低 | 获取查询结果 |

### 条件渲染

| API | 优先级 | 说明 |
|-----|--------|------|
| `glBeginConditionalRender` | 🟡 低 | 条件渲染开始 |
| `glEndConditionalRender` | 🟡 低 | 条件渲染结束 |

### 间接绘制

| API | 优先级 | 说明 |
|-----|--------|------|
| `glDrawElementsIndirect` | 🟠 中 | 间接索引绘制 |
| `glDrawArraysIndirect` | 🟠 中 | 间接数组绘制 |
| `glMultiDrawElementsIndirect` | 🟡 低 | 多间接绘制 |

### 多重采样 (MSAA)

| API | 优先级 | 说明 |
|-----|--------|------|
| `glSampleMaski` | 🟡 低 | 采样掩码 |
| `glMinSampleShading` | 🟡 低 | 最小采样着色 |

### 细分着色器 (Tessellation)

| API | 优先级 | 说明 |
|-----|--------|------|
| `glPatchParameteri` | 🟡 低 | 面片参数 |
| `glPatchParameterfv` | 🟡 低 | 面片浮点参数 |

### 几何着色器 (Geometry Shader)

| 功能 | 优先级 | 说明 |
|------|--------|------|
| 几何着色器编译 | 🟡 低 | `glCompileShader` for GS |
| 几何着色器管线 | 🟡 低 | 管线中加入 GS 阶段 |

### 计算着色器 (Compute Shader)

| 功能 | 优先级 | 说明 |
|------|--------|------|
| 计算着色器编译 | 🟡 低 | `glCompileShader` for CS |
| `glDispatchCompute` | 🟡 低 | 分派计算工作 |
| `glDispatchComputeIndirect` | 🟡 低 | 间接分派 |

### Direct State Access (DSA)

| API | 优先级 | 说明 |
|-----|--------|------|
| `glCreateBuffers` | 🟡 低 | DSA 缓冲创建 |
| `glNamedBufferData` | 🟡 低 | DSA 缓冲数据 |
| `glCreateTextures` | 🟡 低 | DSA 纹理创建 |
| `glTextureSubImage2D` | 🟡 低 | DSA 纹理更新 |
| `glBindTextureUnit` | 🟡 低 | DSA 纹理单元绑定 |

### 帧缓冲对象 (FBO)

| API | 优先级 | 说明 |
|-----|--------|------|
| `glGenFramebuffers` | 🔴 高 | 创建帧缓冲 |
| `glBindFramebuffer` | 🔴 高 | 绑定帧缓冲 |
| `glFramebufferTexture2D` | 🔴 高 | 附加纹理 |
| `glFramebufferRenderbuffer` | 🔴 高 | 附加渲染缓冲 |
| `glCheckFramebufferStatus` | 🔴 高 | 检查状态 |
| `glGenerateMipmap` (FBO 版本) | 🟠 中 | FBO 的 Mipmap 生成 |

### 渲染缓冲区 (Renderbuffer)

| API | 优先级 | 说明 |
|-----|--------|------|
| `glGenRenderbuffers` | 🔴 高 | 创建渲染缓冲 |
| `glBindRenderbuffer` | 🔴 高 | 绑定渲染缓冲 |
| `glRenderbufferStorage` | 🔴 高 | 分配存储 |

---

## 📋 优先级说明

| 级别 | 说明 |
|------|------|
| 🔴 **高** | 核心功能，对大多数应用程序至关重要 |
| 🟠 **中** | 常用功能，许多应用程序需要 |
| 🟡 **低** | 可选功能，特定用途时需要 |

---

## 🎯 实现建议 (Roadmap)

### 第一阶段：核心扩展
1. **帧缓冲对象** - 离屏渲染支持
2. **着色器编译** - 运行时 GLSL 编译
3. **状态管理** - 完善状态查询 `glGet*`
4. **Uniform 系统** - `glGetUniformLocation`, `glUniform*`

### 第二阶段：质量改进
1. **纹理高级特性** - `glTexSubImage2D`, 3D 纹理
2. **裁剪增强** - 裁剪矩形 `glScissor`
3. **混合与掩码** - 颜色混合，写入掩码
4. **采样器对象** - 独立采样器管理

### 第三阶段：高级功能
1. **间接绘制** - `glDrawElementsIndirect`
2. **多重采样** - MSAA 支持
3. **计算着色器** - 通用计算支持

---

## 📁 文件位置参考

| 功能 | 文件位置 |
|------|---------|
| 核心渲染器 | `vc/tinygl.h` |
| 运行时框架 | `vc/vc.cpp` |
| 绘图库 | `vc/olive.cpp` |
| 示例代码 | `demos/*.cpp` |

---

## 🔗 相关标准与文档

- [OpenGL 3.3 规范](https://www.khronos.org/registry/OpenGL/specs/gl/glspec33.core.pdf)
- [OpenGL 4.6 规范](https://www.khronos.org/registry/OpenGL/specs/gl/glspec46.core.pdf)
- [GLSL 3.30 规范](https://www.khronos.org/registry/OpenGL/specs/gl/GLSLangSpec.3.30.pdf)

---

**最后更新**：2026-01-02