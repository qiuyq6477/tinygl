
# Enable testing for this directory and below
enable_testing()

# Define properties to collect test libraries
define_property(GLOBAL PROPERTY TINYGL_TEST_LIBS
    BRIEF_DOCS "List of all tinygl test case libraries"
    FULL_DOCS "List of all tinygl test case libraries"
)
define_property(GLOBAL PROPERTY TINYGL_FRAMEWORK_TEST_LIBS
    BRIEF_DOCS "List of Framework/RHI test case libraries"
    FULL_DOCS "List of Framework/RHI test case libraries to link into framework_tester"
)
define_property(GLOBAL PROPERTY TINYGL_CORE_TEST_LIBS
    BRIEF_DOCS "List of Core test case libraries"
    FULL_DOCS "List of Core test case libraries to link into tinygl_tester"
)

# Find SDL2 (Required for camera test which uses SDL directly)
find_package(SDL2 REQUIRED)

# --- Common Test Utilities & Include Paths ---
# Collect optional sources from tests/src
file(GLOB_RECURSE TEST_UTILS_SOURCES "src/*.cpp")

if(TEST_UTILS_SOURCES)
    add_library(tinygl_test_utils STATIC ${TEST_UTILS_SOURCES})
    target_link_libraries(tinygl_test_utils PUBLIC tinygl_framework SDL2::SDL2main SDL2::SDL2)
    target_include_directories(tinygl_test_utils PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}         # Allows <ITestCase.h>
        ${CMAKE_CURRENT_SOURCE_DIR}/include # Allows custom headers in tests/include/
    )
else()
    # If no sources, create an INTERFACE library to hold properties
    add_library(tinygl_test_utils INTERFACE)
    target_link_libraries(tinygl_test_utils INTERFACE tinygl_framework SDL2::SDL2main SDL2::SDL2)
    target_include_directories(tinygl_test_utils INTERFACE 
        ${CMAKE_CURRENT_SOURCE_DIR} 
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )
endif()

# Helper function to register a test
function(add_tinygl_test target_name)
    add_library(${target_name} STATIC ${ARGN})
    # Link against the common test utils (which pulls in framework, SDL2, and includes)
    target_link_libraries(${target_name} PRIVATE tinygl_test_utils)
    
    # Calculate relative path from tests root (e.g. "framework/rhi/basic")
    # CMAKE_SOURCE_DIR is project root. We assume tests are in ${CMAKE_SOURCE_DIR}/tests
    file(RELATIVE_PATH TEST_REL_PATH "${CMAKE_SOURCE_DIR}/tests" "${CMAKE_CURRENT_SOURCE_DIR}")
    
    # Pass directory path as Group Name
    target_compile_definitions(${target_name} PRIVATE TINYGL_TEST_GROUP="${TEST_REL_PATH}")

    # Add to global list
    set_property(GLOBAL APPEND PROPERTY TINYGL_TEST_LIBS ${target_name})
    
    # Categorize based on path: if it starts with "framework", it goes to framework_tester
    string(FIND "${TEST_REL_PATH}" "framework" FRAMEWORK_POS)
    
    if(FRAMEWORK_POS EQUAL 0)
        set_property(GLOBAL APPEND PROPERTY TINYGL_FRAMEWORK_TEST_LIBS ${target_name})
    else()
        set_property(GLOBAL APPEND PROPERTY TINYGL_CORE_TEST_LIBS ${target_name})
    endif()
endfunction()

# --- Asset Copying ---
# Copy tests/assets to the build directory so tests can find them
add_custom_target(copy_test_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    ${CMAKE_CURRENT_BINARY_DIR}/assets
    COMMENT "Copying test assets..."
)

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
get_property(FRAMEWORK_TEST_LIBS GLOBAL PROPERTY TINYGL_FRAMEWORK_TEST_LIBS)
get_property(CORE_TEST_LIBS GLOBAL PROPERTY TINYGL_CORE_TEST_LIBS)

# --- Test Runner (Core) ---
add_executable(tinygl_tester tinygl_tester.cpp)
target_link_libraries(tinygl_tester PRIVATE tinygl_test_utils ${SDL2_LIBRARIES})

if (CORE_TEST_LIBS)
    if(APPLE)
        target_link_libraries(tinygl_tester PRIVATE -Wl,-all_load ${CORE_TEST_LIBS})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
        target_link_libraries(tinygl_tester PRIVATE -Wl,--whole-archive ${CORE_TEST_LIBS} -Wl,--no-whole-archive)
    elseif(MSVC)
        foreach(lib ${CORE_TEST_LIBS})
            target_link_libraries(tinygl_tester PRIVATE "/WHOLEARCHIVE:${lib}")
        endforeach()
    else()
        target_link_libraries(tinygl_tester PRIVATE ${CORE_TEST_LIBS})
    endif()
endif()
add_dependencies(tinygl_tester copy_test_assets)

# --- Framework Runner (Framework/RHI) ---
# Replaces old rhi_tester
add_executable(framework_tester framework_tester.cpp ../src/third_party/glad.c)
target_link_libraries(framework_tester PRIVATE tinygl_test_utils ${SDL2_LIBRARIES})

if (FRAMEWORK_TEST_LIBS)
    if(APPLE)
        target_link_libraries(framework_tester PRIVATE -Wl,-all_load ${FRAMEWORK_TEST_LIBS})
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR MINGW)
        target_link_libraries(framework_tester PRIVATE -Wl,--whole-archive ${FRAMEWORK_TEST_LIBS} -Wl,--no-whole-archive)
    elseif(MSVC)
        foreach(lib ${FRAMEWORK_TEST_LIBS})
            target_link_libraries(framework_tester PRIVATE "/WHOLEARCHIVE:${lib}")
        endforeach()
    else()
        target_link_libraries(framework_tester PRIVATE ${FRAMEWORK_TEST_LIBS})
    endif()
endif()
add_dependencies(framework_tester copy_test_assets)

# [Windows] Copy tinygl_framework.dll to runner directories
if(WIN32)
    foreach(TARGET tinygl_tester framework_tester)
        add_custom_command(TARGET ${TARGET} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:tinygl_framework>
            $<TARGET_FILE_DIR:${TARGET}>
            COMMENT "Copying tinygl_framework.dll to ${TARGET} directory..."
        )
    endforeach()
endif()
