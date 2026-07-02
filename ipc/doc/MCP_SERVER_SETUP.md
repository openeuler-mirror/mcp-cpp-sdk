# MCP服务器设置说明

## 概述

本项目的示例程序现在配置为使用真实的MCP服务器 `@modelcontextprotocol/server-memory`，而不是简单的回显脚本。

## 前置要求

### 1. 安装Node.js和npm

#### macOS (使用Homebrew)
```bash
# 安装Homebrew（如果还没有安装）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 安装Node.js
brew install node
```

#### macOS (使用官方安装包)
1. 访问 [Node.js官网](https://nodejs.org/)
2. 下载LTS版本的macOS安装包
3. 运行安装包并按照提示安装

#### 验证安装
```bash
node --version
npm --version
npx --version
```

### 2. 安装MCP服务器

```bash
# 全局安装MCP服务器（可选）
npm install -g @modelcontextprotocol/server-memory

# 或者直接使用npx运行（推荐）
npx -y @modelcontextprotocol/server-memory
```

## 配置说明

### 当前配置

示例程序现在使用以下配置：

```cpp
PipeConfig config;
config.command = "npx";
config.args = {"-y", "@modelcontextprotocol/server-memory"};
config.env_vars["NODE_ENV"] = "development";
```

### 参数说明

- `npx`: Node.js包执行器
- `-y`: 自动确认安装（如果包不存在）
- `@modelcontextprotocol/server-memory`: MCP内存服务器包
- `NODE_ENV=development`: 设置Node.js环境为开发模式

## 运行示例

### 1. 编译项目
```bash
cd /Users/apple/wangjiandong/go_mcp/cpp-mcp/build_pipe
make
```

### 2. 运行示例程序
```bash
# 基本示例
./pipe_communication_example

# MCP协议示例
./mcp_protocol_example

# Boy回显示例
./boy_echo_example
```

## 预期行为

使用真实的MCP服务器后，您将看到：

1. **真实的MCP协议响应** - 而不是简单的"我是一个boy"回显
2. **JSON-RPC消息处理** - 服务器会正确解析和响应MCP消息
3. **工具和资源列表** - 服务器会返回可用的工具和资源
4. **错误处理** - 如果消息格式不正确，服务器会返回错误

## 故障排除

### 问题1: npx命令未找到
```bash
# 确保Node.js已正确安装
which node
which npm
which npx

# 如果npx不存在，重新安装Node.js
```

### 问题2: MCP服务器无法启动
```bash
# 检查网络连接
ping npmjs.com

# 手动测试MCP服务器
npx -y @modelcontextprotocol/server-memory --help
```

### 问题3: 权限问题
```bash
# 确保有执行权限
chmod +x pipe_communication_example
chmod +x mcp_protocol_example
chmod +x boy_echo_example
```

## 其他MCP服务器

除了`@modelcontextprotocol/server-memory`，您还可以尝试其他MCP服务器：

```bash
# 文件系统服务器
npx -y @modelcontextprotocol/server-filesystem

# 数据库服务器
npx -y @modelcontextprotocol/server-sqlite

# 网络服务器
npx -y @modelcontextprotocol/server-fetch
```

## 开发模式

如果您想开发自己的MCP服务器，可以：

1. 创建一个新的Node.js项目
2. 安装MCP SDK
3. 实现MCP协议接口
4. 更新示例程序中的命令配置

```bash
# 创建新的MCP服务器项目
mkdir my-mcp-server
cd my-mcp-server
npm init -y
npm install @modelcontextprotocol/sdk
```
