# 查找 SDL2
find_package(SDL2 REQUIRED)

# 定义平台宏
add_definitions(-DVC_PLATFORM=VC_SDL_PLATFORM)

# [库定义] 框架库
# 注意路径是相对于当前 CMakeLists.txt 的
add_library(tinygl_framework STATIC 
    framework/platform.cpp
    core/context.cpp
)

# 这里的 include 目录需要暴露给链接此库的目标
target_include_directories(tinygl_framework PUBLIC ${SDL2_INCLUDE_DIRS})

# 链接 SDL2
target_link_libraries(tinygl_framework PRIVATE ${SDL2_LIBRARIES})

# 设置头文件搜索路径
# PUBLIC: 链接此库的目标也能看到这些头文件
target_include_directories(tinygl_framework PUBLIC 
    ${CMAKE_SOURCE_DIR}/include
)