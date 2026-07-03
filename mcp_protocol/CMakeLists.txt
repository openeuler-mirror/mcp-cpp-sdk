cmake_minimum_required(VERSION 3.10)
project(mcp_protocol VERSION 1.0.0 LANGUAGES CXX)

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

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 查找依赖库
find_package(Threads REQUIRED)
find_package(GTest REQUIRED)

# 源文件
set(PROTOCOL_SOURCES
    src/mcp_protocol_core.cpp
    ../log/src/logger.cpp
)

# 头文件
set(PROTOCOL_HEADERS
    include/mcp_protocol_core.h
    include/mcp_message.h
    include/mcp_protocol_messages.h
)

# 创建静态库
add_library(mcp_protocol STATIC ${PROTOCOL_SOURCES} ${PROTOCOL_HEADERS})

# 设置库属性
set_target_properties(mcp_protocol PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 链接依赖库
target_link_libraries(mcp_protocol
    Threads::Threads
)

# 设置库的包含目录
target_include_directories(mcp_protocol PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 编译定义
target_compile_definitions(mcp_protocol PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 创建示例程序
add_executable(protocol_example examples/protocol_example.cpp)

# 创建 GTest 测试程序
add_executable(protocol_gtest test/mcp_protocol_gtest.cpp)

# 链接示例程序
target_link_libraries(protocol_example
    mcp_protocol
    Threads::Threads
)

target_link_libraries(protocol_gtest
    mcp_protocol
    GTest::GTest
    GTest::Main
    Threads::Threads
)

# 设置示例程序和测试程序的包含目录
target_include_directories(protocol_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_include_directories(protocol_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 启用测试
enable_testing()
add_test(NAME protocol_gtest COMMAND protocol_gtest)

# 安装规则
install(TARGETS mcp_protocol protocol_example protocol_gtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/mcp_protocol
)

install(FILES include/mcp_protocol_core.h include/mcp_message.h include/mcp_protocol_messages.h
    DESTINATION include
)

# 打印配置信息
message(STATUS "MCP Protocol Configuration:")
message(STATUS "  Project Name: ${PROJECT_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Output Directory: ${CMAKE_BINARY_DIR}")