# 查找 SDL2
find_package(SDL2 REQUIRED)

# 定义平台宏
add_definitions(-DVC_PLATFORM=VC_SDL_PLATFORM)

# [库定义] 框架库
# 注意路径是相对于当前 CMakeLists.txt 的
add_library(tinygl_framework SHARED 
    framework/platform.cpp
    core/gl_texture.cpp
    core/gl_buffer.cpp
    core/gl_clip.cpp
    core/gl_util.cpp
    core/tinygl.cpp
)

# 定义导出宏，用于 Windows DLL 导出
target_compile_definitions(tinygl_framework PRIVATE TINYGL_EXPORTS)

# [macOS 关键修正]
# 允许动态库中存在未定义的符号 (如 vc_render, vc_init)，
# 这些符号将由加载该库的主程序 (Demos) 提供。
if(APPLE)
    target_link_options(tinygl_framework PRIVATE -undefined dynamic_lookup)
endif()

# 确保在 macOS 上正确处理动态库路径 (RPATH)
set_target_properties(tinygl_framework PROPERTIES
    MACOSX_BUNDLE_GUI_FOLDER "tinygl"
    INSTALL_RPATH "@loader_path"
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