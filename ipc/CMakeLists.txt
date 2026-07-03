# CMakeLists.txt for pipe communication module

cmake_minimum_required(VERSION 3.10)
project(mcp_ipc VERSION 1.0.0 LANGUAGES CXX)

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

# 依赖 mcp_protocol 模块 - 如果不存在，尝试添加 mcp_protocol 子目录（支持独立构建）
if(NOT TARGET mcp_protocol)
    # 检查是否可以添加 mcp_protocol 子目录
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/CMakeLists.txt")
        message(STATUS "mcp_protocol target not found, adding mcp_protocol subdirectory for standalone build")
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol ${CMAKE_BINARY_DIR}/mcp_protocol)
    else()
        message(FATAL_ERROR "mcp_protocol target not found and mcp_protocol module not found. Please ensure mcp_protocol module is added before ipc module, or build from the project root directory.")
    endif()
endif()

# 源文件
set(IPC_SOURCES
    src/pipe_communication.cpp
)

# 头文件
set(IPC_HEADERS
    include/pipe_communication.h
)

# 创建共享库（改为 SHARED 以支持模块化）
add_library(mcp_ipc SHARED ${IPC_SOURCES} ${IPC_HEADERS})

# 设置库属性
set_target_properties(mcp_ipc PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 设置包含目录 - PUBLIC 会传播给链接此库的目标
target_include_directories(mcp_ipc PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../common>
    $<INSTALL_INTERFACE:include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include>
    $<INSTALL_INTERFACE:include>
)

# PRIVATE 包含目录 - 仅用于编译此库
target_include_directories(mcp_ipc PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 链接依赖库
target_link_libraries(mcp_ipc
    PUBLIC
        mcp_logger
        mcp_protocol
        Threads::Threads
)

# 平台特定设置
if(WIN32)
    target_link_libraries(mcp_ipc PRIVATE kernel32)
endif()

# 编译定义
target_compile_definitions(mcp_ipc PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 创建示例可执行文件
add_executable(pipe_communication_example examples/pipe_communication_example.cpp)
target_link_libraries(pipe_communication_example mcp_ipc mcp_protocol)
target_include_directories(pipe_communication_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
)

# 双模式IPC示例程序
add_executable(ipc_server_example examples/ipc_server_example.cpp)
target_link_libraries(ipc_server_example mcp_ipc mcp_protocol)

add_executable(ipc_client_example examples/ipc_client_example.cpp)
target_link_libraries(ipc_client_example mcp_ipc mcp_protocol)

add_executable(ipc_dual_mode_example examples/ipc_dual_mode_example.cpp)
target_link_libraries(ipc_dual_mode_example mcp_ipc mcp_protocol)

# 设置示例程序的包含目录
target_include_directories(ipc_server_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
)

target_include_directories(ipc_client_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
)

target_include_directories(ipc_dual_mode_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
)

# 安装规则
install(TARGETS mcp_ipc
    EXPORT mcp_ipcTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(TARGETS pipe_communication_example ipc_server_example ipc_client_example ipc_dual_mode_example
    RUNTIME DESTINATION bin
)

install(FILES ${IPC_HEADERS}
    DESTINATION include/mcp/ipc
)

# 测试
enable_testing()

# 添加Google Test支持
find_package(GTest QUIET)
if(GTest_FOUND)
    # 创建Google Test测试可执行文件
    add_executable(pipe_communication_gtest
        test/pipe_communication_gtest.cpp
    )
    
    set_target_properties(pipe_communication_gtest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    target_link_libraries(pipe_communication_gtest
        mcp_ipc
        GTest::GTest
        GTest::Main
    )
    
    target_include_directories(pipe_communication_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../common
    )
    
    target_compile_definitions(pipe_communication_gtest PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )
    
    add_test(NAME pipe_communication_gtest
        COMMAND pipe_communication_gtest
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    set_tests_properties(pipe_communication_gtest PROPERTIES
        TIMEOUT 60
    )
endif()

# 添加基本测试
add_executable(pipe_communication_test test/pipe_communication_test.cpp)
target_link_libraries(pipe_communication_test mcp_ipc)
set_target_properties(pipe_communication_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME pipe_communication_test 
    COMMAND pipe_communication_test
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
