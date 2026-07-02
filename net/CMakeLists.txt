cmake_minimum_required(VERSION 3.10)
project(httplib_demos)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/common)

# 查找必要的包
find_package(Threads REQUIRED)

# 可选：查找OpenSSL（用于HTTPS支持）
find_package(OpenSSL QUIET)

# 注意：httplib_https_demo.cpp 文件已被删除
# 如果需要HTTPS demo，请重新创建该文件

# 如果找到OpenSSL，显示支持信息
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found - HTTPS support available")
else()
    message(STATUS "OpenSSL not found - HTTPS support disabled")
endif()


# 设置curl demo的编译定义

# 创建本地MCP服务器demo
add_executable(localhost_mcp_demo
    localhost_mcp_demo.cpp
    netio.cpp
)

# 设置编译定义
target_compile_definitions(localhost_mcp_demo PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# 链接基础库
target_link_libraries(localhost_mcp_demo
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 创建NetIO MCP demo
add_executable(netio_mcp_demo
    netio_mcp_demo.cpp
    netio.cpp
)



# 设置编译定义
target_compile_definitions(netio_mcp_demo PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

# 链接基础库
target_link_libraries(netio_mcp_demo
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)


# 设置编译选项
if(MSVC)
    target_compile_options(netio_mcp_demo PRIVATE /W4)
    target_compile_options(localhost_mcp_demo PRIVATE /W4)
else()
    target_compile_options(netio_mcp_demo PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(localhost_mcp_demo PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 安装规则
set(INSTALL_TARGETS localhost_mcp_demo netio_mcp_demo)

install(TARGETS ${INSTALL_TARGETS}
    RUNTIME DESTINATION bin
)
