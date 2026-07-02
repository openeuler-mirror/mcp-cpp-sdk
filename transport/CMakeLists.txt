cmake_minimum_required(VERSION 3.10)
project(mcp_transport)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# 设置输出目录 - 放到build目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
)

# 查找依赖库
find_package(Threads REQUIRED)

# 查找OpenSSL（用于HTTPS支持）
find_package(OpenSSL REQUIRED)

# 源文件
set(TRANSPORT_SOURCES
    transport.cpp
    ../log/logger.cpp
)

# 头文件
set(TRANSPORT_HEADERS
    transport.h
)

# 创建静态库
add_library(mcp_transport STATIC ${TRANSPORT_SOURCES} ${TRANSPORT_HEADERS})

# 链接库
target_link_libraries(mcp_transport
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 添加 IPC 和 NET 模块的源文件
target_sources(mcp_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/src/pipe_communication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/netio.cpp
)

# 设置包含目录
target_include_directories(mcp_transport PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 编译定义
target_compile_definitions(mcp_transport PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 创建示例程序
add_executable(transport_example transport_example.cpp)
add_executable(ipc_example ipc_example.cpp)
add_executable(sse_http_example sse_http_example.cpp)
add_executable(sse_https_example sse_https_example.cpp)

# 链接示例程序
target_link_libraries(transport_example
    mcp_transport
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(ipc_example
    mcp_transport
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(sse_http_example
    mcp_transport
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_link_libraries(sse_https_example
    mcp_transport
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 设置示例程序的包含目录
target_include_directories(transport_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_include_directories(ipc_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_include_directories(sse_http_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

target_include_directories(sse_https_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 编译定义
target_compile_definitions(transport_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(ipc_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(sse_http_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(sse_https_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 不安装 - 生成的文件直接放在当前目录

# 创建测试目标
enable_testing()

# 添加测试
add_test(NAME transport_example_config
    COMMAND ./transport_example config
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 设置测试属性
set_tests_properties(transport_example_config PROPERTIES
    TIMEOUT 30
    PASS_REGULAR_EXPRESSION "✅.*配置.*成功"
)

# 创建示例配置文件
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/transport_example.cpp
    ${CMAKE_BINARY_DIR}/transport_example.cpp
    COPYONLY
)

# 添加自定义目标来运行示例
add_custom_target(run_example
    COMMAND ./transport_example
    DEPENDS transport_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 transport 示例程序"
)

# 添加自定义目标来运行交互式测试
add_custom_target(run_interactive
    COMMAND ./transport_example interactive
    DEPENDS transport_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 transport 交互式测试"
)

# 添加自定义目标来运行 IPC 测试
add_custom_target(run_ipc_test
    COMMAND ./transport_example ipc
    DEPENDS transport_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 transport IPC 测试"
)

# 添加自定义目标来运行 HTTP 测试
add_custom_target(run_http_test
    COMMAND ./transport_example http
    DEPENDS transport_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 transport HTTP 测试"
)

# 添加自定义目标来运行 IPC 示例
add_custom_target(run_ipc_example
    COMMAND ./ipc_example
    DEPENDS ipc_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 transport IPC 示例"
)

# 添加自定义目标来运行 HTTP 示例
add_custom_target(run_http_example
    COMMAND ./sse_http_example
    DEPENDS sse_http_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 transport HTTP 示例"
)

# 添加自定义目标来运行本地示例
add_custom_target(run_local_example
    COMMAND ./sse_https_example
    DEPENDS sse_https_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 transport 本地示例"
)

# 打印配置信息
message(STATUS "MCP Transport 配置信息:")
message(STATUS "  项目名称: ${PROJECT_NAME}")
message(STATUS "  版本: 1.0.0")
message(STATUS "  C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  安装前缀: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  输出目录: ${CMAKE_BINARY_DIR} (build目录)")
message(STATUS "  包含目录:")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../net")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../common")

# 检查依赖
message(STATUS "依赖检查:")
message(STATUS "  Threads: ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")

# 平台特定设置
if(WIN32)
    message(STATUS "  平台: Windows")
    # Windows 特定设置
    target_compile_definitions(mcp_transport PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(transport_example PRIVATE WIN32_LEAN_AND_MEAN)
elseif(UNIX)
    message(STATUS "  平台: Unix/Linux")
    # Unix 特定设置
    target_compile_definitions(mcp_transport PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_definitions(transport_example PRIVATE _POSIX_C_SOURCE=200809L)
endif()

# 编译器特定设置
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "  编译器: GCC ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_transport PRIVATE -fPIC)
    target_compile_options(transport_example PRIVATE -fPIC)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "  编译器: Clang ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_transport PRIVATE -fPIC)
    target_compile_options(transport_example PRIVATE -fPIC)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(STATUS "  编译器: MSVC ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_transport PRIVATE /W4)
    target_compile_options(transport_example PRIVATE /W4)
endif()

# 不创建打包目标 - 直接使用生成的文件
