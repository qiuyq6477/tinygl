# TinyGL 项目

这是一个基于 VC 框架的图形渲染演示项目。

## 项目结构

```
tinygl/
├── vc/              # VC 框架核心文件
│   ├── vc.c         # VC 框架实现（提供程序执行流程）
│   └── olive.c      # Olivec 图形库
├── demos/           # 演示案例
│   ├── triangle.c   # 2D 三角形旋转演示
│   ├── triangle3d.c  # 3D 三角形演示
│   └── dots3d.c     # 3D 点阵演示
├── CMakeLists.txt   # CMake 构建配置
└── main.cpp         # C++ 渲染器实现（独立项目）
```

## 构建说明

### 使用 SDL 平台（默认，推荐）

```bash
mkdir build && cd build
cmake ..
make
```

### 使用终端平台

```bash
mkdir build && cd build
cmake -DVC_PLATFORM=VC_TERM_PLATFORM ..
make
```

### 使用 WASM 平台

```bash
mkdir build && cd build
cmake -DVC_PLATFORM=VC_WASM_PLATFORM ..
make
```

## 运行案例

编译完成后，在 `build` 目录下会生成三个可执行文件：

### 方法 1: 使用运行脚本（推荐）

```bash
# 从项目根目录运行
./run.sh triangle    # 运行 2D 三角形旋转演示
./run.sh triangle3d  # 运行 3D 三角形演示
./run.sh dots3d      # 运行 3D 点阵演示
```

### 方法 2: 直接运行

```bash
cd build
./triangle    # 运行 2D 三角形旋转演示
./triangle3d  # 运行 3D 三角形演示
./dots3d     # 运行 3D 点阵演示
```

## 依赖

- **SDL 平台**: 需要安装 SDL2 库
  - macOS: `brew install sdl2`
  - Ubuntu: `sudo apt-get install libsdl2-dev`
  
- **终端平台**: 需要数学库（通常系统自带）

- **WASM 平台**: 需要 WebAssembly 工具链

## VC 框架说明

VC (Virtual Console) 是一个轻量级的图形渲染框架，提供了：

- 跨平台支持（SDL、终端、WASM）
- 统一的渲染接口 `vc_render(float dt)`
- 自动处理窗口创建、事件循环等

每个案例只需要实现 `vc_render` 函数，框架会自动处理其余部分。

