# Pipe Communication Module

这是一个独立的管道通信模块，用于在C++应用程序中与子进程进行通信。该模块提供了跨平台的管道创建、进程管理和消息传输功能。

## 功能特性

- **跨平台支持**: 支持Windows和POSIX系统
- **异步消息处理**: 使用独立线程处理消息接收
- **请求-响应模式**: 支持同步和异步的请求-响应通信
- **JSON-RPC支持**: 内置JSON-RPC 2.0协议支持
- **错误处理**: 完善的错误处理和超时机制
- **线程安全**: 所有操作都是线程安全的

## 文件结构

```
cpp-mcp/
├── include/
│   └── pipe_communication.h    # 头文件
├── src/
│   └── pipe_communication.cpp  # 实现文件
├── examples/
│   └── pipe_communication_example.cpp  # 使用示例
├── test/
│   └── pipe_communication_test.cpp     # 测试文件
├── CMakeLists_pipe.txt         # CMake构建文件
└── PIPE_COMMUNICATION_README.md # 本文档
```

## 快速开始

### 1. 基本使用

```cpp
#include "pipe_communication.h"
#include <iostream>

using namespace mcp;

int main() {
    // 创建管道通信实例
    PipeCommunication pipe_comm;
    
    // 配置管道通信
    PipeConfig config;
    config.command = "python";
    config.args = {"-c", "import sys; print(sys.stdin.read())"};
    config.env_vars["PYTHONUNBUFFERED"] = "1";
    
    // 启动管道通信
    if (pipe_comm.start(config)) {
        std::cout << "Pipe communication started" << std::endl;
        
        // 发送消息
        pipe_comm.send_message("Hello, World!");
        
        // 停止管道通信
        pipe_comm.stop();
    }
    
    return 0;
}
```

### 2. 使用消息处理器

```cpp
class MyMessageHandler : public MessageHandler {
public:
    void on_message(const Message& message) override {
        std::cout << "Received: " << message.content << std::endl;
    }
    
    void on_error(const std::string& error) override {
        std::cerr << "Error: " << error << std::endl;
    }
};

// 设置消息处理器
auto handler = std::make_shared<MyMessageHandler>();
pipe_comm.set_message_handler(handler);
```

### 3. 请求-响应模式

```cpp
// 发送请求并等待响应
std::string request = R"({"jsonrpc": "2.0", "method": "echo", "params": {"message": "Hello"}})";
auto future_response = pipe_comm.send_request(request, 5); // 5秒超时

try {
    auto response = future_response.get();
    std::cout << "Response: " << response << std::endl;
} catch (const std::exception& e) {
    std::cout << "Request failed: " << e.what() << std::endl;
}
```

## 编译和构建

### 使用CMake

```bash
# 创建构建目录
mkdir build && cd build

# 配置项目
cmake -f ../CMakeLists_pipe.txt ..

# 编译
make

# 运行示例
./pipe_communication_example

# 运行测试
./pipe_communication_test
```

### 手动编译

```bash
# 编译库
g++ -std=c++17 -I./include -I./common -c src/pipe_communication.cpp -o pipe_communication.o

# 编译示例
g++ -std=c++17 -I./include -I./common examples/pipe_communication_example.cpp pipe_communication.o -o example

# 运行
./example
```

## API 参考

### PipeConfig 结构体

```cpp
struct PipeConfig {
    std::string command;                                    // 要执行的命令
    std::vector<std::string> args;                         // 命令参数
    std::map<std::string, std::string> env_vars;          // 环境变量
    int buffer_size = 4096;                                // 缓冲区大小
    int read_timeout_ms = 10;                              // 读取超时（毫秒）
    int max_retry_count = 10;                              // 最大重试次数
};
```

### PipeCommunication 类

#### 主要方法

- `bool start(const PipeConfig& config)` - 启动管道通信
- `void stop()` - 停止管道通信
- `bool is_running() const` - 检查是否正在运行
- `bool send_message(const std::string& message)` - 发送消息
- `std::future<std::string> send_request(const std::string& request, int timeout_seconds = 60)` - 发送请求并等待响应

#### 进程管理

- `int get_process_id() const` - 获取子进程ID
- `bool is_process_alive() const` - 检查子进程是否存活

#### 消息处理

- `void set_message_handler(std::shared_ptr<MessageHandler> handler)` - 设置消息处理器

### MessageHandler 接口

```cpp
class MessageHandler {
public:
    virtual ~MessageHandler() = default;
    virtual void on_message(const Message& message) = 0;
    virtual void on_error(const std::string& error) = 0;
};
```

## 注意事项

1. **线程安全**: 所有公共方法都是线程安全的
2. **资源管理**: 确保在程序退出前调用 `stop()` 方法
3. **错误处理**: 建议实现 `MessageHandler` 来处理错误情况
4. **超时设置**: 根据实际需求调整超时时间
5. **平台差异**: Windows和POSIX系统的实现略有不同，但API保持一致

## 故障排除

### 常见问题

1. **进程启动失败**: 检查命令路径和参数是否正确
2. **消息丢失**: 检查缓冲区大小和超时设置
3. **编译错误**: 确保C++17支持和必要的依赖库

### 调试技巧

1. 启用详细日志输出
2. 检查子进程的退出状态
3. 验证JSON格式的消息
4. 使用简单的测试命令（如 `echo` 或 `cat`）进行调试

## 许可证

本模块遵循与主项目相同的许可证。
