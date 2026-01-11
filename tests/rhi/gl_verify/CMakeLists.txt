add_executable(gl_verify_test gl_verify_test.cpp)
target_link_libraries(gl_verify_test PRIVATE tinygl_framework SDL2::SDL2main SDL2::SDL2)

if(APPLE)
    # Ensure it finds the framework dynamic library (if rpath is not set correctly or for dev)
    # But usually RPATH takes care of it.
endif()