add_executable(gl_verify_test gl_verify_test.cpp)
target_link_libraries(gl_verify_test PRIVATE tinygl_framework SDL2::SDL2main SDL2::SDL2)

if(WIN32)
    add_custom_command(TARGET gl_verify_test POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:tinygl_framework>
        $<TARGET_FILE_DIR:gl_verify_test>
        COMMENT "Copying tinygl_framework.dll..."
    )
endif()
