cmake_minimum_required(VERSION 3.10)
project(mcp_client VERSION 1.0.0 LANGUAGES CXX)

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
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${OpenSSL_INCLUDE_DIRS}
)

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

# 查找依赖库
find_package(Threads REQUIRED)
# find_package(OpenSSL REQUIRED)  # 注释掉，使用手动设置

# 添加 session 子目录（如果还没有添加）
if(NOT TARGET mcp_session)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../session ${CMAKE_BINARY_DIR}/session)
endif()

# 源文件
set(CLIENT_SOURCES
    src/mcp_client.cpp
    ../mcp_protocol/src/mcp_protocol_core.cpp
    ../log/src/logger.cpp
)

# 头文件
set(CLIENT_HEADERS
    include/mcp_client.h
    ../mcp_protocol/include/mcp_protocol_core.h
    ../mcp_protocol/include/mcp_message.h
)

# 创建动态库
add_library(mcp_client SHARED ${CLIENT_SOURCES} ${CLIENT_HEADERS})

# 设置库属性
set_target_properties(mcp_client PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 链接依赖库
target_link_libraries(mcp_client
    PRIVATE
        mcp_transport
        mcp_session
        Threads::Threads
        ${OpenSSL_LIBRARIES}
)

# 添加 transport 子目录（如果还没有添加）
if(NOT TARGET mcp_transport)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../transport ${CMAKE_BINARY_DIR}/transport)
endif()

# 确保依赖库在 mcp_client 之前构建
add_dependencies(mcp_client mcp_transport mcp_session)

# 添加其他模块的源文件（直接包含）
target_sources(mcp_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/src/pipe_communication.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/src/netio.cpp
)

# 设置包含目录 - 使用PRIVATE而不是PUBLIC避免INTERFACE_INCLUDE_DIRECTORIES问题
target_include_directories(mcp_client PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${OpenSSL_INCLUDE_DIRS}
)

# 为使用该库的代码设置PUBLIC包含目录（使用相对路径）
target_include_directories(mcp_client PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp_client>
)

# 编译定义
target_compile_definitions(mcp_client PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 创建示例程序
add_executable(mcp_client_ipc_demo examples/mcp_client_ipc_demo.cpp)
add_executable(mcp_client_ipc_parent_child examples/mcp_client_ipc_parent_child.cpp)
add_executable(mcp_client_http_demo examples/mcp_client_http_demo.cpp)
add_executable(mcp_client_https_demo examples/mcp_client_https_demo.cpp)

# 设置示例程序输出目录
set_target_properties(mcp_client_ipc_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(mcp_client_ipc_parent_child PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(mcp_client_http_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)
set_target_properties(mcp_client_https_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 链接示例程序
target_link_libraries(mcp_client_ipc_demo
    mcp_client
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(mcp_client_http_demo
    mcp_client
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(mcp_client_https_demo
    mcp_client
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(mcp_client_ipc_parent_child
    mcp_client
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

# 设置示例程序的包含目录
target_include_directories(mcp_client_ipc_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(mcp_client_http_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(mcp_client_https_demo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(mcp_client_ipc_parent_child PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${OpenSSL_INCLUDE_DIRS}
)

# 编译定义
target_compile_definitions(mcp_client_ipc_demo PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(mcp_client_http_demo PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(mcp_client_https_demo PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

target_compile_definitions(mcp_client_ipc_parent_child PRIVATE
    CPPHTTPLIB_OPENSSL_SUPPORT
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
)

# 安装规则
install(TARGETS mcp_client
    EXPORT mcp_clientTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/mcp_client
)

install(TARGETS mcp_client_ipc_demo mcp_client_ipc_parent_child mcp_client_http_demo mcp_client_https_demo
    RUNTIME DESTINATION bin
)

# 创建配置文件
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_clientConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# 创建简单的配置文件
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mcp_clientConfig.cmake"
"@PACKAGE_INIT@
include(\"\${CMAKE_CURRENT_LIST_DIR}/mcp_clientTargets.cmake\")
check_required_components(mcp_client)
")

# 安装配置文件
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_clientConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_clientConfigVersion.cmake"
    DESTINATION lib/cmake/mcp_client
)

# 导出目标
install(EXPORT mcp_clientTargets
    FILE mcp_clientTargets.cmake
    NAMESPACE mcp_client::
    DESTINATION lib/cmake/mcp_client
)

# 创建测试目标
enable_testing()

# 添加Google Test支持
find_package(GTest REQUIRED)
if(GTest_FOUND)
    message(STATUS "Google Test found: ${GTEST_INCLUDE_DIRS}")
    
    # 创建Google Test测试可执行文件
    add_executable(mcp_client_gtest
        test/mcp_client_gtest.cpp
    )
    
    
    # 设置测试可执行文件输出目录
    set_target_properties(mcp_client_gtest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    
    # 链接测试依赖
    target_link_libraries(mcp_client_gtest
        mcp_client
        GTest::GTest
        GTest::Main
        Threads::Threads
        ${OpenSSL_LIBRARIES}
    )
    
    
# 设置测试包含目录
target_include_directories(mcp_client_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${GTEST_INCLUDE_DIRS}
)
    
    # 编译定义
    target_compile_definitions(mcp_client_gtest PRIVATE
        CPPHTTPLIB_OPENSSL_SUPPORT
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )
    
    # 添加Google Test测试
    add_test(NAME mcp_client_gtest
        COMMAND ./mcp_client_gtest
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    
    # 设置测试属性
    set_tests_properties(mcp_client_gtest PROPERTIES
        TIMEOUT 60
    )
    
    
    message(STATUS "Google Test executable created: mcp_client_gtest")
else()
    message(WARNING "Google Test not found. Skipping test compilation.")
endif()

# 添加测试
add_test(NAME mcp_client_ipc_demo_test
    COMMAND ./mcp_client_ipc_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME mcp_client_http_demo_test
    COMMAND ./mcp_client_http_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

add_test(NAME mcp_client_https_demo_test
    COMMAND ./mcp_client_https_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 设置测试属性
set_tests_properties(mcp_client_ipc_demo_test PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION "MCP Client IPC 通信测试完成"
)

set_tests_properties(mcp_client_http_demo_test PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION "MCP Client HTTP/SSE 通信测试完成"
)

set_tests_properties(mcp_client_https_demo_test PROPERTIES
    TIMEOUT 60
    PASS_REGULAR_EXPRESSION "MCP Client HTTP/SSE 通信测试完成"
)

# 添加自定义目标来运行示例
add_custom_target(run_mcp_client_ipc_demo
    COMMAND ./mcp_client_ipc_demo
    DEPENDS mcp_client_ipc_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "运行 MCP Client IPC 演示程序"
)

add_custom_target(run_mcp_client_http_demo
    COMMAND ./mcp_client_http_demo
    DEPENDS mcp_client_http_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "运行 MCP Client HTTP/SSE 演示程序"
)

add_custom_target(run_mcp_client_https_demo
    COMMAND ./mcp_client_https_demo
    DEPENDS mcp_client_https_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "运行 MCP Client HTTPS/SSE 演示程序"
)

add_custom_target(run_mcp_client_ipc_parent_child
    COMMAND ./mcp_client_ipc_parent_child
    DEPENDS mcp_client_ipc_parent_child
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "运行 MCP Client IPC 父子进程通信演示程序"
)

# 添加自定义目标来运行测试
add_custom_target(run_client_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS mcp_client_ipc_demo mcp_client_http_demo mcp_client_https_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行 MCP Client 测试"
)

# 添加运行Google Test的自定义目标
if(GTest_FOUND)
    add_custom_target(run_gtest
        COMMAND ./mcp_client_gtest
        DEPENDS mcp_client_gtest
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
        COMMENT "运行 MCP Client Google Test 测试"
    )
    
    add_custom_target(run_all_client_tests
        COMMAND ${CMAKE_CTEST_COMMAND} --verbose
        DEPENDS mcp_client_ipc_demo mcp_client_http_demo mcp_client_https_demo mcp_client_gtest
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "运行所有 MCP Client 测试（包括Google Test）"
    )
endif()

# 打印配置信息
message(STATUS "MCP Client 配置信息:")
message(STATUS "  项目名称: ${PROJECT_NAME}")
message(STATUS "  版本: ${PROJECT_VERSION}")
message(STATUS "  C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  安装前缀: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  输出目录: ${CMAKE_BINARY_DIR}")
message(STATUS "  包含目录:")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../transport")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../log")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../net")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../common")

# 依赖检查
message(STATUS "依赖检查:")
message(STATUS "  Threads: ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")

# 平台特定设置
if(WIN32)
    message(STATUS "  平台: Windows")
    target_compile_definitions(mcp_client PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(mcp_client_ipc_demo PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(mcp_client_ipc_parent_child PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(mcp_client_http_demo PRIVATE WIN32_LEAN_AND_MEAN)
    target_compile_definitions(mcp_client_https_demo PRIVATE WIN32_LEAN_AND_MEAN)
elseif(UNIX)
    message(STATUS "  平台: Unix/Linux")
    target_compile_definitions(mcp_client PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_definitions(mcp_client_ipc_demo PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_definitions(mcp_client_ipc_parent_child PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_definitions(mcp_client_http_demo PRIVATE _POSIX_C_SOURCE=200809L)
    target_compile_definitions(mcp_client_https_demo PRIVATE _POSIX_C_SOURCE=200809L)
endif()

# 编译器特定设置
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    message(STATUS "  编译器: GCC ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_client PRIVATE -fPIC)
    target_compile_options(mcp_client_ipc_demo PRIVATE -fPIC)
    target_compile_options(mcp_client_ipc_parent_child PRIVATE -fPIC)
    target_compile_options(mcp_client_http_demo PRIVATE -fPIC)
    target_compile_options(mcp_client_https_demo PRIVATE -fPIC)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    message(STATUS "  编译器: Clang ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_client PRIVATE -fPIC)
    target_compile_options(mcp_client_ipc_demo PRIVATE -fPIC)
    target_compile_options(mcp_client_ipc_parent_child PRIVATE -fPIC)
    target_compile_options(mcp_client_http_demo PRIVATE -fPIC)
    target_compile_options(mcp_client_https_demo PRIVATE -fPIC)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    message(STATUS "  编译器: MSVC ${CMAKE_CXX_COMPILER_VERSION}")
    target_compile_options(mcp_client PRIVATE /W4)
    target_compile_options(mcp_client_ipc_demo PRIVATE /W4)
    target_compile_options(mcp_client_ipc_parent_child PRIVATE /W4)
    target_compile_options(mcp_client_http_demo PRIVATE /W4)
    target_compile_options(mcp_client_https_demo PRIVATE /W4)
endif()

# 创建打包目标
set(CPACK_PACKAGE_NAME "mcp_client")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MCP Client Library with Transport Integration")
set(CPACK_PACKAGE_VENDOR "MCP SDK")
set(CPACK_PACKAGE_CONTACT "support@mcp-sdk.com")

include(CPack)
