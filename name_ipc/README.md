# Named Pipe (命名管道) 模块

这个模块实现了跨平台的命名管道（Named Pipe/FIFO）通信功能。

## 功能特性

- ✅ 跨平台支持（Windows 和 POSIX 系统）
- ✅ 支持只读、只写和双工模式
- ✅ 异步消息处理
- ✅ 消息队列管理
- ✅ JSON 消息支持
- ✅ 连接状态管理
- ✅ 线程安全

## 平台差异

### Windows
- 使用 `CreateNamedPipe` 和 `CreateFile` API
- 管道路径格式：`\\.\pipe\pipe_name`
- 支持双工模式

### POSIX (Linux/macOS)
- 使用 `mkfifo` 创建 FIFO
- 管道路径格式：`/tmp/pipe_name` 或任意文件系统路径
- 双工模式通过 `O_RDWR` 实现

## 使用方法

### 基本示例

#### 服务器端（读取）

```cpp
#include "named_pipe.h"
#include "../../log/include/logger.h"

using namespace mcp;

int main() {
    // 初始化日志
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    // 创建命名管道
    NamedPipe server;
    
    // 配置（服务器模式 - 读取）
    NamedPipeConfig config;
    config.pipe_path = "/tmp/mcp_named_pipe";
    config.mode = NamedPipeMode::READ;
    config.buffer_size = 4096;
    config.auto_create = true;  // 自动创建管道
    
    // 启动
    if (!server.start(config)) {
        return 1;
    }
    
    // 接收消息
    while (server.isRunning()) {
        if (server.hasMessage()) {
            NamedPipeMessage msg = server.getMessage();
            std::cout << "Received: " << msg.content << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    server.stop();
    return 0;
}
```

#### 客户端（写入）

```cpp
#include "named_pipe.h"
#include "../../log/include/logger.h"

using namespace mcp;

int main() {
    // 初始化日志
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    // 创建命名管道
    NamedPipe client;
    
    // 配置（客户端模式 - 写入）
    NamedPipeConfig config;
    config.pipe_path = "/tmp/mcp_named_pipe";
    config.mode = NamedPipeMode::WRITE;
    config.auto_create = false;  // 不创建管道，只连接
    
    // 启动
    if (!client.start(config)) {
        return 1;
    }
    
    // 等待连接
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 发送消息
    client.sendMessage("Hello from client!");
    client.sendMessage(R"({"jsonrpc": "2.0", "id": 1, "method": "test"})");
    
    client.stop();
    return 0;
}
```

### 使用消息处理器

```cpp
class MyMessageHandler : public NamedPipeMessageHandler {
public:
    void onMessage(const NamedPipeMessage& message) override {
        std::cout << "Got message: " << message.content << std::endl;
        if (!message.id.empty()) {
            std::cout << "Message ID: " << message.id << std::endl;
        }
    }
    
    void onError(const std::string& error) override {
        std::cerr << "Error: " << error << std::endl;
    }
    
    void onConnected() override {
        std::cout << "Connected!" << std::endl;
    }
    
    void onDisconnected() override {
        std::cout << "Disconnected!" << std::endl;
    }
};

// 使用
auto handler = std::make_shared<MyMessageHandler>();
named_pipe.setMessageHandler(handler);
```

## 配置选项

### NamedPipeConfig

- `pipe_path`: 命名管道的路径
  - Windows: `\\.\pipe\pipe_name` 或 `pipe_name`（会自动添加前缀）
  - POSIX: `/tmp/pipe_name` 或任意文件系统路径

- `mode`: 管道模式
  - `NamedPipeMode::READ`: 只读模式（服务器端）
  - `NamedPipeMode::WRITE`: 只写模式（客户端）
  - `NamedPipeMode::DUPLEX`: 双工模式

- `buffer_size`: 缓冲区大小（默认 4096 字节）

- `read_timeout_ms`: 读取超时时间（毫秒，默认 1000）

- `write_timeout_ms`: 写入超时时间（毫秒，默认 1000）

- `non_blocking`: 非阻塞模式（默认 false）

- `auto_create`: 自动创建管道（默认 true）
  - 服务器端应设置为 `true`
  - 客户端应设置为 `false`

- `max_instances`: 最大实例数（仅 Windows，默认 1）

## 构建

```bash
cd name_ipc
mkdir -p build && cd build
cmake ..
make
```

## 运行示例

### 终端 1 - 启动服务器

```bash
./bin/named_pipe_server_example
```

### 终端 2 - 启动客户端

```bash
./bin/named_pipe_client_example
```

## 运行测试

```bash
cd build
ctest -R named_pipe_test
# 或直接运行
./bin/named_pipe_test
```

## API 参考

### NamedPipe 类

#### 启动和停止
- `bool start(const NamedPipeConfig& config)`: 启动命名管道
- `void stop()`: 停止命名管道
- `bool isRunning() const`: 检查是否运行中
- `bool isConnected() const`: 检查是否已连接

#### 消息发送
- `bool sendMessage(const std::string& message)`: 发送字符串消息
- `bool sendMessage(const NamedPipeMessage& message)`: 发送消息对象

#### 消息接收
- `bool hasMessage() const`: 检查是否有消息
- `NamedPipeMessage getMessage()`: 获取一条消息
- `std::vector<NamedPipeMessage> getAllMessages()`: 获取所有消息

#### 消息处理
- `void setMessageHandler(std::shared_ptr<NamedPipeMessageHandler> handler)`: 设置消息处理器

#### 配置信息
- `std::string getPipePath() const`: 获取管道路径
- `NamedPipeMode getMode() const`: 获取管道模式

## 注意事项

1. **管道创建顺序**：服务器端应先启动并创建管道，然后客户端再连接
2. **路径格式**：Windows 和 POSIX 的路径格式不同，注意区分
3. **权限问题**：POSIX 系统上，确保对管道路径有读写权限
4. **清理**：程序退出时会自动清理创建的管道文件（POSIX）
5. **线程安全**：所有公共方法都是线程安全的

## 与匿名管道的区别

| 特性 | 匿名管道 | 命名管道 |
|------|---------|---------|
| 创建方式 | `pipe()` | `mkfifo()` / `CreateNamedPipe()` |
| 进程关系 | 必须有父子关系 | 任意进程间通信 |
| 访问方式 | 文件描述符 | 文件系统路径 |
| 持久性 | 进程退出即销毁 | 文件系统存在 |

## 许可证

与主项目保持一致。

