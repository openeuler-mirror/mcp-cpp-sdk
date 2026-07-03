cmake_minimum_required(VERSION 3.10)
project(mcp_transport)

if (NOT TARGET mcp_session)
    add_subdirectory(
        ${CMAKE_CURRENT_SOURCE_DIR}/../session
        ${CMAKE_CURRENT_BINARY_DIR}/session_build
    )
endif()

# 设置默认构建类型为 Debug (如果未设置)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 设置 C++ 标准
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

# 设置OpenSSL路径 - 支持多平台
if(APPLE)
    # macOS (Homebrew)
    set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@3")
    set(OPENSSL_INCLUDE_DIR "/usr/local/opt/openssl@3/include")
    set(OPENSSL_LIBRARIES "/usr/local/opt/openssl@3/lib")
    set(OPENSSL_CRYPTO_LIBRARY "/usr/local/opt/openssl@3/lib/libcrypto.dylib")
    set(OPENSSL_SSL_LIBRARY "/usr/local/opt/openssl@3/lib/libssl.dylib")
    set(OPENSSL_LIBRARY_SUFFIX ".dylib")
elseif(UNIX AND NOT APPLE)
    # Linux (Ubuntu/Debian)
    set(OPENSSL_ROOT_DIR "/usr")
    set(OPENSSL_INCLUDE_DIR "/usr/include/openssl")
    set(OPENSSL_LIBRARIES "/usr/lib/x86_64-linux-gnu")
    set(OPENSSL_CRYPTO_LIBRARY "/usr/lib/x86_64-linux-gnu/libcrypto.so")
    set(OPENSSL_SSL_LIBRARY "/usr/lib/x86_64-linux-gnu/libssl.so")
    set(OPENSSL_LIBRARY_SUFFIX ".so")
elseif(WIN32)
    # Windows
    set(OPENSSL_ROOT_DIR "C:/OpenSSL")
    set(OPENSSL_INCLUDE_DIR "C:/OpenSSL/include")
    set(OPENSSL_LIBRARIES "C:/OpenSSL/lib")
    set(OPENSSL_CRYPTO_LIBRARY "C:/OpenSSL/lib/libcrypto.lib")
    set(OPENSSL_SSL_LIBRARY "C:/OpenSSL/lib/libssl.lib")
    set(OPENSSL_LIBRARY_SUFFIX ".lib")
endif()

# 手动设置OpenSSL变量
set(OpenSSL_FOUND TRUE)
if(APPLE)
    #find_package(OpenSSL REQUIRED) 
    set(OpenSSL_VERSION "3.5.0")
    set(OpenSSL_INCLUDE_DIRS "/usr/local/opt/openssl@3/include")
    set(OpenSSL_LIBRARIES "/usr/local/opt/openssl@3/lib/libssl.dylib;/usr/local/opt/openssl@3/lib/libcrypto.dylib")
elseif(UNIX AND NOT APPLE)
    set(OpenSSL_VERSION "3.0.2")
    set(OpenSSL_INCLUDE_DIRS "/usr/local/include/openssl")
    set(OpenSSL_LIBRARIES "/usr/local/lib64/libssl.so;/usr/local/lib64/libcrypto.so")
elseif(WIN32)
    set(OpenSSL_VERSION "3.0.0")
    set(OpenSSL_INCLUDE_DIRS "C:/OpenSSL/include")
    set(OpenSSL_LIBRARIES "C:/OpenSSL/lib/libssl.lib;C:/OpenSSL/lib/libcrypto.lib")
endif()

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

# 查找依赖库
find_package(Threads REQUIRED)
#  # 注释掉，使用手动设置

# 查找Google Test
find_package(GTest REQUIRED)

# 源文件
set(TRANSPORT_SOURCES
    src/transport_base.cpp
    src/transport_factory.cpp
    src/stdio_transport.cpp
    src/http_transport.cpp
    src/https_transport.cpp
    ../log/src/logger.cpp
)

# 头文件
set(TRANSPORT_HEADERS
    include/transport_base.h
    include/transport_factory.h
    include/stdio_transport.h
    include/http_transport.h
    include/https_transport.h
)

# 创建静态库
add_library(mcp_transport STATIC ${TRANSPORT_SOURCES} ${TRANSPORT_HEADERS})

# 链接库
target_link_libraries(mcp_transport
    PUBLIC
        mcp_session
    PRIVATE
        Threads::Threads
        ${OpenSSL_LIBRARIES}
)

# 添加 IPC 和 NET 模块的源文件
target_sources(mcp_transport PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/src/pipe_communication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/src/netio.cpp
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
# add_executable(transport_example examples/transport_example.cpp)  # 文件不存在，已注释
# add_executable(transport_client_example examples/transport_client_example.cpp)  # 文件不存在，已注释
# add_executable(transport_server_example examples/transport_server_example.cpp)  # 文件不存在，已注释
# add_executable(transport_server_client_example examples/transport_server_client_example.cpp)  # 文件不存在，已注释
# add_executable(transport_dual_mode_example examples/transport_dual_mode_example.cpp)  # 文件不存在，已注释
# add_executable(ipc_example examples/ipc_example.cpp)  # 文件不存在，已注释
# add_executable(sse_http_example examples/sse_http_example.cpp)  # 文件不存在，已注释
# add_executable(sse_https_example examples/sse_https_example.cpp)  # 文件不存在，已注释
add_executable(http_client_example examples/http_client_example.cpp)
add_executable(http_server_example examples/http_server_example.cpp)
add_executable(https_client_example examples/https_client_example.cpp)
add_executable(https_server_example examples/https_server_example.cpp)
add_executable(stdio_client_example examples/stdio_client_example.cpp)
add_executable(stdio_server_example examples/stdio_server_example.cpp)
add_executable(http_example_to_3d examples/http_example_to_3d.cpp)
add_executable(https_example_to_3d examples/https_example_to_3d.cpp)

# 创建 gtest 测试程序
add_executable(http_transport_gtest test/http_transport_gtest.cpp)
add_executable(https_transport_gtest test/https_transport_gtest.cpp)
add_executable(stdio_transport_gtest test/stdio_transport_gtest.cpp)

# 链接示例程序
# target_link_libraries(transport_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

# target_link_libraries(ipc_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

# target_link_libraries(sse_http_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

# target_link_libraries(sse_https_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

# target_link_libraries(transport_client_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

# target_link_libraries(transport_server_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

# target_link_libraries(transport_server_client_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

# target_link_libraries(transport_dual_mode_example  # 目标不存在，已注释
#     mcp_transport
#     Threads::Threads
#     ${OpenSSL_LIBRARIES}
# )

target_link_libraries(http_client_example
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(http_server_example
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(https_client_example
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(https_server_example
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(stdio_client_example
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(stdio_server_example
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(http_example_to_3d
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(https_example_to_3d
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(http_transport_gtest
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
    GTest::GTest
    GTest::Main
)

target_link_libraries(https_transport_gtest
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
    GTest::GTest
    GTest::Main
)

target_link_libraries(stdio_transport_gtest
    mcp_transport
    Threads::Threads
    ${OpenSSL_LIBRARIES}
    GTest::GTest
    GTest::Main
)

# 设置示例程序和测试程序的包含目录
# target_include_directories(transport_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

# target_include_directories(ipc_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

# target_include_directories(sse_http_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

# target_include_directories(sse_https_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

target_include_directories(http_client_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(http_server_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(https_client_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(https_server_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(stdio_client_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(stdio_server_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(http_example_to_3d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(https_example_to_3d PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(http_transport_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(https_transport_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(stdio_transport_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
)

# target_include_directories(transport_gtest PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${CMAKE_CURRENT_SOURCE_DIR}/../3thirdparty/googletest/googletest/include
# )

# target_include_directories(transport_client_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

# target_include_directories(transport_server_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

# target_include_directories(transport_server_client_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

# target_include_directories(transport_dual_mode_example PRIVATE  # 目标不存在，已注释
#     ${CMAKE_CURRENT_SOURCE_DIR}/include
#     ${CMAKE_CURRENT_SOURCE_DIR}
#     ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../net
#     ${CMAKE_CURRENT_SOURCE_DIR}/../common
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
#     ${CMAKE_CURRENT_SOURCE_DIR}/../log
#     ${OpenSSL_INCLUDE_DIRS}
# )

# 编译定义
# target_compile_definitions(transport_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

# target_compile_definitions(ipc_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

# target_compile_definitions(sse_http_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

# target_compile_definitions(sse_https_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

target_compile_definitions(http_client_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(http_server_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(https_client_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(https_server_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(stdio_client_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(stdio_server_example PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(http_example_to_3d PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(https_example_to_3d PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(http_transport_gtest PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(https_transport_gtest PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(stdio_transport_gtest PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# target_compile_definitions(transport_client_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

# target_compile_definitions(transport_server_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

# target_compile_definitions(transport_server_client_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

# target_compile_definitions(transport_dual_mode_example PRIVATE  # 目标不存在，已注释
#     CPPHTTPLIB_OPENSSL_SUPPORT
#     $<$<CONFIG:Debug>:DEBUG>
#     $<$<CONFIG:Release>:NDEBUG>
# )

# 不安装 - 生成的文件直接放在当前目录

# 创建测试目标
enable_testing()

# 添加测试
# add_test(NAME transport_example_config  # 目标不存在，已注释
#     COMMAND ./transport_example config
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
# )

# 添加 gtest 测试
add_test(NAME http_transport_gtest
    COMMAND ./bin/http_transport_gtest
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME https_transport_gtest
    COMMAND ./bin/https_transport_gtest
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_test(NAME stdio_transport_gtest
    COMMAND ./bin/stdio_transport_gtest
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

# 设置测试属性
set_tests_properties(http_transport_gtest PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION ".*PASSED.*"
)

set_tests_properties(https_transport_gtest PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION ".*PASSED.*"
)

set_tests_properties(stdio_transport_gtest PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION ".*PASSED.*"
)

# 创建示例配置文件（如果文件存在）
# configure_file(
#     ${CMAKE_CURRENT_SOURCE_DIR}/examples/transport_example.cpp
#     ${CMAKE_BINARY_DIR}/transport_example.cpp
#     COPYONLY
# )

# 添加自定义目标来运行示例
# add_custom_target(run_transport_example  # 目标不存在，已注释
#     COMMAND ./transport_example
#     DEPENDS transport_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 示例程序"
# )

# 添加自定义目标来运行交互式测试
# add_custom_target(run_interactive  # 目标不存在，已注释
#     COMMAND ./transport_example interactive
#     DEPENDS transport_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 交互式测试"
# )

# 添加自定义目标来运行 IPC 测试
# add_custom_target(run_ipc_test  # 目标不存在，已注释
#     COMMAND ./transport_example ipc
#     DEPENDS transport_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport IPC 测试"
# )

# 添加自定义目标来运行 HTTP 测试
# add_custom_target(run_http_test  # 目标不存在，已注释
#     COMMAND ./transport_example http
#     DEPENDS transport_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport HTTP 测试"
# )

# 添加自定义目标来运行 IPC 示例
# add_custom_target(run_ipc_example  # 目标不存在，已注释
#     COMMAND ./ipc_example
#     DEPENDS ipc_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport IPC 示例"
# )

# 添加自定义目标来运行 HTTP 示例
# add_custom_target(run_http_example  # 目标不存在，已注释
#     COMMAND ./sse_http_example
#     DEPENDS sse_http_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport HTTP 示例"
# )

# 添加自定义目标来运行本地示例
# add_custom_target(run_local_example  # 目标不存在，已注释
#     COMMAND ./sse_https_example
#     DEPENDS sse_https_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 本地示例"
# )

# 添加自定义目标来运行客户端示例
# add_custom_target(run_client_example  # 目标不存在，已注释
#     COMMAND ./transport_client_example
#     DEPENDS transport_client_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 客户端示例"
# )

# 添加自定义目标来运行服务端示例
# add_custom_target(run_server_example  # 目标不存在，已注释
#     COMMAND ./transport_server_example
#     DEPENDS transport_server_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 服务端示例"
# )

# add_custom_target(run_server_client_example  # 目标不存在，已注释
#     COMMAND ./transport_server_client_example
#     DEPENDS transport_server_client_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 服务端客户端示例"
# )

# 添加自定义目标来运行双模式示例
# add_custom_target(run_dual_mode_example  # 目标不存在，已注释
#     COMMAND ./transport_dual_mode_example --server
#     DEPENDS transport_dual_mode_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 双模式示例（服务端）"
# )

# 添加自定义目标来运行双模式客户端示例
# add_custom_target(run_dual_mode_client_example  # 目标不存在，已注释
#     COMMAND ./transport_dual_mode_example --client ./transport_dual_mode_example
#     DEPENDS transport_dual_mode_example
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport 双模式示例（客户端）"
# )

# 添加自定义目标来运行 gtest 测试
# add_custom_target(run_transport_gtest  # 目标不存在，已注释
#     COMMAND ./bin/transport_gtest
#     DEPENDS transport_gtest
#     WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
#     COMMENT "运行 transport gtest 单元测试"
# )

# 添加自定义目标来运行所有测试
add_custom_target(run_transport_all_tests
    COMMAND ./bin/http_transport_gtest && ./bin/https_transport_gtest && ./bin/stdio_transport_gtest
    DEPENDS http_transport_gtest https_transport_gtest stdio_transport_gtest
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行所有 transport 测试"
)

# 添加自定义目标来编译所有示例
add_custom_target(build_transport_all_examples
    DEPENDS http_client_example
             http_server_example
             https_client_example
             https_server_example
             stdio_client_example
             stdio_server_example
             http_example_to_3d
             https_example_to_3d
    COMMENT "编译所有 transport 示例程序"
)

# 添加自定义目标来编译所有测试
add_custom_target(build_transport_all_tests
    DEPENDS http_transport_gtest
             https_transport_gtest
             stdio_transport_gtest
    COMMENT "编译所有 transport 测试程序"
)

# 添加自定义目标来编译所有目标
add_custom_target(build_transport_all
    DEPENDS mcp_transport build_transport_all_examples build_transport_all_tests
    COMMENT "编译 transport 模块的所有目标"
)

# 添加自定义目标来运行所有示例
add_custom_target(run_transport_all_examples
    COMMAND echo "=== Running Transport Examples ===" &&
            echo "1. Running stdio server example..." &&
            ./bin/stdio_server_example &
            STDIO_SERVER_PID=$! &&
            sleep 2 &&
            kill $STDIO_SERVER_PID 2>/dev/null || true &&
            echo "2. Running stdio client example..." &&
            ./bin/stdio_client_example &
            STDIO_CLIENT_PID=$! &&
            sleep 2 &&
            kill $STDIO_CLIENT_PID 2>/dev/null || true &&
            echo "3. Running HTTP server example..." &&
            ./bin/http_server_example &
            HTTP_SERVER_PID=$! &&
            sleep 2 &&
            kill $HTTP_SERVER_PID 2>/dev/null || true &&
            echo "4. Running HTTP client example..." &&
            ./bin/http_client_example &
            HTTP_CLIENT_PID=$! &&
            sleep 2 &&
            kill $HTTP_CLIENT_PID 2>/dev/null || true &&
            echo "=== All examples completed ==="
    DEPENDS build_transport_all_examples
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行所有 transport 示例程序"
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
message(STATUS "  可用目标:")
message(STATUS "    - build_all: 编译所有目标")
message(STATUS "    - build_all_examples: 编译所有示例")
message(STATUS "    - build_all_tests: 编译所有测试")
message(STATUS "    - run_all_examples: 运行所有示例")
message(STATUS "    - run_all_tests: 运行所有测试")
message(STATUS "  示例程序:")
message(STATUS "    - http_client_example: HTTP客户端示例")
message(STATUS "    - http_server_example: HTTP服务端示例")
message(STATUS "    - https_client_example: HTTPS客户端示例")
message(STATUS "    - https_server_example: HTTPS服务端示例")
message(STATUS "    - stdio_client_example: Stdio客户端示例")
message(STATUS "    - stdio_server_example: Stdio服务端示例")
message(STATUS "    - http_example_to_3d: HTTP到3D示例")
message(STATUS "    - https_example_to_3d: HTTPS到3D示例")

# 检查依赖
message(STATUS "依赖检查:")
message(STATUS "  Threads: ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")

# 平台特定设置
if(WIN32)
    message(STATUS "  平台: Windows")
    # Windows 特定设置
    target_compile_definitions(mcp_transport PRIVATE WIN32_LEAN_AND_MEAN)
    # target_compile_definitions(transport_example PRIVATE WIN32_LEAN_AND_MEAN)  # 目标不存在，已注释
elseif(UNIX)
    message(STATUS "  平台: Unix/Linux")
    # Unix 特定设置
    target_compile_definitions(mcp_transport PRIVATE _POSIX_C_SOURCE=200809L)
    # target_compile_definitions(transport_example PRIVATE _POSIX_C_SOURCE=200809L)  # 目标不存在，已注释
endif()

# 编译器特定设置
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "  编译器: GCC ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_transport PRIVATE -fPIC)
    # target_compile_options(transport_example PRIVATE -fPIC)  # 目标不存在，已注释
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "  编译器: Clang ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_transport PRIVATE -fPIC)
    # target_compile_options(transport_example PRIVATE -fPIC)  # 目标不存在，已注释
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(STATUS "  编译器: MSVC ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_transport PRIVATE /W4)
    # target_compile_options(transport_example PRIVATE /W4)  # 目标不存在，已注释
endif()

# 不创建打包目标 - 直接使用生成的文件
