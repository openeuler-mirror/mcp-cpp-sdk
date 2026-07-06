#!/bin/bash

# Test script for MCP SDK libraries

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INSTALL_DIR="$PROJECT_ROOT/build_libs/install"

echo -e "${GREEN}=== MCP SDK Library Test ===${NC}"

# 检查库文件是否存在
echo -e "${YELLOW}Checking library files...${NC}"
if [ ! -f "$INSTALL_DIR/lib/libmcp_client.dylib" ]; then
    echo -e "${RED}❌ libmcp_client.dylib not found!${NC}"
    exit 1
fi

if [ ! -f "$INSTALL_DIR/lib/libmcp_server.dylib" ]; then
    echo -e "${RED}❌ libmcp_server.dylib not found!${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Library files found${NC}"

# 检查头文件是否存在
echo -e "${YELLOW}Checking header files...${NC}"
if [ ! -f "$INSTALL_DIR/include/mcp_sdk/client/mcp_client.h" ]; then
    echo -e "${RED}❌ mcp_client.h not found!${NC}"
    exit 1
fi

if [ ! -f "$INSTALL_DIR/include/mcp_sdk/server/mcp_server.h" ]; then
    echo -e "${RED}❌ mcp_server.h not found!${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Header files found${NC}"

# 编译测试程序
echo -e "${YELLOW}Compiling test program...${NC}"
g++ -std=c++17 \
    -I"$INSTALL_DIR/include" \
    -L"$INSTALL_DIR/lib" \
    -o test_libs \
    test_libs.cpp \
    -lmcp_client -lmcp_server

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Test program compiled successfully${NC}"
else
    echo -e "${RED}❌ Compilation failed${NC}"
    exit 1
fi

# 运行测试程序
echo -e "${YELLOW}Running test program...${NC}"
export DYLD_LIBRARY_PATH="$INSTALL_DIR/lib:$DYLD_LIBRARY_PATH"
./test_libs

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Test program ran successfully${NC}"
else
    echo -e "${RED}❌ Test program failed${NC}"
    exit 1
fi

# 清理
rm -f test_libs

echo -e "${GREEN}=== All tests passed! ===${NC}"
echo "The MCP SDK libraries are working correctly."
