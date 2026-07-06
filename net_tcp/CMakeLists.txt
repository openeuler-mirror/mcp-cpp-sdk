cmake_minimum_required(VERSION 3.10)
project(mcp_net_tcp VERSION 1.0.0 LANGUAGES CXX)

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

# 查找 libevent
set(LIBEVENT_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../3thirdparty/libevent-2.1.12-stable")
set(LIBEVENT_INCLUDE_DIR "${LIBEVENT_ROOT_DIR}/include")
set(LIBEVENT_LIB_DIR "${LIBEVENT_ROOT_DIR}/.libs")

# 检查本地构建的 libevent
if(EXISTS "${LIBEVENT_LIB_DIR}/libevent.a" OR EXISTS "${LIBEVENT_LIB_DIR}/libevent_core.a")
    message(STATUS "Using local libevent from ${LIBEVENT_ROOT_DIR}")
    set(LIBEVENT_FOUND TRUE)
    set(LIBEVENT_INCLUDE_DIRS ${LIBEVENT_INCLUDE_DIR})
    
    # 查找库文件
    if(EXISTS "${LIBEVENT_LIB_DIR}/libevent.a")
        set(LIBEVENT_LIBRARIES "${LIBEVENT_LIB_DIR}/libevent.a")
    elseif(EXISTS "${LIBEVENT_LIB_DIR}/libevent_core.a")
        set(LIBEVENT_LIBRARIES "${LIBEVENT_LIB_DIR}/libevent_core.a")
    else()
        # 尝试使用系统安装的 libevent
        find_library(LIBEVENT_LIBRARIES
            NAMES event event_core
            PATHS
                /usr/lib
                /usr/local/lib
                /opt/homebrew/lib
                /usr/local/opt/libevent/lib
        )
    endif()
else()
    # 尝试使用系统安装的 libevent
    message(STATUS "Trying to find system libevent")
    find_path(LIBEVENT_INCLUDE_DIRS
        NAMES event2/event.h
        PATHS
            /usr/include
            /usr/local/include
            /opt/homebrew/include
            /usr/local/opt/libevent/include
    )
    
    find_library(LIBEVENT_LIBRARIES
        NAMES event event_core
        PATHS
            /usr/lib
            /usr/local/lib
            /opt/homebrew/lib
            /usr/local/opt/libevent/lib
    )
    
    if(LIBEVENT_INCLUDE_DIRS AND LIBEVENT_LIBRARIES)
        set(LIBEVENT_FOUND TRUE)
        message(STATUS "Found system libevent")
    else()
        set(LIBEVENT_FOUND FALSE)
    endif()
endif()

if(NOT LIBEVENT_FOUND)
    message(FATAL_ERROR "libevent not found. Please install libevent or build it in 3thirdparty/libevent-2.1.12-stable")
endif()

message(STATUS "libevent include dir: ${LIBEVENT_INCLUDE_DIRS}")
message(STATUS "libevent libraries: ${LIBEVENT_LIBRARIES}")

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LIBEVENT_INCLUDE_DIRS}
)

# 查找必要的包
find_package(Threads REQUIRED)

# 创建 TCP/UDP Socket 库
add_library(mcp_net_tcp STATIC
    src/tcp_socket.cpp
    src/udp_socket.cpp
)

# 设置库的包含目录
target_include_directories(mcp_net_tcp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(mcp_net_tcp
    Threads::Threads
    ${LIBEVENT_LIBRARIES}
)

# 创建客户端示例程序
add_executable(tcp_client_example
    examples/tcp_client_example.cpp
)

target_link_libraries(tcp_client_example
    mcp_net_tcp
    Threads::Threads
    ${LIBEVENT_LIBRARIES}
)

# 创建服务器示例程序
add_executable(tcp_server_example
    examples/tcp_server_example.cpp
)

target_link_libraries(tcp_server_example
    mcp_net_tcp
    Threads::Threads
    ${LIBEVENT_LIBRARIES}
)

# 创建 UDP 客户端示例程序
add_executable(udp_client_example
    examples/udp_client_example.cpp
)

target_link_libraries(udp_client_example
    mcp_net_tcp
    Threads::Threads
    ${LIBEVENT_LIBRARIES}
)

# 创建 UDP 服务器示例程序
add_executable(udp_server_example
    examples/udp_server_example.cpp
)

target_link_libraries(udp_server_example
    mcp_net_tcp
    Threads::Threads
    ${LIBEVENT_LIBRARIES}
)

# 设置编译选项
if(MSVC)
    target_compile_options(mcp_net_tcp PRIVATE /W4)
    target_compile_options(tcp_client_example PRIVATE /W4)
    target_compile_options(tcp_server_example PRIVATE /W4)
    target_compile_options(udp_client_example PRIVATE /W4)
    target_compile_options(udp_server_example PRIVATE /W4)
else()
    target_compile_options(mcp_net_tcp PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(tcp_client_example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(tcp_server_example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(udp_client_example PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(udp_server_example PRIVATE -Wall -Wextra -Wpedantic)
endif()

# 添加Google Test支持
find_package(GTest QUIET)
if(GTest_FOUND)
    message(STATUS "Google Test found: ${GTEST_INCLUDE_DIRS}")
    
    # 创建Google Test测试可执行文件
    add_executable(net_tcp_gtest
        test/net_tcp_gtest.cpp
    )
    
    # 设置测试可执行文件输出目录
    set_target_properties(net_tcp_gtest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # 链接测试依赖
    target_link_libraries(net_tcp_gtest
        mcp_net_tcp
        GTest::GTest
        GTest::Main
        Threads::Threads
        ${LIBEVENT_LIBRARIES}
    )
    
    # 设置测试包含目录
    target_include_directories(net_tcp_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${LIBEVENT_INCLUDE_DIRS}
        ${GTEST_INCLUDE_DIRS}
    )
    
    # 编译选项
    if(MSVC)
        target_compile_options(net_tcp_gtest PRIVATE /W4)
    else()
        target_compile_options(net_tcp_gtest PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    
    # 启用测试
    enable_testing()
    
    # 添加Google Test测试
    add_test(NAME net_tcp_gtest
        COMMAND ./net_tcp_gtest
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    )
    
    # 设置测试属性
    set_tests_properties(net_tcp_gtest PROPERTIES
        TIMEOUT 60
    )
    
    message(STATUS "Google Test executable created: net_tcp_gtest")
else()
    message(STATUS "Google Test not found, skipping test build")
endif()

# 安装规则
install(TARGETS mcp_net_tcp tcp_client_example tcp_server_example udp_client_example udp_server_example
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 如果测试可执行文件存在，也安装它
if(TARGET net_tcp_gtest)
    install(TARGETS net_tcp_gtest
        RUNTIME DESTINATION bin
    )
endif()

install(FILES include/tcp_socket.h include/udp_socket.h
    DESTINATION include
)

# 打印配置信息
message(STATUS "MCP Net TCP Configuration:")
message(STATUS "  Project Name: ${PROJECT_NAME}")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Output Directory: ${CMAKE_BINARY_DIR}")

