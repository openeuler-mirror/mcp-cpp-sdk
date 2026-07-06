# StdioTransport 客户端-服务器通信示例

本示例展示了如何使用 `StdioTransport` 实现客户端和服务器之间的进程间通信（IPC）。

## 文件说明

- `stdio_server_example.cpp` - 服务器程序，通过 stdin/stdout 接收和处理请求
- `stdio_client_example.cpp` - 客户端程序，启动服务器进程并与之通信

## 编译

确保已编译 transport_new 模块，然后编译示例：

```bash
# 在 transport_new 目录下
mkdir -p build
cd build
cmake ..
make stdio_server_example stdio_client_example
```

## 运行

### 方式1：分别运行（推荐用于测试）

**终端1 - 运行服务器：**
```bash
cd build
./stdio_server_example
```

**终端2 - 运行客户端：**
```bash
cd build
./stdio_client_example ./stdio_server_example
```

### 方式2：客户端自动启动服务器（推荐用于实际使用）

客户端会自动启动服务器进程，只需运行：

```bash
cd build
./stdio_client_example
```

或者指定服务器路径：

```bash
./stdio_client_example /path/to/stdio_server_example
```

## 功能说明

### 服务器支持的方法

1. **ping** - 返回 pong 响应和当前时间戳
   ```json
   {
     "jsonrpc": "2.0",
     "id": "ping_1",
     "method": "ping",
     "params": {}
   }
   ```

2. **echo** - 回显参数
   ```json
   {
     "jsonrpc": "2.0",
     "id": "echo_1",
     "method": "echo",
     "params": "Hello from client!"
   }
   ```

3. **add** - 计算两个数的和
   ```json
   {
     "jsonrpc": "2.0",
     "id": "add_1",
     "method": "add",
     "params": {"a": 42, "b": 58}
   }
   ```

4. **get_time** - 获取当前时间
   ```json
   {
     "jsonrpc": "2.0",
     "id": "time_1",
     "method": "get_time",
     "params": {}
   }
   ```

5. **log** - 通知方法（无响应）
   ```json
   {
     "jsonrpc": "2.0",
     "method": "log",
     "params": {"message": "This is a test log message"}
   }
   ```

## 工作原理

### 服务器模式

- 服务器进程通过 `stdin` 接收 JSON-RPC 请求
- 处理请求后，通过 `stdout` 发送 JSON-RPC 响应
- 使用 `TransportMode::SERVER` 模式
- `config.ipc.command` 设置为空字符串（表示当前进程就是服务器）

### 客户端模式

- 客户端使用 `config.ipc.command` 指定的路径启动服务器进程
- 通过管道与服务器进程的 `stdin/stdout` 通信
- 使用 `TransportMode::CLIENT` 模式
- 客户端负责管理服务器进程的生命周期

## 日志

日志文件保存在 `logs/` 目录：
- `stdio_server_example.log` - 服务器日志
- `stdio_client_example.log` - 客户端日志

## 注意事项

1. 确保服务器程序可执行且有执行权限
2. 服务器和客户端使用 JSON-RPC 2.0 协议通信
3. 所有消息通过 stdin/stdout 传输，使用换行符分隔
4. 服务器会持续运行直到收到 SIGINT 或 SIGTERM 信号

## 扩展

你可以：

1. 添加更多的方法到 `ExampleServerRequestHandler`
2. 修改客户端测试不同的请求
3. 使用 `TransportFactory` 创建传输实例
4. 自定义配置参数（缓冲区大小、超时时间等）

