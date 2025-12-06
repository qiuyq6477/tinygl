cmake_minimum_required(VERSION 3.10)
project(tinygl CXX)

# 设置 C 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
    
    # 为每个案例创建可执行文件
    foreach(DEMO_FILE ${DEMO_FILES})
        get_filename_component(DEMO_NAME ${DEMO_FILE} NAME_WE)
        add_executable(${DEMO_NAME} ${DEMO_FILE})
        target_link_libraries(${DEMO_NAME} ${SDL2_LIBRARIES})
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
