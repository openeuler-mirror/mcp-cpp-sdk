# PipeCommunication 日志功能

## 概述

本次更新为 `PipeCommunication` 类添加了完整的日志功能，用于记录多进程通信过程中的各种事件和子进程输出。

## 新增功能

### 1. 日志级别
- `DEBUG`: 详细的调试信息
- `INFO`: 一般信息
- `WARNING`: 警告信息
- `ERROR`: 错误信息

### 2. 日志配置
在 `PipeConfig` 结构体中新增了以下配置选项：
```cpp
struct PipeConfig {
    // ... 原有配置 ...
    bool enable_logging = true;           // 是否启用日志
    std::string log_file_path = "";       // 日志文件路径（空字符串表示输出到控制台）
    LogLevel log_level = LogLevel::INFO;  // 日志级别
};
```

### 3. 日志方法
```cpp
// 设置日志级别
void set_log_level(LogLevel level);

// 设置日志文件
void set_log_file(const std::string& file_path);

// 启用/禁用日志
void enable_logging(bool enable);

// 手动记录日志
void log(LogLevel level, const std::string& message);
```

### 4. 子进程输出记录
- 自动记录子进程的原始输出
- 记录处理后的消息
- 区分不同类型的输出（raw_output, processed_message）

## 使用示例

### 基本使用
```cpp
#include "pipe_communication.h"

using namespace mcp;

int main() {
    PipeCommunication pipe_comm;
    
    // 配置日志
    pipe_comm.set_log_level(LogLevel::DEBUG);
    pipe_comm.set_log_file("communication.log");
    pipe_comm.enable_logging(true);
    
    // 配置管道通信
    PipeConfig config;
    config.command = "python3";
    config.args = {"-c", "print('Hello World')"};
    config.enable_logging = true;
    config.log_level = LogLevel::DEBUG;
    config.log_file_path = "subprocess.log";
    
    // 启动通信
    if (pipe_comm.start(config)) {
        // 处理消息...
        pipe_comm.stop();
    }
    
    return 0;
}
```

### 日志输出格式
```
[2024-01-15 10:30:45.123] [INFO] Starting PipeCommunication with command: python3
[2024-01-15 10:30:45.124] [DEBUG] Creating pipes on POSIX
[2024-01-15 10:30:45.125] [DEBUG] Pipes created successfully on POSIX
[2024-01-15 10:30:45.126] [DEBUG] Starting process on POSIX
[2024-01-15 10:30:45.127] [DEBUG] Child process executing
[2024-01-15 10:30:45.128] [DEBUG] Executing command: python3 -c print('Hello World')
[2024-01-15 10:30:45.129] [INFO] Process started successfully on POSIX, PID: 12345
[2024-01-15 10:30:45.130] [INFO] Read thread started
[2024-01-15 10:30:45.200] [SUBPROCESS-raw_output] Hello World
[2024-01-15 10:30:45.201] [SUBPROCESS-processed_message] Hello World
[2024-01-15 10:30:45.202] [INFO] Read thread finished
[2024-01-15 10:30:45.203] [INFO] Stopping PipeCommunication
[2024-01-15 10:30:45.204] [DEBUG] Stopping process, PID: 12345
[2024-01-15 10:30:45.205] [DEBUG] Process stopped
[2024-01-15 10:30:45.206] [INFO] PipeCommunication stopped
```

## 改进的错误处理

### 1. 进程启动错误
- 记录详细的错误信息
- 区分不同平台的错误代码
- 提供有意义的错误消息

### 2. 管道操作错误
- 记录管道创建失败的原因
- 监控管道读写操作
- 处理管道关闭事件

### 3. 子进程监控
- 记录子进程的启动和停止
- 监控子进程的输出
- 处理子进程异常退出

## 测试

运行测试示例：
```bash
cd cpp-mcp
mkdir build && cd build
cmake ..
make logging_test_example
./logging_test_example
```

测试将创建两个日志文件：
- `pipe_communication.log`: 主进程的日志
- `subprocess.log`: 子进程输出的日志

## 注意事项

1. 日志功能是线程安全的，可以在多线程环境中安全使用
2. 日志文件会自动创建，如果目录不存在会失败
3. 建议在生产环境中设置合适的日志级别以减少性能影响
4. 子进程的日志记录可能会影响性能，可以根据需要禁用
