# CMakeLists.txt for pipe communication module

cmake_minimum_required(VERSION 3.10)
project(PipeCommunication)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找依赖
find_package(PkgConfig REQUIRED)
pkg_check_modules(JSONCPP jsoncpp)

# 包含目录
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../common)

# 源文件
set(PIPE_COMM_SOURCES
    src/pipe_communication.cpp
)

# 头文件
set(PIPE_COMM_HEADERS
    include/pipe_communication.h
)

# 创建静态库
add_library(pipe_communication STATIC ${PIPE_COMM_SOURCES} ${PIPE_COMM_HEADERS})

# 链接库
if(JSONCPP_FOUND)
    target_link_libraries(pipe_communication ${JSONCPP_LIBRARIES})
    target_include_directories(pipe_communication PRIVATE ${JSONCPP_INCLUDE_DIRS})
    target_compile_options(pipe_communication PRIVATE ${JSONCPP_CFLAGS_OTHER})
else()
    # 如果没有找到系统安装的jsoncpp，使用项目中的版本
    target_include_directories(pipe_communication PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../common)
endif()

# 平台特定设置
if(WIN32)
    target_link_libraries(pipe_communication kernel32)
else()
    target_link_libraries(pipe_communication pthread)
endif()

# 创建示例可执行文件
add_executable(pipe_communication_example examples/pipe_communication_example.cpp)
target_link_libraries(pipe_communication_example pipe_communication)

# 安装规则
install(TARGETS pipe_communication pipe_communication_example
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${PIPE_COMM_HEADERS}
    DESTINATION include/mcp
)

# 测试
enable_testing()

# 添加测试
add_executable(pipe_communication_test test/pipe_communication_test.cpp)
target_link_libraries(pipe_communication_test pipe_communication)

add_test(NAME pipe_communication_test COMMAND pipe_communication_test)
