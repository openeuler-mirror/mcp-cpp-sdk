cmake_minimum_required(VERSION 3.10)
project(MCPSession)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)

# 依赖 log 模块 - 如果不存在，尝试添加 log 子目录（支持独立构建）
if(NOT TARGET mcp_logger)
    # 检查是否可以添加 log 子目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../log/CMakeLists.txt")
        message(STATUS "mcp_logger target not found, adding log subdirectory for standalone build")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../log ${CMAKE_BINARY_DIR}/log)
    else()
        message(FATAL_ERROR "mcp_logger target not found and log module not found. Please ensure log module is added before session module, or build from the project root directory.")
    endif()
endif()

add_library(mcp_session STATIC
    src/session_manager.cpp
)

target_include_directories(mcp_session
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
)

target_link_libraries(mcp_session
    PUBLIC
        Threads::Threads
        mcp_logger
)

add_executable(session_manager_example
    examples/session_manager_example.cpp
)

target_link_libraries(session_manager_example
    PRIVATE
        mcp_session
        mcp_logger
)

add_executable(session_manager_gtest
    test/session_manager_gtest.cpp
)

target_link_libraries(session_manager_gtest
    PRIVATE
        mcp_session
        mcp_logger
        GTest::GTest
        GTest::Main
        Threads::Threads
)

enable_testing()
add_test(
    NAME session_manager_gtest
    COMMAND session_manager_gtest
)

install(TARGETS mcp_session session_manager_example session_manager_gtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)


