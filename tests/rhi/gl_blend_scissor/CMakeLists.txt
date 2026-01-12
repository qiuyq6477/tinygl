add_executable(gl_blend_scissor gl_blend_scissor.cpp)
target_link_libraries(gl_blend_scissor PRIVATE tinygl_framework ${SDL2_LIBRARIES})

if(WIN32)
    add_custom_command(TARGET gl_blend_scissor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:tinygl_framework>
        $<TARGET_FILE_DIR:gl_blend_scissor>
        COMMENT "Copying tinygl_framework.dll..."
    )
endif()
