cmake_minimum_required(VERSION 3.10)
project(MCPAuth)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_library(mcp_auth STATIC
    src/auth_manager.cpp
)

target_include_directories(mcp_auth
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${OPENSSL_INCLUDE_DIR}
        ${OpenSSL_INCLUDE_DIR}
        ${OpenSSL_INCLUDE_DIRS}
)

target_link_libraries(mcp_auth
    PUBLIC
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
)

add_executable(auth_manager_example
    examples/auth_manager_example.cpp
)

target_link_libraries(auth_manager_example
    PRIVATE
        mcp_auth
)

add_executable(auth_manager_gtest
    test/auth_manager_gtest.cpp
)

target_link_libraries(auth_manager_gtest
    PRIVATE
        mcp_auth
        GTest::gtest
        GTest::gtest_main
        Threads::Threads
        OpenSSL::SSL
        OpenSSL::Crypto
)

enable_testing()
add_test(
    NAME auth_manager_gtest
    COMMAND auth_manager_gtest
)

install(TARGETS mcp_auth auth_manager_example auth_manager_gtest
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY include/
    DESTINATION include
)


