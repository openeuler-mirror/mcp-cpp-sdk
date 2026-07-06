# HttpsTransport 客户端-服务器通信示例

本示例展示了如何使用 `HttpsTransport` 实现客户端和服务器之间的 HTTPS（加密的 HTTP）通信。

## 文件说明

- `https_server_example.cpp` - HTTPS 服务器程序，监听 HTTPS 请求并处理 JSON-RPC 消息
- `https_client_example.cpp` - HTTPS 客户端程序，连接到服务器并发送请求

## 编译

确保已编译 transport_new 模块，然后编译示例：

```bash
# 在 transport_new 目录下
mkdir -p build
cd build
cmake ..
make https_server_example https_client_example
```

## SSL/TLS 证书配置

HTTPS 需要 SSL/TLS 证书。你可以：

1. **使用自签名证书（用于测试）**
2. **使用 CA 签发的证书（用于生产环境）**

### 生成自签名证书（用于测试）

```bash
# 生成私钥
openssl genrsa -out server.key 2048

# 生成证书签名请求
openssl req -new -key server.key -out server.csr

# 生成自签名证书（有效期1年）
openssl x509 -req -days 365 -in server.csr -signkey server.key -out server.crt

# 清理临时文件
rm server.csr
```

## 运行

### 方式1：使用自签名证书（推荐用于测试）

**终端1 - 运行服务器：**
```bash
cd build
./https_server_example [host] [port] [endpoint] [cert_file] [key_file] [ca_file] [verify_peer] [verify_hostname]
# 默认: localhost 8443 / "" "" "" true true
# 示例: ./https_server_example 0.0.0.0 8443 / server.crt server.key "" true true
```

**终端2 - 运行客户端（禁用证书验证，用于自签名证书）：**
```bash
cd build
./https_client_example [host] [port] [endpoint] [ca_file] [verify_peer] [verify_hostname]
# 默认: localhost 8443 / "" true true
# 示例（禁用验证）: ./https_client_example localhost 8443 / "" false false
```

### 方式2：使用默认配置（需要先配置证书）

```bash
# 终端1 - 服务器（需要提供证书文件）
./https_server_example localhost 8443 / server.crt server.key

# 终端2 - 客户端（禁用验证以使用自签名证书）
./https_client_example localhost 8443 / "" false false
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
     "params": "Hello from HTTPS client!"
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

- 服务器使用 `HttpsTransport` 在服务器模式下运行
- 通过 NetIO 模块监听指定端口（默认 8443）
- 使用 SSL/TLS 加密所有通信
- 接收 HTTPS POST 请求，请求体包含 JSON-RPC 消息
- 处理请求后，返回 JSON-RPC 响应
- 使用 `SseServerRequestHandler` 处理请求

### 客户端模式

- 客户端使用 `HttpsTransport` 在客户端模式下运行
- 连接到指定的服务器地址和端口
- 使用 SSL/TLS 加密所有通信
- 通过 HTTPS POST 发送 JSON-RPC 请求
- 接收并解析 JSON-RPC 响应

## 配置参数

### 服务器配置

- `host`: 监听地址（默认: "localhost"）
- `port`: 监听端口（默认: 8443）
- `endpoint`: HTTP 端点路径（默认: "/"）
- `cert_file`: 服务器证书文件路径（可选）
- `key_file`: 服务器私钥文件路径（可选）
- `ca_file`: CA 证书文件路径（可选）
- `verify_peer`: 是否验证客户端证书（默认: true）
- `verify_hostname`: 是否验证主机名（默认: true）

### 客户端配置

- `host`: 服务器地址（默认: "localhost"）
- `port`: 服务器端口（默认: 8443）
- `endpoint`: HTTP 端点路径（默认: "/"）
- `ca_file`: CA 证书文件路径（可选，用于验证服务器证书）
- `verify_peer`: 是否验证服务器证书（默认: true）
- `verify_hostname`: 是否验证主机名（默认: true）
- `timeout_seconds`: 请求超时时间（默认: 30秒）
- `keep_alive`: 保持连接（默认: true）

## 日志

日志文件保存在 `logs/` 目录：
- `https_server_example.log` - 服务器日志
- `https_client_example.log` - 客户端日志

## 测试

客户端会自动执行以下测试：

1. Ping 请求测试
2. Echo 请求测试
3. Add 请求测试（42 + 58 = 100）
4. Get Time 请求测试
5. Log 通知测试

## 使用 curl 测试服务器

你也可以使用 curl 直接测试服务器（需要禁用证书验证或提供 CA 证书）：

```bash
# Ping 请求（禁用证书验证）
curl -k -X POST https://localhost:8443/ \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"1","method":"ping","params":{}}'

# Echo 请求
curl -k -X POST https://localhost:8443/ \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"2","method":"echo","params":"Hello World"}'

# Add 请求
curl -k -X POST https://localhost:8443/ \
  -H "Content-Type: application/json" \
  -d '{"jsonrpc":"2.0","id":"3","method":"add","params":{"a":10,"b":20}}'
```

注意：`-k` 选项用于禁用 SSL 证书验证（仅用于测试）。

## 注意事项

1. **证书验证**：
   - 生产环境应启用证书验证
   - 测试环境可以使用自签名证书并禁用验证
   - 客户端需要提供 CA 证书文件以验证服务器证书

2. **端口**：
   - 确保端口未被占用（默认 8443）
   - HTTPS 通常使用 443 端口（需要 root 权限）或 8443（非特权端口）

3. **安全**：
   - 所有通信都通过 SSL/TLS 加密
   - 私钥文件应妥善保管，不要泄露
   - 生产环境应使用 CA 签发的有效证书

4. **协议**：
   - 服务器和客户端使用 JSON-RPC 2.0 协议通信
   - 所有请求使用 HTTPS POST 方法
   - 请求和响应都使用 JSON 格式

5. **服务器运行**：
   - 服务器会持续运行直到收到 SIGINT 或 SIGTERM 信号

## 与 HttpTransport 的区别

- **HttpTransport**: 使用 HTTP 协议，通信不加密，适用于内网或开发环境
- **HttpsTransport**: 使用 HTTPS 协议，所有通信通过 SSL/TLS 加密，适用于生产环境或需要安全通信的场景

## 扩展

你可以：

1. 添加更多的方法到 `ExampleSseServerRequestHandler`
2. 修改客户端测试不同的请求
3. 使用 `TransportFactory` 创建传输实例
4. 自定义配置参数（超时时间、重试次数等）
5. 配置双向 TLS（mTLS）认证
6. 使用不同的证书格式（PEM、DER 等）

## 故障排除

### 连接失败

- 检查服务器是否正在运行
- 检查端口是否正确
- 检查防火墙设置
- 检查证书文件路径是否正确

### 证书验证错误

- 如果使用自签名证书，客户端需要禁用验证或提供 CA 证书
- 检查证书是否过期
- 检查证书的主机名是否匹配服务器地址

### SSL/TLS 错误

- 检查 OpenSSL 库是否正确安装
- 检查证书和私钥是否匹配
- 检查证书格式是否正确（应为 PEM 格式）

