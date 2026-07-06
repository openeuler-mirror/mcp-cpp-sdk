# MCP SDK Packaging

This directory contains scripts and configuration files for packaging the MCP SDK as Debian packages and shared libraries.

## Directory Structure

```
packaging/
├── debian/                    # Debian package structure
│   └── DEBIAN/               # Debian control files
│       ├── control           # Package metadata
│       ├── postinst          # Post-installation script
│       ├── prerm             # Pre-removal script
│       └── postrm            # Post-removal script
├── build_deb.sh              # Debian package builder
├── build_libs.sh             # Shared library builder (macOS/Linux)
└── README.md                 # This file
```

## Building Shared Libraries

### For Development/Testing (macOS/Linux)

Use the `build_libs.sh` script to build shared libraries for local development:

```bash
cd packaging
./build_libs.sh
```

This will:
- Build the MCP SDK as shared libraries (.so/.dylib)
- Install them to a temporary directory
- Show library dependencies
- Provide usage instructions

### Output

The script creates:
- `libmcp_client.so` - Client library
- `libmcp_server.so` - Server library
- Header files in `include/mcp_sdk/`

## Building Debian Package

### Prerequisites

- Ubuntu/Debian system
- `dpkg-deb` package
- CMake 3.10+
- OpenSSL development libraries
- C++17 compatible compiler

### Build Process

```bash
cd packaging
./build_deb.sh
```

This will:
- Build the MCP SDK as shared libraries
- Create a proper Debian package structure
- Generate a `.deb` file
- Validate the package

### Package Contents

The Debian package includes:
- `/usr/lib/libmcp_client.so*` - Client shared library
- `/usr/lib/libmcp_server.so*` - Server shared library
- `/usr/include/mcp_sdk/` - Header files
- `/usr/lib/cmake/mcp_sdk/` - CMake configuration files

### Installation

```bash
# Install the package
sudo dpkg -i mcp-sdk_1.0.0_amd64.deb

# Check installation
dpkg -L mcp-sdk

# Remove package
sudo dpkg -r mcp-sdk
```

## Using the Libraries

### CMake Integration

```cmake
# Find the MCP SDK
find_package(mcp_sdk REQUIRED)

# Link against the libraries
target_link_libraries(your_target
    mcp_sdk::mcp_client
    mcp_sdk::mcp_server
)
```

### Manual Linking

```bash
# Compile with MCP SDK
g++ -o myapp myapp.cpp -lmcp_client -lmcp_server

# Or with pkg-config (if available)
g++ -o myapp myapp.cpp $(pkg-config --cflags --libs mcp-sdk)
```

### Include Headers

```cpp
#include <mcp_sdk/client/mcp_client.h>
#include <mcp_sdk/server/mcp_server.h>
```

## Package Information

- **Package Name**: mcp-sdk
- **Version**: 1.0.0
- **Architecture**: amd64
- **Dependencies**: libssl3, libc6, libstdc++6
- **Section**: libs
- **Priority**: optional

## Troubleshooting

### Library Not Found

If you get "library not found" errors:

```bash
# Update library cache
sudo ldconfig

# Check library location
ldd /usr/lib/libmcp_client.so
```

### Missing Headers

Ensure the header files are installed:

```bash
# Check header installation
ls -la /usr/include/mcp_sdk/
```

### Build Errors

Common build issues:

1. **Missing OpenSSL**: Install `libssl-dev`
2. **CMake version**: Ensure CMake 3.10+
3. **Compiler**: Use C++17 compatible compiler
4. **Permissions**: Run with appropriate permissions

## Development

### Modifying Package Configuration

Edit the following files:
- `debian/DEBIAN/control` - Package metadata
- `debian/DEBIAN/postinst` - Post-installation actions
- `debian/DEBIAN/prerm` - Pre-removal actions
- `debian/DEBIAN/postrm` - Post-removal actions

### Testing Package

```bash
# Test package installation
sudo dpkg -i mcp-sdk_1.0.0_amd64.deb

# Test package removal
sudo dpkg -r mcp-sdk

# Verify no files left behind
dpkg -L mcp-sdk
```

## Support

For issues with packaging or installation, please check:
1. System requirements
2. Dependencies
3. Build logs
4. Package validation output

Contact: support@mcp-sdk.com
