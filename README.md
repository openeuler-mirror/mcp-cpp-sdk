# MCP FileSystem Server HTTP Demo

这个目录包含用于测试MCP文件系统服务器HTTP功能的示例文件和目录。

## 传输方式

- **HTTP (超文本传输协议)**: 通过HTTP协议进行通信
- **优势**: 标准化、跨平台、易于调试、适合网络部署
- **适用场景**: Web应用、远程访问、分布式系统

## 目录结构

- `test_files/`: 测试文件目录
  - `sample.txt`: 示例文本文件
  - `subdir/`: 子目录
- `docs/`: 文档目录
- `logs/`: 日志目录
- `README.md`: 本说明文件

## 测试方法

1. 使用 `list_directory` 工具列出目录内容
2. 使用 `read_file` 工具读取文件内容
3. 使用 `write_file` 工具创建新文件
4. 使用 `create_directory` 工具创建新目录
5. 使用 `file_info` 工具获取文件信息

## HTTP 通信特点

- 标准化协议，易于集成
- 支持跨网络访问
- 可以使用标准HTTP工具测试（curl, Postman等）
- 适合Web应用和远程客户端
