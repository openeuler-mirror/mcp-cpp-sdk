# HTTP JSON请求客户端

这个项目演示了如何使用NetIO接口和httplib库构建JSON数据请求网站，包括MCP协议请求。

## 文件结构

```
http/
├── common/
│   ├── httplib.h          # HTTP库头文件
│   └── json.hpp           # JSON库头文件
├── netio.h                # NetIO接口定义
├── netio.cpp              # NetIO接口实现
├── simple_http_client.cpp # 简单HTTP客户端示例
├── mcp_curl_client.cpp    # MCP Curl客户端（模拟curl命令）
├── mcp_http_client.cpp    # 完整MCP HTTP客户端
├── json_request_client.cpp # 通用JSON请求客户端
├── netio_example.cpp      # NetIO使用示例
├── CMakeLists.txt         # 构建配置
└── README.md              # 说明文档
```

## 功能特性

### ✅ 已实现的功能

1. **简单HTTP客户端** (`simple_http_client.cpp`)
   - 使用httplib直接发送HTTP请求
   - 支持JSON POST和GET请求
   - 不依赖SSL，适合测试和学习

2. **MCP Curl客户端** (`mcp_curl_client.cpp`)
   - 完全模拟你提供的curl命令
   - 发送MCP工具列表请求
   - 详细的请求和响应信息显示

3. **完整MCP客户端** (`mcp_http_client.cpp`)
   - 集成MCP协议接口
   - 支持工具列表、工具调用、资源管理等
   - 完整的错误处理和响应解析

4. **通用JSON客户端** (`json_request_client.cpp`)
   - 支持任意JSON数据请求
   - 灵活的请求配置
   - 多种请求方法支持

5. **NetIO接口** (`netio.h`, `netio.cpp`)
   - 高级网络IO抽象
   - 支持HTTP/HTTPS/WebSocket
   - 异步和同步请求支持

## 使用示例

### 1. 简单HTTP客户端

```cpp
#include "common/httplib.h"
#include "common/json.hpp"

// 创建客户端
httplib::Client client("httpbin.org", 80);

// 创建JSON数据
nlohmann::json data = {
    {"jsonrpc", "2.0"},
    {"method", "tools/list"},
    {"id", 1}
};

// 发送POST请求
auto result = client.Post("/post", data.dump(), "application/json");
```

### 2. MCP Curl客户端

```cpp
// 完全模拟curl命令
curl -X "POST" "https://mcp.didichuxing.com/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ" \
     -H 'Content-Type: application/json; charset=utf-8' \
     -d $'{
  "jsonrpc": "2.0",
  "method": "tools/list",
  "id": 1,
  "params": {
    "_meta": {
      "progressToken": 1
    }
  }
}'
```

### 3. 通用JSON客户端

```cpp
JsonRequestClient client("https://api.example.com", "your-api-key");

if (client.connect()) {
    // 发送MCP请求
    auto response = client.send_tools_list_request();
    
    // 发送自定义请求
    nlohmann::json custom_data = {
        {"message", "Hello World"},
        {"data", {"key", "value"}}
    };
    client.send_custom_request("POST", "/api/endpoint", custom_data);
}
```

## 构建和运行

### 构建

```bash
cd /Users/apple/wangjiandong/go_mcp/cpp_mcp_sdk/http
mkdir build_simple && cd build_simple
cmake ..
make
```

### 运行示例

```bash
# 运行简单HTTP客户端
./simple_http_client

# 运行MCP Curl客户端
./mcp_curl_client

# 运行NetIO示例（需要SSL支持）
./netio_example
```

## 示例输出

### 简单HTTP客户端输出

```
🚀 简单HTTP客户端示例
====================
🔧 MCP工具列表请求演示
====================
📤 发送HTTP请求:
URL: http://httpbin.org:80/post
Method: POST
Headers:
  Content-Type: application/json; charset=utf-8
  User-Agent: SimpleHttpClient/1.0
  Accept: application/json
Body:
{"id":1,"jsonrpc":"2.0","method":"tools/list","params":{"_meta":{"progressToken":1}}}

⏳ 发送请求中...

📥 收到响应:
状态码: 200
响应体:
{
  "args": {},
  "data": "{\"id\":1,\"jsonrpc\":\"2.0\",\"method\":\"tools/list\",\"params\":{\"_meta\":{\"progressToken\":1}}}",
  "files": {},
  "form": {},
  "headers": {
    "Accept": "application/json",
    "Content-Type": "application/json",
    "Host": "httpbin.org",
    "User-Agent": "SimpleHttpClient/1.0"
  },
  "json": {
    "id": 1,
    "jsonrpc": "2.0",
    "method": "tools/list",
    "params": {
      "_meta": {
        "progressToken": 1
      }
    }
  },
  "origin": "221.238.56.53",
  "url": "http://httpbin.org/post"
}

✅ MCP请求演示完成
```

### MCP Curl客户端输出

```
🚀 MCP Curl客户端 - 模拟curl命令
=================================
📤 发送请求 (模拟curl):
curl -X "POST" "https://mcp.didichuxing.com/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ" \
     -H 'Content-Type: application/json; charset=utf-8' \
     -d $'{"id":1,"jsonrpc":"2.0","method":"tools/list","params":{"_meta":{"progressToken":1}}}'

🔗 实际请求信息:
URL: https://mcp.didichuxing.com/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ
Method: POST
Headers:
  Content-Type: application/json; charset=utf-8
Body:
{"id":1,"jsonrpc":"2.0","method":"tools/list","params":{"_meta":{"progressToken":1}}}

⏳ 发送请求中...

📥 收到响应:
状态码: 400
响应头:
  Connection: close
  Content-Length: 272
  Content-Type: text/html
  Date: Wed, 24 Sep 2025 09:14:53 GMT
  Server: router/2.42.0
响应体:
<html>
<head><title>400 The plain HTTP request was sent to HTTPS port</title></head>
<body bgcolor="white">
<center><h1>400 Bad Request</h1></center>
<center>The plain HTTP request was sent to HTTPS port</center>
<hr><center>router/2.42.0</center>
</body>
</html>

✅ 请求完成
```

## 技术说明

### HTTP vs HTTPS

- **简单HTTP客户端**: 使用httplib的HTTP客户端，适合测试和学习
- **HTTPS支持**: 需要OpenSSL库和SSL客户端，在系统没有SSL支持时会失败
- **实际部署**: 生产环境中的MCP服务器通常需要HTTPS连接

### JSON处理

- 使用nlohmann/json库进行JSON解析和生成
- 支持复杂的嵌套JSON结构
- 自动格式化输出，便于调试

### 错误处理

- 完整的HTTP状态码处理
- JSON解析错误处理
- 网络连接错误处理
- 详细的错误信息显示

## 扩展功能

### 1. 添加SSL支持

要支持HTTPS，需要：

1. 安装OpenSSL开发库
2. 在CMakeLists.txt中定义`CPPHTTPLIB_OPENSSL_SUPPORT`
3. 链接OpenSSL库

### 2. 添加认证支持

```cpp
// 添加API密钥
Headers headers = {
    {"Authorization", "Bearer " + api_key},
    {"X-API-Key", api_key}
};
```

### 3. 添加重试机制

```cpp
// 设置重试次数
client.set_connection_timeout(30);
client.set_read_timeout(30);
client.set_write_timeout(30);
```

## 总结

这个项目提供了多种方式来发送JSON数据请求：

1. **简单直接**: 使用httplib直接发送HTTP请求
2. **功能完整**: 集成MCP协议接口的高级客户端
3. **灵活通用**: 支持任意JSON数据的通用客户端
4. **易于扩展**: 模块化设计，易于添加新功能

所有示例都包含详细的输出信息，便于理解和调试。在实际使用中，建议根据具体需求选择合适的客户端实现。