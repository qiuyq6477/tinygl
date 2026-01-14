
# Enable testing for this directory and below
enable_testing()

# Define properties to collect test libraries
define_property(GLOBAL PROPERTY TINYGL_TEST_LIBS
    BRIEF_DOCS "List of all tinygl test case libraries"
    FULL_DOCS "List of all tinygl test case libraries"
)
define_property(GLOBAL PROPERTY TINYGL_RHI_TEST_LIBS
    BRIEF_DOCS "List of RHI test case libraries"
    FULL_DOCS "List of RHI test case libraries to link into rhi_runner"
)
define_property(GLOBAL PROPERTY TINYGL_CORE_TEST_LIBS
    BRIEF_DOCS "List of Core test case libraries"
    FULL_DOCS "List of Core test case libraries to link into test_runner"
)

# Find SDL2 (Required for camera test which uses SDL directly)
find_package(SDL2 REQUIRED)

# Helper function to register a test
function(add_tinygl_test target_name)
    add_library(${target_name} STATIC ${ARGN})
    target_link_libraries(${target_name} PRIVATE tinygl_framework SDL2::SDL2main SDL2::SDL2)
    
    # Add to global list
    set_property(GLOBAL APPEND PROPERTY TINYGL_TEST_LIBS ${target_name})
    
    # Categorize based on path
    # Check if the current source directory contains "/rhi/" or ends with "/rhi"
    string(FIND "${CMAKE_CURRENT_SOURCE_DIR}" "/rhi" RHI_POS)
    
    if(RHI_POS GREATER -1)
        set_property(GLOBAL APPEND PROPERTY TINYGL_RHI_TEST_LIBS ${target_name})
    else()
        set_property(GLOBAL APPEND PROPERTY TINYGL_CORE_TEST_LIBS ${target_name})
    endif()
endfunction()

# Auto-discovery of test cases
# We look for CMakeLists.txt in subdirectories (depth 2: group/testname)
file(GLOB_RECURSE ALL_CMAKELISTS "*/CMakeLists.txt")

foreach(cmake_file ${ALL_CMAKELISTS})
    get_filename_component(test_dir ${cmake_file} DIRECTORY)
    
    # Skip the current directory's CMakeLists.txt
    if(NOT test_dir STREQUAL CMAKE_CURRENT_SOURCE_DIR)
        # Compute a unique binary directory to avoid conflicts
        file(RELATIVE_PATH rel_path ${CMAKE_CURRENT_SOURCE_DIR} ${test_dir})
        add_subdirectory(${test_dir} ${CMAKE_CURRENT_BINARY_DIR}/${rel_path})
    endif()
endforeach()

# Collect registered libraries
get_property(ALL_TEST_LIBS GLOBAL PROPERTY TINYGL_TEST_LIBS)
get_property(RHI_TEST_LIBS GLOBAL PROPERTY TINYGL_RHI_TEST_LIBS)
get_property(CORE_TEST_LIBS GLOBAL PROPERTY TINYGL_CORE_TEST_LIBS)

# --- Test Runner (Core) ---
add_executable(test_runner test_runner.cpp)
target_link_libraries(test_runner PRIVATE tinygl_framework ${SDL2_LIBRARIES})

if (CORE_TEST_LIBS)
    if(APPLE)
        target_link_libraries(test_runner PRIVATE -Wl,-all_load ${CORE_TEST_LIBS})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
        target_link_libraries(test_runner PRIVATE -Wl,--whole-archive ${CORE_TEST_LIBS} -Wl,--no-whole-archive)
    elseif(MSVC)
        foreach(lib ${CORE_TEST_LIBS})
            target_link_libraries(test_runner PRIVATE "/WHOLEARCHIVE:${lib}")
        endforeach()
    else()
        target_link_libraries(test_runner PRIVATE ${CORE_TEST_LIBS})
    endif()
endif()

# --- RHI Runner (RHI Only) ---
add_executable(rhi_runner rhi_runner.cpp ../src/third_party/glad.c)
target_link_libraries(rhi_runner PRIVATE tinygl_framework ${SDL2_LIBRARIES})

if (RHI_TEST_LIBS)
    if(APPLE)
        target_link_libraries(rhi_runner PRIVATE -Wl,-all_load ${RHI_TEST_LIBS})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
        target_link_libraries(rhi_runner PRIVATE -Wl,--whole-archive ${RHI_TEST_LIBS} -Wl,--no-whole-archive)
    elseif(MSVC)
        foreach(lib ${RHI_TEST_LIBS})
            target_link_libraries(rhi_runner PRIVATE "/WHOLEARCHIVE:${lib}")
        endforeach()
    else()
        target_link_libraries(rhi_runner PRIVATE ${RHI_TEST_LIBS})
    endif()
endif()

# [Windows] Copy tinygl_framework.dll to runner directories
if(WIN32)
    foreach(TARGET test_runner rhi_runner)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:tinygl_framework>
            $<TARGET_FILE_DIR:${TARGET}>
            COMMENT "Copying tinygl_framework.dll to ${TARGET} directory..."
        )
    endforeach()
endif()
