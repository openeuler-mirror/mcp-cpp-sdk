#!/bin/bash

# MCP SDK Debian Package Builder
# This script builds the MCP SDK as two Debian packages:
# - libmcp-sdk: Runtime libraries
# - libmcp-sdk-dev: Development files

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGING_DIR="$PROJECT_ROOT/packaging"
DEBIAN_DIR="$PACKAGING_DIR/debian"
BUILD_DIR="$PROJECT_ROOT/build_deb"
PACKAGE_NAME="mcp-sdk"
VERSION="1.0.0"
ARCHITECTURE="amd64"

echo -e "${GREEN}=== MCP SDK Debian Package Builder ===${NC}"
echo "Building two packages:"
echo "  - libmcp-sdk: Runtime libraries"
echo "  - libmcp-sdk-dev: Development files"
echo "Project Root: $PROJECT_ROOT"
echo "Packaging Dir: $PACKAGING_DIR"
echo "Build Dir: $BUILD_DIR"

# 清理之前的构建
echo -e "${YELLOW}Cleaning previous builds...${NC}"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"

# 构建项目
echo -e "${YELLOW}Building MCP SDK...${NC}"
cd "$PROJECT_ROOT"
mkdir -p build_deb
cd build_deb

# 配置CMake
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON

# 编译
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu)

# 创建两个包的目录结构
echo -e "${YELLOW}Creating package directories...${NC}"
RUNTIME_DIR="$BUILD_DIR/libmcp-sdk"
DEV_DIR="$BUILD_DIR/libmcp-sdk-dev"

mkdir -p "$RUNTIME_DIR/usr/lib"
mkdir -p "$RUNTIME_DIR/usr/bin"
mkdir -p "$RUNTIME_DIR/DEBIAN"

mkdir -p "$DEV_DIR/usr/include"
mkdir -p "$DEV_DIR/usr/lib"
mkdir -p "$DEV_DIR/usr/lib/pkgconfig"
mkdir -p "$DEV_DIR/usr/lib/cmake"
mkdir -p "$DEV_DIR/DEBIAN"

# 安装到临时目录
echo -e "${YELLOW}Installing to temporary directory...${NC}"
TEMP_INSTALL_DIR="$BUILD_DIR/temp_install"
mkdir -p "$TEMP_INSTALL_DIR"
make DESTDIR="$TEMP_INSTALL_DIR" install

# 分离运行时和开发文件
echo -e "${YELLOW}Separating runtime and development files...${NC}"

# 复制运行时库文件
if [ -d "$TEMP_INSTALL_DIR/usr/lib" ]; then
    find "$TEMP_INSTALL_DIR/usr/lib" -name "*.so*" -exec cp {} "$RUNTIME_DIR/usr/lib/" \;
fi

# 复制可执行文件
if [ -d "$TEMP_INSTALL_DIR/usr/bin" ]; then
    find "$TEMP_INSTALL_DIR/usr/bin" -name "mcp_*" -exec cp {} "$RUNTIME_DIR/usr/bin/" \;
fi

# 复制开发文件
if [ -d "$TEMP_INSTALL_DIR/usr/include" ]; then
    cp -r "$TEMP_INSTALL_DIR/usr/include"/* "$DEV_DIR/usr/include/"
fi

if [ -d "$TEMP_INSTALL_DIR/usr/lib" ]; then
    find "$TEMP_INSTALL_DIR/usr/lib" -name "*.a" -exec cp {} "$DEV_DIR/usr/lib/" \;
    if [ -d "$TEMP_INSTALL_DIR/usr/lib/cmake" ]; then
        cp -r "$TEMP_INSTALL_DIR/usr/lib/cmake"/* "$DEV_DIR/usr/lib/cmake/"
    fi
    if [ -d "$TEMP_INSTALL_DIR/usr/lib/pkgconfig" ]; then
        cp -r "$TEMP_INSTALL_DIR/usr/lib/pkgconfig"/* "$DEV_DIR/usr/lib/pkgconfig/"
    fi
fi

# 创建符号链接
echo -e "${YELLOW}Creating symbolic links...${NC}"
cd "$RUNTIME_DIR/usr/lib"
for lib in libmcp_client libmcp_server; do
    if [ -f "${lib}.so.1.0.0" ]; then
        ln -sf "${lib}.so.1.0.0" "${lib}.so.1"
        ln -sf "${lib}.so.1" "${lib}.so"
    fi
done

# 创建 pkg-config 文件
echo -e "${YELLOW}Creating pkg-config file...${NC}"
cat > "$DEV_DIR/usr/lib/pkgconfig/mcp-sdk.pc" << 'EOF'
prefix=/usr
exec_prefix=${prefix}
libdir=${exec_prefix}/lib
includedir=${prefix}/include

Name: MCP SDK
Description: Model Context Protocol C++ SDK
Version: 1.0.0
Libs: -L${libdir} -lmcp_client -lmcp_server
Cflags: -I${includedir}/mcp_sdk
Requires: openssl
EOF

# 复制控制文件
echo -e "${YELLOW}Copying control files...${NC}"
cp "$DEBIAN_DIR/DEBIAN/control" "$RUNTIME_DIR/DEBIAN/"
cp "$DEBIAN_DIR/DEBIAN/control" "$DEV_DIR/DEBIAN/"

# 创建单独的 control 文件
cat > "$RUNTIME_DIR/DEBIAN/control" << 'EOF'
Package: libmcp-sdk
Version: 1.0.0
Section: libs
Priority: optional
Architecture: amd64
Depends:
 ${shlibs:Depends},
 ${misc:Depends},
 libssl3 (>= 3.0.0),
 libc6 (>= 2.17),
 libstdc++6 (>= 9.0.0)
Maintainer: MCP SDK Team <support@mcp-sdk.com>
Description: MCP SDK - Model Context Protocol C++ SDK runtime libraries
 This package provides the MCP (Model Context Protocol) C++ SDK shared libraries
 including both client and server components. The SDK enables communication
 between AI models and external tools through a standardized protocol.
 .
 Features:
  - Client library for connecting to MCP servers
  - Server library for implementing MCP services
  - Support for multiple transport protocols (HTTP, HTTPS, IPC)
  - Comprehensive logging and error handling
  - Cross-platform compatibility
 .
 This package contains the shared libraries needed to run applications
 using the MCP SDK.
EOF

cat > "$DEV_DIR/DEBIAN/control" << 'EOF'
Package: libmcp-sdk-dev
Version: 1.0.0
Section: libdevel
Priority: optional
Architecture: amd64
Depends:
 ${shlibs:Depends},
 ${misc:Depends},
 libmcp-sdk (= ${binary:Version})
Maintainer: MCP SDK Team <support@mcp-sdk.com>
Description: MCP SDK - Model Context Protocol C++ SDK development files
 This package provides the MCP (Model Context Protocol) C++ SDK development
 files including header files and CMake configuration files needed to
 develop applications using the MCP SDK.
 .
 Features:
  - Complete header files for both client and server APIs
  - CMake configuration files for easy integration
  - Example programs and documentation
  - Cross-platform compatibility
 .
 This package contains the development files needed to build applications
 using the MCP SDK.
EOF

# 设置权限
echo -e "${YELLOW}Setting permissions...${NC}"
find "$RUNTIME_DIR" -type f -name "*.so*" -exec chmod 755 {} \;
find "$RUNTIME_DIR" -type f -name "mcp_*" -exec chmod 755 {} \;
find "$DEV_DIR" -type f -name "*.h" -exec chmod 644 {} \;
find "$DEV_DIR" -type f -name "*.a" -exec chmod 644 {} \;
find "$DEV_DIR" -type f -name "*.pc" -exec chmod 644 {} \;
find "$RUNTIME_DIR" -type d -exec chmod 755 {} \;
find "$DEV_DIR" -type d -exec chmod 755 {} \;

# 计算包大小
echo -e "${YELLOW}Calculating package sizes...${NC}"
RUNTIME_SIZE=$(du -sk "$RUNTIME_DIR" | cut -f1)
DEV_SIZE=$(du -sk "$DEV_DIR" | cut -f1)

# 更新 control 文件中的 Installed-Size
sed -i.bak "s/^Installed-Size:.*/Installed-Size: $RUNTIME_SIZE/" "$RUNTIME_DIR/DEBIAN/control" 2>/dev/null || true
sed -i.bak "s/^Installed-Size:.*/Installed-Size: $DEV_SIZE/" "$DEV_DIR/DEBIAN/control" 2>/dev/null || true

# 构建包
echo -e "${YELLOW}Building packages...${NC}"
cd "$BUILD_DIR"

# 检查是否有dpkg-deb命令
if command -v dpkg-deb &> /dev/null; then
    # 使用dpkg-deb构建deb包
    echo -e "${BLUE}Building libmcp-sdk package...${NC}"
    dpkg-deb --build libmcp-sdk "libmcp-sdk_${VERSION}_${ARCHITECTURE}.deb"
    
    echo -e "${BLUE}Building libmcp-sdk-dev package...${NC}"
    dpkg-deb --build libmcp-sdk-dev "libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.deb"
    
    echo -e "${YELLOW}Validating packages...${NC}"
    echo "=== libmcp-sdk package info ==="
    dpkg-deb --info "libmcp-sdk_${VERSION}_${ARCHITECTURE}.deb"
    echo ""
    echo "=== libmcp-sdk package contents ==="
    dpkg-deb --contents "libmcp-sdk_${VERSION}_${ARCHITECTURE}.deb" | head -10
    echo ""
    echo "=== libmcp-sdk-dev package info ==="
    dpkg-deb --info "libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.deb"
    echo ""
    echo "=== libmcp-sdk-dev package contents ==="
    dpkg-deb --contents "libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.deb" | head -10
else
    # 在macOS上创建tar.gz包
    echo -e "${BLUE}Creating tar.gz packages...${NC}"
    tar -czf "libmcp-sdk_${VERSION}_${ARCHITECTURE}.tar.gz" -C libmcp-sdk .
    tar -czf "libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.tar.gz" -C libmcp-sdk-dev .
    echo -e "${YELLOW}Created tar.gz packages instead of deb packages${NC}"
    echo -e "${YELLOW}Package contents:${NC}"
    echo "=== libmcp-sdk package contents ==="
    tar -tzf "libmcp-sdk_${VERSION}_${ARCHITECTURE}.tar.gz" | head -10
    echo ""
    echo "=== libmcp-sdk-dev package contents ==="
    tar -tzf "libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.tar.gz" | head -10
fi

echo -e "${GREEN}=== Packages built successfully! ===${NC}"
if command -v dpkg-deb &> /dev/null; then
    echo "Packages created:"
    echo "  - libmcp-sdk_${VERSION}_${ARCHITECTURE}.deb (Runtime libraries)"
    echo "  - libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.deb (Development files)"
    echo ""
    echo "Location: $BUILD_DIR/"
    echo ""
    echo "To install the packages:"
    echo "  sudo dpkg -i libmcp-sdk_${VERSION}_${ARCHITECTURE}.deb"
    echo "  sudo dpkg -i libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.deb"
    echo ""
    echo "To remove the packages:"
    echo "  sudo dpkg -r libmcp-sdk-dev"
    echo "  sudo dpkg -r libmcp-sdk"
    echo ""
    echo "To check package contents:"
    echo "  dpkg -L libmcp-sdk"
    echo "  dpkg -L libmcp-sdk-dev"
    echo ""
    echo "Package sizes:"
    ls -lh "$BUILD_DIR"/*.deb
else
    echo "Packages created:"
    echo "  - libmcp-sdk_${VERSION}_${ARCHITECTURE}.tar.gz (Runtime libraries)"
    echo "  - libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.tar.gz (Development files)"
    echo ""
    echo "Location: $BUILD_DIR/"
    echo ""
    echo "To extract the packages:"
    echo "  tar -xzf libmcp-sdk_${VERSION}_${ARCHITECTURE}.tar.gz -C /usr/local"
    echo "  tar -xzf libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.tar.gz -C /usr/local"
    echo ""
    echo "To check package contents:"
    echo "  tar -tzf libmcp-sdk_${VERSION}_${ARCHITECTURE}.tar.gz"
    echo "  tar -tzf libmcp-sdk-dev_${VERSION}_${ARCHITECTURE}.tar.gz"
    echo ""
    echo "Package sizes:"
    ls -lh "$BUILD_DIR"/*.tar.gz
fi

echo ""
echo -e "${BLUE}=== Package Structure ===${NC}"
echo "libmcp-sdk (Runtime):"
echo "  - /usr/lib/libmcp_client.so*"
echo "  - /usr/lib/libmcp_server.so*"
echo "  - /usr/bin/mcp_* (example programs)"
echo ""
echo "libmcp-sdk-dev (Development):"
echo "  - /usr/include/mcp_sdk/"
echo "  - /usr/lib/libmcp_client.a"
echo "  - /usr/lib/libmcp_server.a"
echo "  - /usr/lib/pkgconfig/mcp-sdk.pc"
echo "  - /usr/lib/cmake/mcp_sdk/"
