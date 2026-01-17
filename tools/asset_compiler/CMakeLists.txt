
find_package(assimp REQUIRED)

add_executable(tinygl_ac
    main.cpp
    asset_cooker.cpp
    asset_cooker.h
)

target_include_directories(tinygl_ac PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/third_party
)

target_link_libraries(tinygl_ac PRIVATE assimp::assimp)

# Copy to bin directory or specific tool output?
# For now, cmake build output is fine.
