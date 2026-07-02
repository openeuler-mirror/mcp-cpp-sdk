# MCP Transport 模块

MCP Transport 是一个统一的传输层库，支持 IPC（进程间通信）和 HTTP/HTTPS 两种传输方式。它封装了现有的 IPC 和 HTTP 模块，提供了统一的接口来简化 MCP 客户端的开发。

## 特性

- **统一接口**: 提供一致的 API 来使用 IPC 和 HTTP 传输
- **自动选择**: 支持根据配置自动选择最适合的传输方式
- **配置灵活**: 支持 JSON 配置文件或代码配置
- **异步支持**: 支持异步和同步消息发送
- **回调机制**: 支持消息、连接和错误回调
- **日志记录**: 内置日志系统，支持文件和控制台输出
- **统计信息**: 提供连接和消息统计
- **健康检查**: 支持连接健康状态检查

## 快速开始

### 1. 构建

```bash
cd cpp_mcp_sdk/transport
mkdir build && cd build
cmake ..
make
```

### 2. 基本使用

#### 使用 IPC 传输

```cpp
#include "transport.h"

using namespace mcp_transport;

// 创建 IPC 配置
auto config = utils::create_ipc_config("npx", 
    {"-y", "@modelcontextprotocol/server-filesystem", "."},
    {{"NODE_ENV", "development"}});

// 创建传输实例
auto transport = std::make_unique<Transport>();

// 启动传输
if (transport->start(config)) {
    // 发送消息
    auto response = transport->send_request_sync(R"({
        "jsonrpc": "2.0",
        "method": "tools/list",
        "id": 1,
        "params": {}
    })");
    
    std::cout << "响应: " << response.content << std::endl;
    
    // 停止传输
    transport->stop();
}
```

#### 使用 HTTP 传输

```cpp
#include "transport.h"

using namespace mcp_transport;

// 创建 HTTP 配置
auto config = utils::create_https_config("mcp.didichuxing.com", 443);
config.http.timeout_seconds = 30;
config.http.user_agent = "MyApp/1.0";

// 创建传输实例
auto transport = std::make_unique<Transport>();

// 启动传输
if (transport->start(config)) {
    // 发送消息
    auto response = transport->send_request_sync(R"({
        "jsonrpc": "2.0",
        "method": "tools/list",
        "id": 1,
        "params": {}
    })", "POST", "/mcp-servers?key=YOUR_KEY");
    
    std::cout << "响应: " << response.content << std::endl;
    
    // 停止传输
    transport->stop();
}
```

#### 使用配置文件

```cpp
#include "transport.h"

using namespace mcp_transport;

// 从配置文件加载
auto config = TransportConfig::from_json_file("transport_config.json");

// 创建传输实例
auto transport = std::make_unique<Transport>();

// 设置回调
transport->set_message_callback([](const TransportMessage& msg) {
    std::cout << "收到消息: " << msg.content << std::endl;
});

transport->set_connection_callback([](const ConnectionInfo& info) {
    std::cout << "连接状态: " << static_cast<int>(info.status) << std::endl;
});

transport->set_error_callback([](const std::string& error) {
    std::cerr << "错误: " << error << std::endl;
});

// 启动传输
if (transport->start(config)) {
    // 使用传输...
    transport->stop();
}
```

### 3. 运行示例

```bash
# 运行所有测试
./transport_example

# 运行 IPC 测试
./transport_example ipc

# 运行 HTTP 测试
./transport_example http

# 运行配置测试
./transport_example config

# 运行交互式测试
./transport_example interactive
```

## 配置

### JSON 配置文件格式

```json
{
    "type": 0,
    "name": "mcp_transport",
    "enable_logging": true,
    "log_level": 1,
    "log_file_path": "transport.log",
    "ipc": {
        "command": "npx",
        "args": ["-y", "@modelcontextprotocol/server-filesystem", "."],
        "env_vars": {
            "NODE_ENV": "development"
        },
        "buffer_size": 4096,
        "read_timeout_ms": 100,
        "max_retry_count": 10,
        "enable_logging": true,
        "log_file_path": "ipc.log",
        "log_level": 1
    },
    "http": {
        "host": "mcp.didichuxing.com",
        "port": 443,
        "protocol": 1,
        "timeout_seconds": 30,
        "max_retries": 3,
        "user_agent": "MCP-Transport/1.0",
        "keep_alive": true,
        "auto_reconnect": false,
        "reconnect_interval": 5,
        "headers": {
            "Content-Type": "application/json",
            "Accept": "application/json"
        }
    }
}
```

### 配置参数说明

#### 通用配置
- `type`: 传输类型 (0=IPC, 1=HTTP, 2=AUTO)
- `name`: 传输实例名称
- `enable_logging`: 是否启用日志
- `log_level`: 日志级别 (0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR)
- `log_file_path`: 日志文件路径

#### IPC 配置
- `command`: 要执行的命令
- `args`: 命令参数列表
- `env_vars`: 环境变量
- `buffer_size`: 缓冲区大小
- `read_timeout_ms`: 读取超时时间（毫秒）
- `max_retry_count`: 最大重试次数

#### HTTP 配置
- `host`: 服务器主机名
- `port`: 服务器端口
- `protocol`: 协议类型 (0=HTTP, 1=HTTPS)
- `timeout_seconds`: 超时时间（秒）
- `max_retries`: 最大重试次数
- `user_agent`: 用户代理字符串
- `keep_alive`: 是否保持连接
- `auto_reconnect`: 是否自动重连
- `reconnect_interval`: 重连间隔（秒）
- `headers`: 默认请求头

## API 参考

### Transport 类

#### 连接管理
- `bool start(const TransportConfig& config)`: 启动传输
- `bool stop()`: 停止传输
- `bool is_running() const`: 检查是否运行中
- `TransportStatus get_status() const`: 获取传输状态
- `ConnectionInfo get_connection_info() const`: 获取连接信息

#### 消息发送
- `std::future<TransportMessage> send_request(const TransportMessage& message)`: 异步发送消息
- `std::future<TransportMessage> send_request(const std::string& content, const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers)`: 异步发送消息（简化版）
- `TransportMessage send_request_sync(const TransportMessage& message)`: 同步发送消息
- `TransportMessage send_request_sync(const std::string& content, const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers)`: 同步发送消息（简化版）

#### 消息接收
- `bool has_message() const`: 检查是否有消息
- `TransportMessage get_message()`: 获取一条消息
- `std::vector<TransportMessage> get_all_messages()`: 获取所有消息

#### 回调设置
- `void set_message_callback(MessageCallback callback)`: 设置消息回调
- `void set_connection_callback(ConnectionCallback callback)`: 设置连接回调
- `void set_error_callback(ErrorCallback callback)`: 设置错误回调

#### 配置管理
- `void update_config(const TransportConfig& config)`: 更新配置
- `TransportConfig get_config() const`: 获取当前配置

#### 统计和健康检查
- `nlohmann::json get_statistics() const`: 获取统计信息
- `bool health_check()`: 健康检查

#### 日志功能
- `void log(int level, const std::string& message)`: 记录日志
- `void set_log_level(int level)`: 设置日志级别
- `void set_log_file(const std::string& file_path)`: 设置日志文件
- `void enable_logging(bool enable)`: 启用/禁用日志

### 工具函数

#### 配置创建
- `TransportConfig create_ipc_config(const std::string& command, const std::vector<std::string>& args, const std::map<std::string, std::string>& env_vars)`: 创建 IPC 配置
- `TransportConfig create_http_config(const std::string& host, int port, netio::Protocol protocol)`: 创建 HTTP 配置
- `TransportConfig create_https_config(const std::string& host, int port)`: 创建 HTTPS 配置

#### 消息创建
- `TransportMessage create_request(const std::string& content, const std::string& method, const std::string& path, const std::map<std::string, std::string>& headers)`: 创建请求消息
- `TransportMessage create_response(const std::string& content, int status_code, const std::map<std::string, std::string>& headers)`: 创建响应消息
- `TransportMessage create_error(const std::string& error_message, int status_code, const std::map<std::string, std::string>& headers)`: 创建错误消息

#### 验证函数
- `bool validate_config(const TransportConfig& config)`: 验证配置
- `bool validate_message(const TransportMessage& message)`: 验证消息

## 依赖

- C++17 或更高版本
- CMake 3.10 或更高版本
- nlohmann/json 库
- httplib 库
- IPC 模块 (pipe_communication)
- HTTP 模块 (netio)

## 构建选项

- `CMAKE_BUILD_TYPE`: 构建类型 (Debug/Release)
- `CMAKE_INSTALL_PREFIX`: 安装前缀
- `CMAKE_CXX_STANDARD`: C++ 标准版本

## 示例

查看 `transport_example.cpp` 文件获取完整的使用示例。

## 许可证

本项目采用 MIT 许可证。详见 LICENSE 文件。

## 贡献

欢迎提交 Issue 和 Pull Request 来改进本项目。

## 联系方式

如有问题，请通过以下方式联系：

- 项目主页: https://github.com/mcp-project/mcp-transport
- 邮箱: mcp@example.com
- 问题反馈: https://github.com/mcp-project/mcp-transport/issues
