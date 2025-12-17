cmake_minimum_required(VERSION 3.10)
project(tinygl CXX)

# 设置 C 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========================================================
# [配置] 构建类型与优化
# ========================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected, defaulting to Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release)" FORCE)
endif()

if(MSVC)
    # Windows MSVC 编译器优化
    add_compile_options(/O2 /Oi /Ot /GL)
else()
    # macOS/Linux (Clang/GCC) 编译器优化
    # -O3: 最高级别通用优化
    # -march=native: 告诉编译器针对当前机器的 CPU (Apple Silicon 或 x86) 生成专用指令，这对 SIMD 至关重要
    # -ffast-math: 允许编译器牺牲微小的浮点精度换取速度 (图形学常用)
    # -funroll-loops: 激进的循环展开
    add_compile_options(-O3 -march=native -ffast-math -funroll-loops)
    
    # [可选] 如果你需要调试性能问题，可以开启 debug info 但保留优化
    # set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -g -march=native")
endif()

# ========================================================
# [配置] 平台选择
# ========================================================
# 默认使用 SDL 平台，可以通过命令行参数修改
if(NOT DEFINED VC_PLATFORM)
    set(VC_PLATFORM VC_SDL_PLATFORM)
endif()

# ========================================================
# [构建] 添加子目录
# ========================================================
# 先添加 src，因为 demos 依赖于 src 中定义的库
add_subdirectory(src)
add_subdirectory(demos)