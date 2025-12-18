# Only demo_cube.cpp remains
add_executable(demo_cube demo_cube.cpp)

# Link to the framework
target_link_libraries(demo_cube PRIVATE tinygl_framework)

# Compile options
target_compile_options(demo_cube PRIVATE -Wall -Wextra)

# Copy assets
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
