cmake_minimum_required(VERSION 3.10)
project(tinygl CXX)

# 设置 C 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 全局包含目录
include_directories(include)

# ========================================================
# [新增] 1. 设置默认构建类型为 Release
# 如果用户没有指定 -DCMAKE_BUILD_TYPE=Debug，默认开启全速优化
# ========================================================
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No build type selected, defaulting to Release")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type (Debug, Release)" FORCE)
endif()

# ========================================================
# [新增] 2. 添加极致性能优化 flag
# ========================================================
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

# 默认使用 SDL 平台，可以通过命令行参数修改
# 例如: cmake -DVC_PLATFORM=VC_TERM_PLATFORM ..
if(NOT DEFINED VC_PLATFORM)
    set(VC_PLATFORM VC_SDL_PLATFORM)
endif()

# 自动扫描 demos 目录下的所有 .c 文件
file(GLOB DEMO_FILES "demos/*.cpp")
# 提取文件名（去掉路径和扩展名）作为案例名称
set(DEMOS "")
foreach(DEMO_FILE ${DEMO_FILES})
    get_filename_component(DEMO_NAME ${DEMO_FILE} NAME_WE)
    list(APPEND DEMOS ${DEMO_NAME})
endforeach()
# 排序以确保输出顺序一致
list(SORT DEMOS)

# 根据平台设置不同的编译选项和链接库
if(VC_PLATFORM STREQUAL "VC_SDL_PLATFORM")
    # 查找 SDL2
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    
    # 定义平台宏
    add_definitions(-DVC_PLATFORM=VC_SDL_PLATFORM)

    # [新增] 框架库
    add_library(tinygl_framework STATIC src/framework/platform.cpp)
    target_link_libraries(tinygl_framework ${SDL2_LIBRARIES})
    
    # 为每个案例创建可执行文件
    foreach(DEMO_FILE ${DEMO_FILES})
        get_filename_component(DEMO_NAME ${DEMO_FILE} NAME_WE)
        add_executable(${DEMO_NAME} ${DEMO_FILE})
        target_link_libraries(${DEMO_NAME} tinygl_framework)
        target_compile_options(${DEMO_NAME} PRIVATE -Wall -Wextra)
    endforeach()
    
elseif(VC_PLATFORM STREQUAL "VC_TERM_PLATFORM")
    # 终端平台，需要数学库
    add_definitions(-DVC_PLATFORM=VC_TERM_PLATFORM)
    
    # 为每个案例创建可执行文件
    foreach(DEMO_FILE ${DEMO_FILES})
        get_filename_component(DEMO_NAME ${DEMO_FILE} NAME_WE)
        add_executable(${DEMO_NAME} ${DEMO_FILE})
        target_link_libraries(${DEMO_NAME} m)
        target_compile_options(${DEMO_NAME} PRIVATE -Wall -Wextra)
    endforeach()
    
elseif(VC_PLATFORM STREQUAL "VC_WASM_PLATFORM")
    # WASM 平台
    add_definitions(-DVC_PLATFORM=VC_WASM_PLATFORM)
    
    # 为每个案例创建可执行文件
    foreach(DEMO_FILE ${DEMO_FILES})
        get_filename_component(DEMO_NAME ${DEMO_FILE} NAME_WE)
        add_executable(${DEMO_NAME} ${DEMO_FILE})
        target_compile_options(${DEMO_NAME} PRIVATE -Wall -Wextra)
    endforeach()
    
else()
    message(FATAL_ERROR "Unknown VC_PLATFORM: ${VC_PLATFORM}")
endif()
