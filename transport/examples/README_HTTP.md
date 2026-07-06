# HttpTransport 客户端-服务器通信示例

本示例展示了如何使用 `HttpTransport` 实现客户端和服务器之间的 HTTP 通信。

## 文件说明

- `http_server_example.cpp` - HTTP 服务器程序，监听 HTTP 请求并处理 JSON-RPC 消息
- `http_client_example.cpp` - HTTP 客户端程序，连接到服务器并发送请求

## 编译

确保已编译 transport_new 模块，然后编译示例：

```bash
# 在 transport_new 目录下
mkdir -p build
cd build
cmake ..
make http_server_example http_client_example
```

## 运行

### 方式1：分别运行（推荐）

**终端1 - 运行服务器：**
```bash
cd build
./http_server_example [host] [port] [endpoint]
# 默认: localhost 8080 /
# 示例: ./http_server_example 0.0.0.0 8080 /api
```

**终端2 - 运行客户端：**
```bash
cd build
./http_client_example [host] [port] [endpoint]
# 默认: localhost 8080 /
# 示例: ./http_client_example localhost 8080 /api
```

### 方式2：使用默认配置

```bash
# 终端1
./http_server_example

# 终端2
./http_client_example
```

## 功能说明

### 服务器支持的方法

服务器实现了以下 JSON-RPC 2.0 方法：

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
     "params": "Hello from HTTP client!"
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

- 服务器使用 `HttpTransport` 在服务器模式下运行
- 通过 NetIO 模块监听指定端口（默认 8080）
- 接收 HTTP POST 请求，请求体包含 JSON-RPC 消息
- 处理请求后，返回 JSON-RPC 响应
- 使用 `SseServerRequestHandler` 处理请求

### 客户端模式

- 客户端使用 `HttpTransport` 在客户端模式下运行
- 连接到指定的服务器地址和端口
- 通过 HTTP POST 发送 JSON-RPC 请求
- 接收并解析 JSON-RPC 响应

## 配置参数

### 服务器配置

- `host`: 监听地址（默认: "localhost"）
- `port`: 监听端口（默认: 8080）
- `endpoint`: HTTP 端点路径（默认: "/"）
- `protocol`: 协议类型（HTTP 或 HTTPS）

### 客户端配置

- `host`: 服务器地址（默认: "localhost"）
- `port`: 服务器端口（默认: 8080）
- `endpoint`: HTTP 端点路径（默认: "/"）
- `timeout_seconds`: 请求超时时间（默认: 30秒）
- `keep_alive`: 保持连接（默认: true）

## 日志

日志文件保存在 `logs/` 目录：
- `http_server_example.log` - 服务器日志
- `http_client_example.log` - 客户端日志

## 测试

客户端会自动执行以下测试：

1. Ping 请求测试
2. Echo 请求测试
3. Add 请求测试（42 + 58 = 100）
4. Get Time 请求测试
5. Log 通知测试

## 使用 curl 测试服务器

你也可以使用 curl 直接测试服务器：

```bash
# Ping 请求
curl -X POST http://localhost:8080/ \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"ping","params":{}}'

# Echo 请求
curl -X POST http://localhost:8080/ \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"2","method":"echo","params":"Hello World"}'

# Add 请求
curl -X POST http://localhost:8080/ \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"3","method":"add","params":{"a":10,"b":20}}'
```

## 注意事项

1. 确保端口未被占用（默认 8080）
2. 服务器和客户端使用 JSON-RPC 2.0 协议通信
3. 所有请求使用 HTTP POST 方法
4. 请求和响应都使用 JSON 格式
5. 服务器会持续运行直到收到 SIGINT 或 SIGTERM 信号

## 扩展

你可以：

1. 添加更多的方法到 `ExampleSseServerRequestHandler`
2. 修改客户端测试不同的请求
3. 使用 `TransportFactory` 创建传输实例
4. 自定义配置参数（超时时间、重试次数等）
5. 实现 HTTPS 支持（使用 `HttpsTransport`）

## 与 StdioTransport 的区别

- **StdioTransport**: 使用进程间通信（stdin/stdout），适用于本地进程通信
- **HttpTransport**: 使用 HTTP 协议，适用于网络通信，可以跨机器通信

