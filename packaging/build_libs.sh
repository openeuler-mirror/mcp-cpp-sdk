#!/bin/bash

# MCP SDK Library Builder (macOS/Linux)
# This script builds the MCP SDK shared libraries for testing

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build_libs"

echo -e "${GREEN}=== MCP SDK Library Builder ===${NC}"
echo "Project Root: $PROJECT_ROOT"
echo "Build Dir: $BUILD_DIR"

# 清理之前的构建
echo -e "${YELLOW}Cleaning previous builds...${NC}"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# 构建项目
echo -e "${YELLOW}Building MCP SDK...${NC}"
cd "$PROJECT_ROOT"

# 配置CMake
cmake -B build_libs \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# 编译
cmake --build build_libs --parallel

# 安装到临时目录
echo -e "${YELLOW}Installing libraries...${NC}"
TEMP_INSTALL_DIR="$BUILD_DIR/install"
cmake --install build_libs --prefix "$TEMP_INSTALL_DIR"

# 显示构建结果
echo -e "${GREEN}=== Build completed successfully! ===${NC}"
echo "Libraries built:"
find "$TEMP_INSTALL_DIR" -name "*.so*" -o -name "*.dylib*" | while read lib; do
    echo "  - $lib"
    if command -v otool >/dev/null 2>&1; then
        echo "    Dependencies:"
        otool -L "$lib" | sed 's/^/      /'
    elif command -v ldd >/dev/null 2>&1; then
        echo "    Dependencies:"
        ldd "$lib" | sed 's/^/      /'
    fi
    echo ""
done

echo "Header files installed:"
find "$TEMP_INSTALL_DIR" -name "*.h" | while read header; do
    echo "  - $header"
done

echo ""
echo "To use the libraries in your project:"
echo "  - Add to CMakeLists.txt:"
echo "    find_library(MCP_CLIENT_LIB mcp_client PATHS $TEMP_INSTALL_DIR/lib)"
echo "    find_library(MCP_SERVER_LIB mcp_server PATHS $TEMP_INSTALL_DIR/lib)"
echo "    target_link_libraries(your_target \${MCP_CLIENT_LIB} \${MCP_SERVER_LIB})"
echo ""
echo "  - Include headers:"
echo "    #include <mcp_sdk/client/mcp_client.h>"
echo "    #include <mcp_sdk/server/mcp_server.h>"
