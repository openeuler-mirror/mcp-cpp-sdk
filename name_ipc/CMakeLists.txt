# CMakeLists.txt for named pipe communication module

cmake_minimum_required(VERSION 3.10)
project(mcp_named_pipe VERSION 1.0.0 LANGUAGES CXX)

# 设置默认构建类型为 Debug (如果未设置)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4 /Zi /Od")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -g -O0")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖
find_package(Threads REQUIRED)

# 依赖 log 模块 - 如果不存在，尝试添加 log 子目录（支持独立构建）
if(NOT TARGET mcp_logger)
    # 检查是否可以添加 log 子目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../log/CMakeLists.txt")
        message(STATUS "mcp_logger target not found, adding log subdirectory for standalone build")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../log ${CMAKE_BINARY_DIR}/log)
    else()
        message(FATAL_ERROR "mcp_logger target not found and log module not found. Please ensure log module is added before ipc module, or build from the project root directory.")
    endif()
endif()

# 源文件
set(NAMED_PIPE_SOURCES
    src/named_pipe.cpp
)

# 头文件
set(NAMED_PIPE_HEADERS
    include/named_pipe.h
)

# 创建共享库（改为 SHARED 以支持模块化）
add_library(mcp_named_pipe SHARED ${NAMED_PIPE_SOURCES} ${NAMED_PIPE_HEADERS})

# 设置库属性
set_target_properties(mcp_named_pipe PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 设置包含目录 - PUBLIC 会传播给链接此库的目标
target_include_directories(mcp_named_pipe PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<INSTALL_INTERFACE:include>
)

# PRIVATE 包含目录 - 仅用于编译此库
target_include_directories(mcp_named_pipe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 链接依赖库
target_link_libraries(mcp_named_pipe
    PUBLIC
        mcp_logger
        Threads::Threads
)

# 平台特定设置
if(WIN32)
    target_link_libraries(mcp_named_pipe PRIVATE kernel32)
endif()

# 编译定义
target_compile_definitions(mcp_named_pipe PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 创建示例可执行文件
add_executable(named_pipe_server_example examples/named_pipe_server_example.cpp)
target_link_libraries(named_pipe_server_example mcp_named_pipe)
target_include_directories(named_pipe_server_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

add_executable(named_pipe_client_example examples/named_pipe_client_example.cpp)
target_link_libraries(named_pipe_client_example mcp_named_pipe)
target_include_directories(named_pipe_client_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

add_executable(named_pipe_client_test examples/named_pipe_client_test.cpp)
target_link_libraries(named_pipe_client_test mcp_named_pipe)
target_include_directories(named_pipe_client_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

add_executable(named_pipe_server_test examples/named_pipe_server_test.cpp)
target_link_libraries(named_pipe_server_test mcp_named_pipe)
target_include_directories(named_pipe_server_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 安装规则
install(TARGETS mcp_named_pipe
    EXPORT mcp_named_pipeTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS named_pipe_server_example named_pipe_client_example named_pipe_client_test named_pipe_server_test
    RUNTIME DESTINATION bin
)

install(FILES ${NAMED_PIPE_HEADERS}
    DESTINATION include/mcp/named_pipe
)

# 测试
enable_testing()

# 添加基本测试
add_executable(named_pipe_test test/named_pipe_test.cpp)
target_link_libraries(named_pipe_test mcp_named_pipe)
set_target_properties(named_pipe_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

target_include_directories(named_pipe_test PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

add_test(NAME named_pipe_test 
    COMMAND named_pipe_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

set_tests_properties(named_pipe_test PROPERTIES
    TIMEOUT 30
)
