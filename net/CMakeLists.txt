cmake_minimum_required(VERSION 3.10)
project(mcp_netio VERSION 1.0.0 LANGUAGES CXX)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
    ${CMAKE_CURRENT_SOURCE_DIR}/../log/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${OpenSSL_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

# 检查是否正在从项目根目录构建（根目录会先添加 log）
get_filename_component(PARENT_DIR "${CMAKE_CURRENT_SOURCE_DIR}" DIRECTORY)
if(NOT TARGET mcp_logger AND NOT (CMAKE_SOURCE_DIR STREQUAL PARENT_DIR))
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../log/CMakeLists.txt")
        # 独立构建，添加 log 子目录
        add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../log ${CMAKE_BINARY_DIR}/net_log)
    else()
        message(FATAL_ERROR "mcp_logger target not found and log module not found. Please ensure log module is added before net module, or build from the project root directory.")
    endif()
endif()

# 查找必要的包
find_package(Threads REQUIRED)
find_package(GTest REQUIRED)
#  # 注释掉，使用手动设置

# 如果找到OpenSSL，显示支持信息
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found - HTTPS support available")
else()
    message(STATUS "OpenSSL not found - HTTPS support disabled")
endif()

# 使用已存在的 logger 库，不需要重新定义

# 创建 NetIO 库
add_library(mcp_netio STATIC
    src/netio.cpp
)

# 设置库的包含目录
target_include_directories(mcp_netio PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接基础库
target_link_libraries(mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

# 创建示例程序
add_executable(netio_http_demo_client_to_3d
    examples/netio_http_demo_client_to_3d.cpp
)

add_executable(netio_https_demo_client_to_3d
    examples/netio_https_demo_client_to_3d.cpp
)

add_executable(netio_http_server_demo
    examples/netio_http_server_demo.cpp
)

add_executable(netio_https_server_demo
    examples/netio_https_server_demo.cpp
)

add_executable(netio_http_client_demo
    examples/netio_http_client_demo.cpp
)

add_executable(netio_https_client_demo
    examples/netio_https_client_demo.cpp
)

# 创建Google Test测试程序
add_executable(netio_gtest
    test/netio_gtest.cpp
)

# 设置编译定义
target_compile_definitions(mcp_netio PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(netio_http_demo_client_to_3d PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(netio_https_demo_client_to_3d PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(netio_http_server_demo PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(netio_https_server_demo PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(netio_http_client_demo PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(netio_https_client_demo PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)
target_compile_definitions(netio_gtest PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# 链接基础库
target_link_libraries(netio_http_demo_client_to_3d
    mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(netio_https_demo_client_to_3d
    mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(netio_http_server_demo
    mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(netio_https_server_demo
    mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(netio_http_client_demo
    mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(netio_https_client_demo
    mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
)

target_link_libraries(netio_gtest
    mcp_netio
    mcp_logger
    Threads::Threads
    ${OpenSSL_LIBRARIES}
    ${GTEST_LIBRARIES}
    ${GTEST_MAIN_LIBRARIES}
)


# 设置编译选项
if(MSVC)
    target_compile_options(mcp_netio PRIVATE /W4)
    target_compile_options(netio_http_demo_client_to_3d PRIVATE /W4)
    target_compile_options(netio_https_demo_client_to_3d PRIVATE /W4)
    target_compile_options(netio_http_server_demo PRIVATE /W4)
    target_compile_options(netio_https_server_demo PRIVATE /W4)
    target_compile_options(netio_http_client_demo PRIVATE /W4)
    target_compile_options(netio_https_client_demo PRIVATE /W4)
    target_compile_options(netio_gtest PRIVATE /W4)
else()
    target_compile_options(mcp_netio PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(netio_http_demo_client_to_3d PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(netio_https_demo_client_to_3d PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(netio_http_server_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(netio_https_server_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(netio_http_client_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(netio_https_client_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(netio_gtest PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 安装规则
install(TARGETS mcp_netio netio_http_demo_client_to_3d netio_https_demo_client_to_3d netio_http_server_demo netio_https_server_demo netio_http_client_demo netio_https_client_demo netio_gtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(FILES include/netio.h
    DESTINATION include
)

# 打印配置信息
message(STATUS "MCP NetIO Configuration:")
message(STATUS "  Project Name: ${PROJECT_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  Output Directory: ${CMAKE_BINARY_DIR}")
