cmake_minimum_required(VERSION 3.10)
project(mcp_sdk VERSION 1.0.0 LANGUAGES CXX)

# 启用生成 compile_commands.json，用于IDE代码跳转和智能提示
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 全局调试开关 - 默认开启
option(ENABLE_DEBUG_BUILD "Enable debug build with debug symbols" ON)

# 设置默认构建类型为 Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    if(ENABLE_DEBUG_BUILD)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zi /Od")
    endif()
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -fPIC")
    if(ENABLE_DEBUG_BUILD)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -O0")
    endif()
endif()

message(STATUS "MCP SDK 调试配置:")
message(STATUS "  ENABLE_DEBUG_BUILD: ${ENABLE_DEBUG_BUILD}")
message(STATUS "  CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")
message(STATUS "  CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 设置OpenSSL路径 - 支持多平台
if(APPLE)
    # macOS (Homebrew)
    if(EXISTS "/opt/homebrew/opt/openssl@3/include")
        set(OPENSSL_ROOT_DIR "/opt/homebrew/opt/openssl@3")
        set(OPENSSL_INCLUDE_DIR "/opt/homebrew/opt/openssl@3/include")
        set(OPENSSL_LIBRARIES "/opt/homebrew/opt/openssl@3/lib")
        set(OPENSSL_CRYPTO_LIBRARY "/opt/homebrew/opt/openssl@3/lib/libcrypto.dylib")
        set(OPENSSL_SSL_LIBRARY "/opt/homebrew/opt/openssl@3/lib/libssl.dylib")
    else()
        set(OPENSSL_ROOT_DIR "/usr/local/opt/openssl@3")
        set(OPENSSL_INCLUDE_DIR "/usr/local/opt/openssl@3/include")
        set(OPENSSL_LIBRARIES "/usr/local/opt/openssl@3/lib")
        set(OPENSSL_CRYPTO_LIBRARY "/usr/local/opt/openssl@3/lib/libcrypto.dylib")
        set(OPENSSL_SSL_LIBRARY "/usr/local/opt/openssl@3/lib/libssl.dylib")
    endif()
    set(OPENSSL_LIBRARY_SUFFIX ".dylib")
elseif(UNIX AND NOT APPLE)
    # Linux (Ubuntu/Debian) - 使用 /usr/local/lib64/ 下的 OpenSSL 库
    set(OPENSSL_ROOT_DIR "/usr/local")
    set(OPENSSL_INCLUDE_DIR "/usr/local/include")
    set(OPENSSL_LIBRARIES "/usr/local/lib64")
    set(OPENSSL_CRYPTO_LIBRARY "/usr/local/lib64/libcrypto.so")
    set(OPENSSL_SSL_LIBRARY "/usr/local/lib64/libssl.so")
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
    if(EXISTS "/opt/homebrew/opt/openssl@3/include")
        set(OpenSSL_VERSION "3.5.0")
        set(OpenSSL_INCLUDE_DIRS "/opt/homebrew/opt/openssl@3/include")
        set(OpenSSL_LIBRARIES "/opt/homebrew/opt/openssl@3/lib/libssl.dylib;/opt/homebrew/opt/openssl@3/lib/libcrypto.dylib")
    else()
        set(OpenSSL_VERSION "3.5.0")
        set(OpenSSL_INCLUDE_DIRS "/usr/local/opt/openssl@3/include")
        set(OpenSSL_LIBRARIES "/usr/local/opt/openssl@3/lib/libssl.dylib;/usr/local/opt/openssl@3/lib/libcrypto.dylib")
    endif()
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
#find_package(OpenSSL REQUIRED)

enable_testing()

# 添加子目录
add_subdirectory(mcp_protocol)
add_subdirectory(log)
add_subdirectory(net)
add_subdirectory(ipc)
add_subdirectory(session)
add_subdirectory(transport)
add_subdirectory(client)
add_subdirectory(server)
add_subdirectory(agent)
add_subdirectory(auth)

# 设置安装规则
install(TARGETS mcp_client mcp_server mcp_agent
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# 安装头文件
install(DIRECTORY client/include/ DESTINATION include)
install(DIRECTORY server/include/ DESTINATION include)
install(DIRECTORY mcp_protocol/include/ DESTINATION include)
install(DIRECTORY transport/include/ DESTINATION include)
install(DIRECTORY ipc/include/ DESTINATION include)
install(DIRECTORY net/include/ DESTINATION include)
install(DIRECTORY log/include/ DESTINATION include)
install(DIRECTORY agent/include/ DESTINATION include)
install(DIRECTORY session/include/ DESTINATION include)
install(DIRECTORY auth/include/ DESTINATION include)
install(DIRECTORY common/ DESTINATION include)

# 安装pkg-config文件
install(FILES debian/mcp-sdk.pc
    DESTINATION /usr/share/pkgconfig)


# 创建打包目标
set(CPACK_PACKAGE_NAME "mcp-sdk")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "MCP SDK - Model Context Protocol C++ SDK")
set(CPACK_PACKAGE_VENDOR "MCP SDK Team")
set(CPACK_PACKAGE_CONTACT "support@mcp-sdk.com")

include(CPack)
