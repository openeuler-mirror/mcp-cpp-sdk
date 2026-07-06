cmake_minimum_required(VERSION 3.10)
project(MCPLogger)

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

# 包含头文件目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 查找Google Test
find_package(GTest REQUIRED)

# 创建日志库
add_library(mcp_logger STATIC
    src/logger.cpp
)

# 设置库的包含目录
target_include_directories(mcp_logger PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 创建日志测试示例
add_executable(logger_example
    examples/logger_example.cpp
)

# 创建Google Test测试程序
add_executable(logger_gtest
    test/logger_gtest.cpp
)

# 链接库
target_link_libraries(logger_example
    mcp_logger
)

target_link_libraries(logger_gtest
    mcp_logger
    GTest::GTest
    GTest::Main
    Threads::Threads
)

# 设置编译特性
target_compile_features(mcp_logger PUBLIC cxx_std_17)
target_compile_features(logger_example PRIVATE cxx_std_17)
target_compile_features(logger_gtest PRIVATE cxx_std_17)

# 添加编译定义
target_compile_definitions(mcp_logger PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

# 如果是Windows，需要链接pthread
if(WIN32)
    # Windows下通常不需要额外链接pthread
else()
    # Unix-like系统需要链接pthread
    find_package(Threads REQUIRED)
    target_link_libraries(mcp_logger Threads::Threads)
    target_link_libraries(logger_example Threads::Threads)
endif()

# 安装规则
install(TARGETS mcp_logger logger_example logger_gtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES include/logger.h
    DESTINATION include
)

# 打印配置信息
message(STATUS "MCP Logger Configuration:")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Output Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

# 添加自定义目标用于运行示例
add_custom_target(run_logger_example
    COMMAND logger_example
    DEPENDS logger_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running logger example"
)

# 添加清理目标
add_custom_target(clean_logs
    COMMAND ${CMAKE_COMMAND} -E remove -f test_log.txt test_log.txt.*
    COMMENT "Cleaning log files"
)
