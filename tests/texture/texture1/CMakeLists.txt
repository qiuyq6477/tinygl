add_tinygl_test(test_texture_texture1 texture1_test.cpp)

# Copy assets to the build directory structure
# This copies 'assets' folder to ${CMAKE_BINARY_DIR}/tests/texture/cube/assets
file(COPY assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
