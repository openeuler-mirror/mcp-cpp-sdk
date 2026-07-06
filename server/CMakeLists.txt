cmake_minimum_required(VERSION 3.10)
project(mcp_server VERSION 1.0.0 LANGUAGES CXX)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session
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

# 查找依赖库
find_package(Threads REQUIRED)
# find_package(OpenSSL REQUIRED)  # 注释掉，使用手动设置

# 添加 session 子目录（如果还没有添加）
if(NOT TARGET mcp_session)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../session ${CMAKE_BINARY_DIR}/session)
endif()

# 添加 transport 子目录（如果还没有添加）
if(NOT TARGET mcp_transport)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../transport ${CMAKE_BINARY_DIR}/transport)
endif()

# 添加 client 子目录（如果还没有添加，用于编译客户端示例）
if(NOT TARGET mcp_client)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../client ${CMAKE_BINARY_DIR}/client)
endif()

# 源文件
set(SERVER_SOURCES
    src/mcp_server.cpp
    src/mcp_filesystem_server.cpp
    ../mcp_protocol/src/mcp_protocol_core.cpp
    ../log/src/logger.cpp
    ../ipc/src/pipe_communication.cpp
    ../net/src/netio.cpp
)

# 头文件
set(SERVER_HEADERS
    include/mcp_server.h
    include/mcp_filesystem_server.h
    include/mcp_protocol.h
    ../mcp_protocol/include/mcp_protocol_core.h
    ../mcp_protocol/include/mcp_message.h
)

# 创建动态库
add_library(mcp_server SHARED ${SERVER_SOURCES} ${SERVER_HEADERS})

# 设置库属性
set_target_properties(mcp_server PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 链接依赖库
target_link_libraries(mcp_server
    PRIVATE
        mcp_transport
        mcp_session
        Threads::Threads
        ${OpenSSL_LIBRARIES}
)

# 确保依赖库在 mcp_server 之前构建
add_dependencies(mcp_server mcp_transport mcp_session)

# 设置库的包含目录
target_include_directories(mcp_server PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/mcp_sdk/server>
)

# 设置库的私有包含目录（包括OpenSSL）
target_include_directories(mcp_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session
    ${OpenSSL_INCLUDE_DIRS}
)

# 编译定义
target_compile_definitions(mcp_server PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:NDEBUG>
    CPPHTTPLIB_OPENSSL_SUPPORT
)

# 创建示例程序
add_executable(mcp_filesystem_server_http_example examples/mcp_filesystem_server_http_example.cpp)
add_executable(mcp_filesystem_server_ipc_example examples/mcp_filesystem_server_ipc_example.cpp)
add_executable(mcp_filesystem_client_http_example examples/mcp_filesystem_client_http_example.cpp)
add_executable(mcp_filesystem_server_https_example examples/mcp_filesystem_server_https_example.cpp)
add_executable(mcp_filesystem_client_https_example examples/mcp_filesystem_client_https_example.cpp)

# 确保 mcp_client 在客户端示例之前构建
add_dependencies(mcp_filesystem_client_http_example mcp_client)
add_dependencies(mcp_filesystem_client_https_example mcp_client)

# 添加Google Test支持
find_package(GTest QUIET)
if(GTest_FOUND)
    message(STATUS "Google Test found: ${GTEST_INCLUDE_DIRS}")
    
    # 创建Google Test测试可执行文件
    add_executable(mcp_server_gtest
        test/mcp_server_gtest.cpp
    )
    
    # 设置测试可执行文件输出目录
    set_target_properties(mcp_server_gtest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # 链接测试依赖
    target_link_libraries(mcp_server_gtest
        mcp_server
        GTest::GTest
        GTest::Main
        Threads::Threads
        ${OpenSSL_LIBRARIES}
    )
    
    # 设置测试包含目录
    target_include_directories(mcp_server_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
        ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../log
        ${CMAKE_CURRENT_SOURCE_DIR}/../common
        ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../transport
        ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
        ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../net
        ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../session
        ${GTEST_INCLUDE_DIRS}
        ${OpenSSL_INCLUDE_DIRS}
    )
    
    # 编译定义
    target_compile_definitions(mcp_server_gtest PRIVATE
        CPPHTTPLIB_OPENSSL_SUPPORT
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
    )
    
    # 启用测试
    enable_testing()
    
    # 添加Google Test测试
    add_test(NAME mcp_server_gtest
        COMMAND ./mcp_server_gtest
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # 设置测试属性
    set_tests_properties(mcp_server_gtest PROPERTIES
        TIMEOUT 60
    )
    
    message(STATUS "Google Test executable created: mcp_server_gtest")
else()
    message(WARNING "Google Test not found. Skipping test compilation.")
endif()

# 为所有可执行文件添加OpenSSL支持编译定义
target_compile_definitions(mcp_filesystem_server_http_example PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(mcp_filesystem_server_ipc_example PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(mcp_filesystem_client_http_example PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(mcp_filesystem_server_https_example PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(mcp_filesystem_client_https_example PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
# 链接示例程序
target_link_libraries(mcp_filesystem_server_http_example
    mcp_server
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(mcp_filesystem_server_ipc_example
    mcp_server
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(mcp_filesystem_client_http_example
    mcp_client
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(mcp_filesystem_server_https_example
    mcp_server
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(mcp_filesystem_client_https_example
    mcp_client
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

# 设置示例程序和测试程序的包含目录
target_include_directories(mcp_filesystem_server_http_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(mcp_filesystem_server_ipc_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(mcp_filesystem_client_http_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../client
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(mcp_filesystem_server_https_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${CMAKE_CURRENT_SOURCE_DIR}/../session/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../session
    ${OpenSSL_INCLUDE_DIRS}
)

target_include_directories(mcp_filesystem_client_https_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../client/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../client
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../mcp_protocol
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../ipc
    ${CMAKE_CURRENT_SOURCE_DIR}/../net/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../net
    ${OpenSSL_INCLUDE_DIRS}
)

# 安装规则
install(TARGETS mcp_server mcp_filesystem_server_http_example mcp_filesystem_server_ipc_example mcp_filesystem_client_http_example mcp_filesystem_server_https_example mcp_filesystem_client_https_example
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/mcp_server
)

# 安装 gtest 可执行文件（如果存在）
if(GTest_FOUND)
    install(TARGETS mcp_server_gtest
        RUNTIME DESTINATION bin
    )
endif()

install(FILES include/mcp_server.h include/mcp_filesystem_server.h include/mcp_protocol.h
    DESTINATION include
)

# 打印配置信息
message(STATUS "MCP Server Configuration:")
message(STATUS "  Project Name: ${PROJECT_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Output Directory: ${CMAKE_BINARY_DIR}")
