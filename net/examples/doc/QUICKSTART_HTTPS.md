# HTTPS 快速开始指南

本指南帮助您快速启动HTTPS客户端和服务器。

## 步骤1: 生成证书

```bash
cd net/examples
./generate_certs.sh
```

这会在 `certs/` 目录下生成所有必需的证书文件。

## 步骤2: 启动服务器

```bash
# 在 net/examples 目录下
./netio_https_server_demo \
    certs/server.crt \
    certs/server.key \
    0.0.0.0 \
    10443 \
    /mcp
```

服务器将在 `https://localhost:10443/mcp` 上监听。

## 步骤3: 运行客户端

在另一个终端中：

```bash
# 在 net/examples 目录下
./netio_https_client_demo \
    localhost \
    10443 \
    /mcp \
    certs/ca.crt
```

## 完整示例

```bash
# 终端1: 生成证书
cd net/examples
./generate_certs.sh

# 终端1: 启动服务器
./netio_https_server_demo certs/server.crt certs/server.key 0.0.0.0 10443 /mcp

# 终端2: 运行客户端
cd net/examples
./netio_https_client_demo localhost 10443 /mcp certs/ca.crt
```

## 不使用CA证书（仅测试）

如果您想跳过证书验证（仅用于测试自签名证书）：

```bash
./netio_https_client_demo localhost 10443 /mcp
```

⚠️ **注意**: 这会禁用证书验证，仅用于测试环境。

## 故障排除

### 问题: 证书验证失败

**解决方案**:
1. 确保使用了正确的CA证书文件: `certs/ca.crt`
2. 检查服务器证书是否由CA签名: `openssl verify -CAfile certs/ca.crt certs/server.crt`
3. 确保服务器证书中的CN或SAN包含 `localhost`

### 问题: 连接被拒绝

**解决方案**:
1. 确保服务器正在运行
2. 检查端口是否被占用: `lsof -i :10443`
3. 检查防火墙设置

### 问题: OpenSSL错误

**解决方案**:
1. 确保系统已安装OpenSSL
2. 检查编译时是否启用了OpenSSL支持
3. 查看服务器日志获取详细错误信息

## 更多信息

- 详细证书生成说明: [README_CERTIFICATES.md](README_CERTIFICATES.md)
- 服务器示例代码: `netio_https_server_demo.cpp`
- 客户端示例代码: `netio_https_client_demo.cpp`

