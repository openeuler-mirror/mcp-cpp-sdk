# MCP Agent IPC Demo 使用说明

## 概述

`agent_ipc_demo.cpp` 是基于 `agent_demo.cpp` 的 IPC 版本演示程序。与原始版本的主要区别是使用 IPC (Inter-Process Communication) 传输而不是 SSE (Server-Sent Events) 传输。

## 主要特性

- **IPC 传输**: 使用本地进程间通信，启动文件系统服务器
- **智能对话**: 支持与 LLM 的自然语言交互
- **工具调用**: 自动调用 MCP 工具完成任务
- **资源管理**: 读取和管理 MCP 资源
- **交互式界面**: 支持实时对话和状态查询

## 系统要求

### 必需依赖
1. **Node.js** (版本 16 或更高)
2. **npm** (Node.js 包管理器)
3. **OpenAI API 密钥** (用于 LLM 交互)

### MCP 服务器依赖
```bash
npm install -g @modelcontextprotocol/server-filesystem
```

## 构建和运行

### 1. 构建项目
```bash
# 在 agent 目录下运行
./build_ipc_demo.sh
```

### 2. 配置 API 密钥
编辑 `agent_ipc_demo.cpp` 文件，将以下行中的 API 密钥替换为你的实际密钥：
```cpp
agent_config.llm_config.api_key = "your-api-key-here";
```

### 3. 运行程序
```bash
cd build/bin
./agent_ipc_demo
```

## 使用说明

### 交互式命令

程序启动后，你可以使用以下命令：

- **普通对话**: 直接输入你的问题或请求
- **`quit` 或 `exit`**: 退出程序
- **`clear`**: 清空对话历史
- **`tools`**: 显示可用工具列表
- **`status`**: 显示 Agent 状态信息
- **`resources`**: 显示可用资源列表
- **`prompts`**: 显示可用提示列表

### 示例对话

```
用户: 帮我查看当前目录下的文件
助手: [Agent 会调用文件系统工具来列出文件]

用户: 读取 README.md 文件的内容
助手: [Agent 会调用 read_file 工具读取文件内容]

用户: 状态
助手: 显示当前连接状态、初始化状态和对话轮数
```

## 配置说明

### IPC 配置参数

```cpp
// IPC 传输配置
agent_config.transport_config.type = mcp_transport::TransportType::IPC;
agent_config.transport_config.ipc.command = "npx";
agent_config.transport_config.ipc.args = {"-y", "@modelcontextprotocol/server-filesystem", "."};
agent_config.transport_config.ipc.buffer_size = 2048;
agent_config.transport_config.ipc.read_timeout_ms = 100;
agent_config.transport_config.ipc.max_retry_count = 10;
```

### LLM 配置参数

```cpp
// LLM 配置
agent_config.llm_config.api_endpoint = "https://api.openai.com/v1/chat/completions";
agent_config.llm_config.api_key = "your-api-key-here";
agent_config.llm_config.model_name = "gpt-3.5-turbo";
agent_config.llm_config.max_tokens = 2048;
agent_config.llm_config.temperature = 0.7;
```

## 故障排除

### 常见问题

1. **Agent 初始化失败**
   - 检查 Node.js 是否已安装
   - 检查 @modelcontextprotocol/server-filesystem 包是否已安装
   - 检查网络连接是否正常

2. **连接失败**
   - 确保当前目录有文件系统访问权限
   - 检查是否有其他进程占用相关端口

3. **API 调用失败**
   - 验证 OpenAI API 密钥是否正确
   - 检查网络连接和 API 配额

### 调试信息

程序会生成详细的日志文件：
- `agent_ipc_demo.log`: 主程序日志
- `agent_ipc_transport.log`: IPC 传输层日志

## 与 SSE 版本的对比

| 特性 | IPC 版本 | SSE 版本 |
|------|----------|----------|
| 传输方式 | 本地进程间通信 | HTTP/SSE |
| 服务器要求 | 本地文件系统服务器 | 远程 HTTP 服务器 |
| 网络依赖 | 不需要网络连接 | 需要网络连接 |
| 性能 | 更快，延迟更低 | 相对较慢 |
| 适用场景 | 本地开发、测试 | 远程服务、生产环境 |

## 扩展功能

### 自定义工具
你可以通过修改 MCP 服务器配置来添加自定义工具。

### 自定义资源
可以通过配置 MCP 服务器来提供自定义资源访问。

### 自定义提示
可以配置自定义提示模板来改善 Agent 的响应质量。

## 技术支持

如果遇到问题，请检查：
1. 日志文件中的错误信息
2. 系统依赖是否完整安装
3. 网络连接和 API 配置是否正确

更多信息请参考 MCP SDK 文档。
