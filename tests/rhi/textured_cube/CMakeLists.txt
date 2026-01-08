add_tinygl_test(rhi_textured_cube_test rhi_textured_cube_test.cpp)

file(COPY assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
