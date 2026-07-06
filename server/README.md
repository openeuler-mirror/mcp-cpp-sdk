# MCP Server 实现

## 概述

这个目录包含了基于最新transport模块的MCP (Model Context Protocol) 服务器实现，提供了类似 `@modelcontextprotocol/server-filesystem` 的文件系统功能。

## 架构设计

### 继承层次结构

```
MCPServer (基类)
    ↓
MCPFileSystemServer (文件系统专用子类)
```

### 设计原则

1. **抽象与具体分离**: 基类MCPServer提供通用的MCP服务器功能，子类实现特定的业务逻辑
2. **可扩展性**: 通过继承和重写虚拟方法，可以轻松创建不同类型的专用服务器
3. **功能封装**: 每个子类封装特定的工具和资源管理逻辑
4. **安全性**: 内置路径验证和权限控制机制

## 主要特性

### 🚀 基于最新Transport模块
- 集成了最新的 `mcp_transport::Transport` 模块
- 支持多种传输方式：IPC、SSE、自动选择
- 统一的请求处理接口
- 完整的错误处理和日志记录

### 📁 文件系统工具
- **read_file**: 读取文件内容
- **write_file**: 写入文件内容
- **list_directory**: 列出目录内容
- **create_directory**: 创建新目录

### 🔗 文件系统资源
- 支持 `file://` URI 协议
- 自动检测文件和目录类型
- 返回结构化的JSON数据
- 包含文件元信息（大小、类型等）

### 🛠️ 核心功能
- MCP协议完整实现
- 工具调用处理
- 资源管理
- 提示管理
- 通知系统

## 文件结构

```
server/
├── include/
│   ├── mcp_server.h              # 基类服务器头文件
│   ├── mcp_filesystem_server.h   # 文件系统服务器头文件
│   └── mcp_protocol.h            # 统一协议头文件
├── src/
│   ├── mcp_server.cpp            # 基类服务器实现
│   └── mcp_filesystem_server.cpp # 文件系统服务器实现
├── examples/
│   ├── server_example.cpp                    # 基础示例
│   ├── mcp_server_filesystem_example.cpp     # 文件系统示例（旧版）
│   └── mcp_filesystem_server_demo.cpp        # 文件系统服务器demo
├── test/
│   ├── mcp_server_test.cpp           # 基类测试
│   └── mcp_filesystem_server_test.cpp # 文件系统服务器测试
├── CMakeLists.txt            # 构建配置
└── README.md                 # 本文档
```

## 快速开始

### 编译

```bash
cd /path/to/cpp_mcp_sdk
mkdir build && cd build
cmake ..
make server_filesystem_demo
```

### 运行文件系统服务器

```bash
# 运行完整的demo（推荐）
./bin/server_filesystem_demo

# 或者运行基础示例
./bin/server_filesystem_example

# 或者运行基础服务器
./bin/server_example
```

### 运行测试

```bash
# 测试文件系统服务器功能
./bin/filesystem_server_test

# 测试基础服务器功能
./bin/server_test
```

### 使用MCP客户端连接

```bash
# 使用IPC连接
./bin/mcp_client_ipc_demo
```

## API 参考

### 工具接口

#### read_file
读取指定路径的文件内容。

**参数:**
- `path` (string): 文件路径

**返回:**
```json
{
  "content": "文件内容",
  "path": "文件路径"
}
```

#### write_file
将内容写入指定路径的文件。

**参数:**
- `path` (string): 文件路径
- `content` (string): 要写入的内容

**返回:**
```json
{
  "message": "File written successfully",
  "path": "文件路径"
}
```

#### list_directory
列出指定目录的内容。

**参数:**
- `path` (string): 目录路径（可选，默认为当前目录）

**返回:**
```json
{
  "files": [
    {
      "name": "文件名",
      "path": "完整路径",
      "is_directory": false,
      "size": 1024
    }
  ],
  "path": "目录路径"
}
```

#### create_directory
创建新目录。

**参数:**
- `path` (string): 目录路径

**返回:**
```json
{
  "message": "Directory created successfully",
  "path": "目录路径"
}
```

### 资源接口

#### 文件资源 (file://)
访问文件系统资源。

**URI格式:**
- `file://.` - 当前目录
- `file://path/to/file` - 指定文件
- `file://path/to/directory` - 指定目录

**目录资源返回:**
```json
{
  "type": "directory",
  "path": "目录路径",
  "uri": "file://目录路径",
  "files": [
    {
      "name": "文件名",
      "path": "完整路径",
      "is_directory": false,
      "size": 1024
    }
  ]
}
```

**文件资源返回:**
```json
{
  "type": "file",
  "path": "文件路径",
  "uri": "file://文件路径",
  "content": "文件内容",
  "size": 1024
}
```

## 架构设计

### 核心组件

1. **MCPServer**: 主要的服务器类
2. **MCPServerRequestHandler**: 处理transport层的请求
3. **Transport集成**: 使用最新的transport模块进行通信

### 请求处理流程

```
Client Request → Transport → MCPServerRequestHandler → MCPServer::Impl → Response
```

### 错误处理

- 统一的异常处理机制
- 详细的错误日志记录
- 友好的错误消息返回

## 配置选项

### 服务器配置

```cpp
// 设置服务器信息
server.setServerInfo("MCP FileSystem Server", "1.0.0");

// 设置服务器能力
ServerCapabilities capabilities;
capabilities.tools = json{{"listChanged", true}};
capabilities.resources = json{{"subscribe", true}, {"listChanged", true}};
server.setCapabilities(capabilities);
```

### Transport配置

```cpp
mcp_transport::TransportConfig config;
config.type = mcp_transport::TransportType::AUTO;
config.mode = mcp_transport::TransportMode::SERVER;
config.enable_logging = true;
config.log_level = 1;
```

## 日志系统

服务器使用统一的日志系统，支持：

- 多级别日志（DEBUG, INFO, WARNING, ERROR）
- 文件和控制台输出
- 模块化日志记录
- 自动时间戳

### 日志配置示例

```cpp
mcp_logger::LogConfig config;
config.level = mcp_logger::LogLevel::INFO;
config.target = mcp_logger::LogTarget::BOTH;
config.file_path = "logs/mcp_server_filesystem.log";
config.enable_timestamp = true;
config.format = "[{timestamp}] [{level}] {message}";
config.auto_flush = true;
```

## 测试

### 运行测试

```bash
make server_test
./bin/server_test
```

### 手动测试

1. 启动服务器：`./bin/server_filesystem_example`
2. 使用客户端连接：`./bin/mcp_client_ipc_demo`
3. 测试各种工具和资源操作

## 使用继承架构

### 创建自定义服务器子类

```cpp
#include "mcp_server.h"

class MyCustomServer : public mcp::MCPServer {
public:
    MyCustomServer() : MCPServer() {
        // 设置服务器信息
        setServerInfo("My Custom Server", "1.0.0");
        
        // 设置自定义工具和资源
        setupDefaultTools();
        setupDefaultResources();
        
        // 设置自定义处理器
        setToolCallHandler([this](const ToolCallParams& params) -> ToolCallResult {
            return handleCustomToolCall(params);
        });
    }

protected:
    // 重写基类的虚拟方法
    void setupDefaultTools() override {
        // 添加自定义工具
        Tool customTool;
        customTool.name = "my_tool";
        customTool.description = "My custom tool";
        customTool.inputSchema = {
            {"type", "object"},
            {"properties", {
                {"param", {{"type", "string"}, {"description", "Parameter"}}}
            }},
            {"required", {"param"}}
        };
        addTool(customTool);
    }
    
    void setupDefaultResources() override {
        // 添加自定义资源
        Resource customResource;
        customResource.uri = "custom://resource";
        customResource.name = "Custom Resource";
        customResource.description = "My custom resource";
        addResource(customResource);
    }

private:
    ToolCallResult handleCustomToolCall(const ToolCallParams& params) {
        ToolCallResult result;
        
        if (params.name == "my_tool") {
            // 处理自定义工具调用
            result.content = json{{"message", "Custom tool executed"}};
        } else {
            result.isError = true;
            result.errorMessage = "Unknown tool: " + params.name;
        }
        
        return result;
    }
};
```

### 使用MCPFileSystemServer

```cpp
#include "mcp_filesystem_server.h"

// 创建只读文件系统服务器
mcp::MCPFileSystemServer server("/path/to/root", false);

// 添加自定义资源
server.addFileSystemResource("docs", "文档目录", "项目文档");

// 启动服务器
server.start("");
```

## 扩展开发

### 添加新工具

1. 在 `setupDefaultTools()` 中添加工具定义
2. 在工具调用处理器中添加处理逻辑
3. 更新CMakeLists.txt（如果需要）

### 添加新资源

1. 在 `setupDefaultResources()` 中添加资源定义
2. 在资源读取处理器中添加处理逻辑
3. 实现相应的URI协议支持

### 自定义处理器

```cpp
// 设置自定义工具调用处理器
server.setToolCallHandler([](const ToolCallParams& params) -> ToolCallResult {
    // 自定义处理逻辑
    ToolCallResult result;
    // ...
    return result;
});

// 设置自定义资源读取处理器
server.setResourceReadHandler([](const std::string& uri) -> json {
    // 自定义处理逻辑
    json result;
    // ...
    return result;
});
```

## 故障排除

### 常见问题

1. **编译错误**: 确保所有依赖模块已正确编译
2. **连接失败**: 检查transport配置和端口设置
3. **权限错误**: 确保有足够的文件系统访问权限
4. **日志问题**: 检查日志目录是否存在且可写

### 调试技巧

1. 启用详细日志：设置 `log_level = 0` (DEBUG)
2. 检查transport状态：使用 `transport.getStatus()`
3. 查看连接信息：使用 `transport.getConnectionInfo()`

## 许可证

本项目遵循项目根目录的许可证条款。

## 贡献

欢迎提交Issue和Pull Request来改进这个实现。

## 更新日志

### v1.0.0
- 基于最新transport模块重构
- 实现完整的文件系统功能
- 添加统一的错误处理
- 完善日志系统集成
- 提供完整的示例和文档
