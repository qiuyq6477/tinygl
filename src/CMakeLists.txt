# 查找 SDL2
find_package(SDL2 REQUIRED)
find_package(assimp REQUIRED)

# 定义平台宏
add_definitions(-DVC_PLATFORM=VC_SDL_PLATFORM)

# [库定义] 框架库 (现代 C++ 架构)
add_library(tinygl_framework SHARED 
    third_party/glad.c
    third_party/microui.c
    third_party/stb_image.cpp
    framework/atlas.c
    framework/geometry.cpp
    framework/application.cpp
    framework/ui_renderer.cpp
    framework/mesh.cpp
    framework/model.cpp
    framework/texture.cpp
    framework/texture_manager.cpp
    tinygl/gl_texture.cpp
    tinygl/gl_buffer.cpp
    tinygl/gl_clip.cpp
    tinygl/gl_util.cpp
    tinygl/tinygl.cpp
    rhi/soft_device.cpp
    rhi/gl_device.cpp
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
    ${CMAKE_SOURCE_DIR}/include/third_party
)

target_link_libraries(tinygl_framework PRIVATE ${SDL2_LIBRARIES} PUBLIC assimp::assimp)
