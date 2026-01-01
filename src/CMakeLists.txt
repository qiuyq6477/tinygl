# 查找 SDL2
find_package(SDL2 REQUIRED)
find_package(assimp REQUIRED)

# 定义平台宏
add_definitions(-DVC_PLATFORM=VC_SDL_PLATFORM)

# [库定义] 框架库 (现代 C++ 架构)
add_library(tinygl_framework SHARED 
    framework/application.cpp
    framework/ui_renderer.cpp
    framework/microui.c
    framework/mesh.cpp
    framework/model.cpp
    core/gl_texture.cpp
    core/gl_buffer.cpp
    core/gl_clip.cpp
    core/gl_util.cpp
    core/tinygl.cpp
)

# 定义导出宏
target_compile_definitions(tinygl_framework PRIVATE TINYGL_EXPORTS)

# macOS dynamic lookup for runtime symbols (though Application uses virtual functions now, 
# keeping it for flexibility if needed, or we can remove it if everything is linked).
if(APPLE)
    target_link_options(tinygl_framework PRIVATE -undefined dynamic_lookup)
endif()

# RPATH
set_target_properties(tinygl_framework PROPERTIES
    MACOSX_BUNDLE_GUI_FOLDER "tinygl"
    INSTALL_RPATH "@loader_path"
)

target_include_directories(tinygl_framework PUBLIC 
    ${SDL2_INCLUDE_DIRS} 
    ${CMAKE_SOURCE_DIR}/include
)
target_link_libraries(tinygl_framework PRIVATE ${SDL2_LIBRARIES} PUBLIC assimp::assimp)
