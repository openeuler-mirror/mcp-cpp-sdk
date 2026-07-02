cmake_minimum_required(VERSION 3.10)
project(mcp_sdk VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 设置编译选项
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Od /Zi")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
endif()

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 包含目录（全局）
include_directories(
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/log
    ${CMAKE_SOURCE_DIR}/ipc/include
    ${CMAKE_SOURCE_DIR}/net
)

# 查找依赖库
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

# 添加子目录
add_subdirectory(log)
add_subdirectory(ipc)
add_subdirectory(net)
add_subdirectory(transport)
add_subdirectory(mcp_protocol)
add_subdirectory(client)

# 创建主要的库目标（组合所有模块）
add_library(mcp_sdk_all INTERFACE)

# 链接所有子模块
target_link_libraries(mcp_sdk_all INTERFACE
    mcp_logger
    mcp_transport
    mcp_protocol
    mcp_client
)

# 设置包含目录
target_include_directories(mcp_sdk_all INTERFACE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/common
    ${CMAKE_SOURCE_DIR}/log
    ${CMAKE_SOURCE_DIR}/ipc/include
    ${CMAKE_SOURCE_DIR}/net
    ${CMAKE_SOURCE_DIR}/transport
    ${CMAKE_SOURCE_DIR}/mcp_protocol
    ${CMAKE_SOURCE_DIR}/client
)

# 创建综合示例程序
add_executable(mcp_sdk_demo
    examples/sdk_demo.cpp
)

target_link_libraries(mcp_sdk_demo
    mcp_sdk_all
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
)

# 安装规则
install(TARGETS mcp_sdk_all
    EXPORT mcp_sdkTargets
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(TARGETS mcp_sdk_demo
    RUNTIME DESTINATION bin
)

# 安装头文件
install(DIRECTORY ${CMAKE_SOURCE_DIR}/client/
    DESTINATION include/mcp_sdk/client
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/mcp_protocol/
    DESTINATION include/mcp_sdk/mcp_protocol
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/transport/
    DESTINATION include/mcp_sdk/transport
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/log/
    DESTINATION include/mcp_sdk/log
    FILES_MATCHING PATTERN "*.h"
)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/common/
    DESTINATION include/mcp_sdk/common
    FILES_MATCHING PATTERN "*.h*"
)

# 创建配置文件
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_sdkConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# 创建主配置文件
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/mcp_sdkConfig.cmake"
"@PACKAGE_INIT@

# 包含所有子模块的配置
include(\"\${CMAKE_CURRENT_LIST_DIR}/mcp_sdkTargets.cmake\")

# 设置变量
set(MCP_SDK_VERSION ${PROJECT_VERSION})
set(MCP_SDK_INCLUDE_DIRS 
    \${CMAKE_CURRENT_LIST_DIR}/../../../include/mcp_sdk
    \${CMAKE_CURRENT_LIST_DIR}/../../../include/mcp_sdk/client
    \${CMAKE_CURRENT_LIST_DIR}/../../../include/mcp_sdk/mcp_protocol
    \${CMAKE_CURRENT_LIST_DIR}/../../../include/mcp_sdk/transport
    \${CMAKE_CURRENT_LIST_DIR}/../../../include/mcp_sdk/log
    \${CMAKE_CURRENT_LIST_DIR}/../../../include/mcp_sdk/common
)

# 检查必需的组件
check_required_components(mcp_sdk_all)
")

# 安装配置文件
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_sdkConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/mcp_sdkConfigVersion.cmake"
    DESTINATION lib/cmake/mcp_sdk
)

# 导出目标
install(EXPORT mcp_sdkTargets
    FILE mcp_sdkTargets.cmake
    NAMESPACE mcp_sdk::
    DESTINATION lib/cmake/mcp_sdk
)

# 创建测试目标
enable_testing()

# 添加测试
add_test(NAME mcp_sdk_demo_test
    COMMAND ./mcp_sdk_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 添加自定义目标
add_custom_target(run_demo
    COMMAND ./mcp_sdk_demo
    DEPENDS mcp_sdk_demo
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    COMMENT "运行 MCP SDK 演示程序"
)

add_custom_target(run_all_examples
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_client_example
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target run_demo
    DEPENDS mcp_sdk_demo mcp_client_example
    COMMENT "运行所有示例程序"
)

add_custom_target(run_all_tests
    COMMAND ${CMAKE_CTEST_COMMAND} --verbose
    DEPENDS mcp_sdk_demo mcp_client_example
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "运行所有测试"
)

# 创建清理目标
add_custom_target(clean_all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/bin
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}/lib
    COMMENT "清理所有生成的文件"
)

# 打印配置信息
message(STATUS "")
message(STATUS "==========================================")
message(STATUS "MCP SDK 构建配置")
message(STATUS "==========================================")
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "版本: ${PROJECT_VERSION}")
message(STATUS "C++ 标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "安装前缀: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "源目录: ${CMAKE_SOURCE_DIR}")
message(STATUS "构建目录: ${CMAKE_BINARY_DIR}")
message(STATUS "")
message(STATUS "输出目录:")
message(STATUS "  可执行文件: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")
message(STATUS "  库文件: ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
message(STATUS "  静态库: ${CMAKE_ARCHIVE_OUTPUT_DIRECTORY}")
message(STATUS "")
message(STATUS "依赖检查:")
message(STATUS "  Threads: ${CMAKE_THREAD_LIBS_INIT}")
message(STATUS "  OpenSSL: ${OPENSSL_VERSION}")
message(STATUS "")
message(STATUS "平台信息:")
message(STATUS "  系统: ${CMAKE_SYSTEM_NAME}")
message(STATUS "  处理器: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "")
message(STATUS "构建目标:")
message(STATUS "  mcp_sdk_all - 主要库目标（包含所有模块）")
message(STATUS "  mcp_client - MCP客户端库")
message(STATUS "  mcp_transport - 传输层库")
message(STATUS "  mcp_protocol - 协议核心库")
message(STATUS "  mcp_logger - 日志库")
message(STATUS "  mcp_sdk_demo - 演示程序")
message(STATUS "  mcp_client_example - 客户端示例")
message(STATUS "")
message(STATUS "自定义目标:")
message(STATUS "  run_demo - 运行演示程序")
message(STATUS "  run_all_examples - 运行所有示例")
message(STATUS "  run_all_tests - 运行所有测试")
message(STATUS "  clean_all - 清理所有文件")
message(STATUS "==========================================")
message(STATUS "")

# 创建打包配置
set(CPACK_PACKAGE_NAME "mcp_sdk")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Model Context Protocol (MCP) SDK for C++")
set(CPACK_PACKAGE_VENDOR "MCP SDK Team")
set(CPACK_PACKAGE_CONTACT "support@mcp-sdk.com")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/mcp-sdk/cpp-mcp")

# 设置打包组件
set(CPACK_COMPONENTS_ALL
    mcp_sdk_all
    mcp_client
    mcp_transport
    mcp_protocol
    mcp_logger
    headers
    examples
)

# 组件描述
set(CPACK_COMPONENT_MCP_SDK_ALL_DISPLAY_NAME "MCP SDK Core")
set(CPACK_COMPONENT_MCP_SDK_ALL_DESCRIPTION "Complete MCP SDK with all modules")

set(CPACK_COMPONENT_MCP_CLIENT_DISPLAY_NAME "MCP Client")
set(CPACK_COMPONENT_MCP_CLIENT_DESCRIPTION "MCP Client library with transport integration")

set(CPACK_COMPONENT_MCP_TRANSPORT_DISPLAY_NAME "MCP Transport")
set(CPACK_COMPONENT_MCP_TRANSPORT_DESCRIPTION "Transport layer supporting IPC and SSE")

set(CPACK_COMPONENT_MCP_PROTOCOL_DISPLAY_NAME "MCP Protocol")
set(CPACK_COMPONENT_MCP_PROTOCOL_DESCRIPTION "MCP protocol core implementation")

set(CPACK_COMPONENT_MCP_LOGGER_DISPLAY_NAME "MCP Logger")
set(CPACK_COMPONENT_MCP_LOGGER_DESCRIPTION "Unified logging system")

set(CPACK_COMPONENT_HEADERS_DISPLAY_NAME "Header Files")
set(CPACK_COMPONENT_HEADERS_DESCRIPTION "C++ header files")

set(CPACK_COMPONENT_EXAMPLES_DISPLAY_NAME "Examples")
set(CPACK_COMPONENT_EXAMPLES_DESCRIPTION "Example programs and demos")

include(CPack)
