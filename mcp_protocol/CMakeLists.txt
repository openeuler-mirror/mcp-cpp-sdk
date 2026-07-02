cmake_minimum_required(VERSION 3.10)
project(mcp_protocol VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../log
    ${CMAKE_CURRENT_SOURCE_DIR}/../common
)

# 源文件
set(MCP_PROTOCOL_SOURCES
    mcp_protocol_core.cpp
    mcp_client.cpp
    mcp_server.cpp
    ../log/logger.cpp
)

# 头文件
set(MCP_PROTOCOL_HEADERS
    mcp_protocol.h
    mcp_protocol_core.h
    mcp_client.h
    mcp_server.h
    mcp_message.h
)

# 创建静态库
add_library(mcp_protocol STATIC ${MCP_PROTOCOL_SOURCES} ${MCP_PROTOCOL_HEADERS})

# 设置库属性
set_target_properties(mcp_protocol PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
)

# 链接线程库
find_package(Threads REQUIRED)
target_link_libraries(mcp_protocol PRIVATE Threads::Threads)

# 创建示例程序
add_executable(mcp_example mcp_example.cpp)
target_link_libraries(mcp_example PRIVATE mcp_protocol)

# 创建重构后的示例程序
add_executable(mcp_example_refactored mcp_example_refactored.cpp)
target_link_libraries(mcp_example_refactored PRIVATE mcp_protocol)

# 安装规则
install(TARGETS mcp_protocol
    EXPORT mcp_protocolTargets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
    PUBLIC_HEADER DESTINATION include/mcp_protocol
)

install(TARGETS mcp_example mcp_example_refactored
    RUNTIME DESTINATION bin
)

# 创建配置文件
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_protocolConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# 创建简单的配置文件
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mcp_protocolConfig.cmake"
"@PACKAGE_INIT@
include(\"\${CMAKE_CURRENT_LIST_DIR}/mcp_protocolTargets.cmake\")
check_required_components(mcp_protocol)
")

# 安装配置文件
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_protocolConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_protocolConfigVersion.cmake"
    DESTINATION lib/cmake/mcp_protocol
)

# 导出目标
install(EXPORT mcp_protocolTargets
    FILE mcp_protocolTargets.cmake
    NAMESPACE mcp_protocol::
    DESTINATION lib/cmake/mcp_protocol
)

# 打印配置信息
message(STATUS "MCP Protocol 配置信息:")
message(STATUS "  项目名称: ${PROJECT_NAME}")
message(STATUS "  版本: ${PROJECT_VERSION}")
message(STATUS "  C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  安装前缀: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  输出目录: ${CMAKE_BINARY_DIR} (build目录)")
message(STATUS "  包含目录:")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../log")
message(STATUS "    - ${CMAKE_CURRENT_SOURCE_DIR}/../common")
message(STATUS "依赖检查:")
message(STATUS "  Threads: ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "  平台: ${CMAKE_SYSTEM_NAME}")
