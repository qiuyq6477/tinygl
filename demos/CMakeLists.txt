# 自动扫描 demos 目录下的所有 .c 文件
file(GLOB DEMO_FILES "*.cpp")

# 提取文件名（去掉路径和扩展名）作为案例名称
set(DEMOS "")
foreach(DEMO_FILE ${DEMO_FILES})
    get_filename_component(DEMO_NAME ${DEMO_FILE} NAME_WE)
    
    # 创建可执行文件
    add_executable(${DEMO_NAME} ${DEMO_FILE})
    
    # 链接框架库
    target_link_libraries(${DEMO_NAME} PRIVATE tinygl_framework)
    
    # 编译选项
    target_compile_options(${DEMO_NAME} PRIVATE -Wall -Wextra)
endforeach()

# [资源复制]
# 将 assets 目录复制到构建目录 (build/demos/assets)
# 这样编译出的程序可以通过 "assets/filename" 访问资源
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/assets DESTINATION ${CMAKE_CURRENT_BINARY_DIR})