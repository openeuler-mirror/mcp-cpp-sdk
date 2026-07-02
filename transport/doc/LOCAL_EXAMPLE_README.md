# 本地HTTP通信示例 (local_example.cpp)

这个示例演示如何使用 MCP Transport 模块进行本地HTTP通信测试。

## 功能特性

- 🏠 连接到本地服务器 (localhost:8080)
- 📡 支持多种HTTP方法 (GET, POST, PUT, DELETE)
- 🔍 连接状态检查和健康检查
- 📊 详细的统计信息
- 🛠️ 错误处理和调试信息

## 使用方法

### 1. 启动本地服务器

在运行示例之前，需要先启动一个本地HTTP服务器：

```bash
# 使用Python启动简单HTTP服务器
python -m http.server 8080

# 或者使用Node.js
npx http-server -p 8080

# 或者使用其他HTTP服务器
```

### 2. 编译示例

```bash
cd cpp_mcp_sdk/transport
mkdir -p build
cd build
cmake ..
make local_example
```

### 3. 运行示例

```bash
# 直接运行
./local_example

# 或者使用CMake目标
make run_local_example
```

## 测试内容

示例会执行以下测试：

1. **连接测试** - 验证到本地服务器的连接
2. **API测试** - 测试各种API端点：
   - `/api/ping` - Ping测试
   - `/api/status` - 状态检查
   - `/api/info` - 信息获取
   - `/api/health` - 健康检查
3. **HTTP方法测试** - 测试不同的HTTP方法：
   - GET请求到根路径 `/`
   - PUT请求到 `/api/test`
   - DELETE请求到 `/api/test`
4. **统计信息** - 显示请求和响应统计

## 配置说明

示例使用以下配置：

- **主机**: localhost
- **端口**: 8080
- **协议**: HTTP (非HTTPS)
- **超时**: 10秒
- **重试次数**: 3次
- **用户代理**: MCP-Transport-Local/1.0

## 错误处理

示例包含详细的错误处理：

- 连接失败时提供启动本地服务器的建议
- 根据HTTP状态码提供具体的错误建议
- 显示详细的错误信息和响应内容

## 日志

示例会生成日志文件 `local_test.log`，包含详细的调试信息。

## 注意事项

- 确保本地服务器在端口8080上运行
- 如果端口被占用，可以修改代码中的端口号
- 示例假设本地服务器支持基本的HTTP方法
- 某些API端点可能不存在，这是正常的测试行为
