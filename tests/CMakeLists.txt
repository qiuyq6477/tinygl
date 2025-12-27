
# Define a property to collect all test libraries
define_property(GLOBAL PROPERTY TINYGL_TEST_LIBS
    BRIEF_DOCS "List of tinygl test case libraries"
    FULL_DOCS "List of tinygl test case libraries to link into test_runner"
)

# Find SDL2 (Required for camera test which uses SDL directly)
find_package(SDL2 REQUIRED)

# Helper function to register a test
function(add_tinygl_test target_name)
    add_library(${target_name} STATIC ${ARGN})
    target_link_libraries(${target_name} PRIVATE tinygl_framework)
    # Add to global list
    set_property(GLOBAL APPEND PROPERTY TINYGL_TEST_LIBS ${target_name})
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

# Collect all registered libraries
get_property(TEST_LIBS GLOBAL PROPERTY TINYGL_TEST_LIBS)

# Create the runner executable
add_executable(test_runner test_runner.cpp)

# Link the core framework first
target_link_libraries(test_runner PRIVATE tinygl_framework ${SDL2_LIBRARIES})

# Link test libraries with platform-specific flags to force the linker
# to include all object files from the static libs. This is crucial for
# the self-registration pattern to work.
if (TEST_LIBS)
    if(APPLE)
        # For macOS/iOS linker (ld)
        target_link_libraries(test_runner PRIVATE -Wl,-all_load ${TEST_LIBS})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # For GNU ld or lld on Linux
        target_link_libraries(test_runner PRIVATE -Wl,--whole-archive ${TEST_LIBS} -Wl,--no-whole-archive)
    elseif(MSVC)
        # For MSVC linker. The syntax is slightly different for each lib.
        foreach(lib ${TEST_LIBS})
            target_link_libraries(test_runner PRIVATE "/WHOLEARCHIVE:${lib}")
        endforeach()
    else()
        # Fallback for other systems, might not work as intended.
        target_link_libraries(test_runner PRIVATE ${TEST_LIBS})
    endif()
endif()
