# MCP Logger 日志系统

这是一个功能完整的C++日志系统，支持多线程、文件轮转、模块化日志等功能。

## 功能特性

- **多日志级别**: DEBUG, INFO, WARNING, ERROR, FATAL
- **多种输出目标**: 控制台、文件、或同时输出
- **线程安全**: 支持多线程并发写入
- **文件轮转**: 自动管理日志文件大小和数量
- **模块化日志**: 支持不同模块的独立日志记录器
- **自定义格式**: 可配置日志输出格式
- **时间戳支持**: 精确到毫秒的时间戳
- **线程ID支持**: 显示日志来源线程
- **文件位置支持**: 显示日志来源文件和行号

## 文件结构

```
log/
├── logger.h              # 日志系统头文件
├── logger.cpp            # 日志系统实现
├── logger_example.cpp    # 使用示例
├── CMakeLists.txt        # CMake构建文件
├── build_and_run.sh      # 快速编译运行脚本
└── README.md            # 说明文档
```

## 快速开始

### 方法1: 使用编译脚本（推荐）

```bash
cd /Users/apple/wangjiandong/go_mcp/cpp_mcp_sdk/log
./build_and_run.sh
```

### 方法2: 手动编译

```bash
cd /Users/apple/wangjiandong/go_mcp/cpp_mcp_sdk/log
g++ -std=c++17 -I. -o logger_example logger.cpp logger_example.cpp -pthread
./logger_example
```

### 方法3: 使用CMake

```bash
cd /Users/apple/wangjiandong/go_mcp/cpp_mcp_sdk/log
mkdir build && cd build
cmake ..
make
./logger_example
```

## 基本使用

### 1. 初始化日志系统

```cpp
#include "logger.h"

// 获取日志实例
Logger& logger = Logger::getInstance();

// 配置日志系统
LogConfig config;
config.level = LogLevel::DEBUG_LEVEL;
config.target = LogTarget::BOTH;  // 同时输出到控制台和文件
config.file_path = "app.log";
config.enable_timestamp = true;
config.enable_thread_id = true;
config.enable_file_location = true;

logger.initialize(config);
```

### 2. 使用便捷宏

```cpp
// 基本日志宏
MCP_LOG_DEBUG("调试信息");
MCP_LOG_INFO("信息日志");
MCP_LOG_WARNING("警告信息");
MCP_LOG_ERROR("错误信息");
MCP_LOG_FATAL("致命错误");

// 模块化日志宏
MCP_MODULE_LOG_INFO("Network", "网络连接已建立");
MCP_MODULE_LOG_ERROR("Database", "数据库连接失败");
```

### 3. 使用模块化日志记录器

```cpp
// 获取模块日志记录器
auto networkLogger = logger.getModuleLogger("Network");
auto databaseLogger = logger.getModuleLogger("Database");

// 使用模块日志记录器
networkLogger->info("网络连接已建立");
databaseLogger->error("数据库查询失败");
```

### 4. 动态配置

```cpp
// 修改日志级别
logger.setLevel(LogLevel::WARNING);

// 修改输出目标
logger.setTarget(LogTarget::FILE);

// 修改日志文件
logger.setLogFile("new_log.txt");

// 修改格式
logger.setFormat("[{timestamp}] [{level}] {message}");
```

## 配置选项

### LogConfig 结构

```cpp
struct LogConfig {
    LogLevel level = LogLevel::INFO;           // 日志级别
    LogTarget target = LogTarget::CONSOLE;     // 输出目标
    std::string file_path = "";                // 日志文件路径
    bool enable_timestamp = true;              // 启用时间戳
    bool enable_thread_id = false;             // 启用线程ID
    bool enable_file_location = false;         // 启用文件位置
    bool auto_flush = true;                    // 自动刷新
    size_t max_file_size = 10 * 1024 * 1024;  // 最大文件大小(10MB)
    int max_files = 5;                         // 最大文件数量
    std::string format = "[{timestamp}] [{level}] {message}";  // 日志格式
};
```

### 日志级别

- `DEBUG_LEVEL`: 调试信息
- `INFO`: 一般信息
- `WARNING`: 警告信息
- `ERROR`: 错误信息
- `FATAL`: 致命错误

### 输出目标

- `CONSOLE`: 仅控制台输出
- `FILE`: 仅文件输出
- `BOTH`: 同时输出到控制台和文件

### 格式占位符

- `{timestamp}`: 时间戳
- `{level}`: 日志级别
- `{message}`: 日志消息
- `{thread_id}`: 线程ID
- `{file_location}`: 文件位置（文件名:行号:函数名）

## 示例输出

```
[2025-09-26 16:45:59.485] [DEBUG] [0x7ff847027fc0] logger_example.cpp:28:testBasicLogging - 这是一条调试信息
[2025-09-26 16:45:59.486] [INFO] [0x7ff847027fc0] logger_example.cpp:29:testBasicLogging - 这是一条信息日志
[2025-09-26 16:45:59.486] [WARNING] [0x7ff847027fc0] logger_example.cpp:30:testBasicLogging - 这是一条警告信息
[2025-09-26 16:45:59.486] [ERROR] [0x7ff847027fc0] logger_example.cpp:31:testBasicLogging - 这是一条错误信息
[2025-09-26 16:45:59.486] [FATAL] [0x7ff847027fc0] logger_example.cpp:32:testBasicLogging - 这是一条致命错误信息
```

## 线程安全

日志系统是线程安全的，可以在多线程环境中安全使用。所有日志操作都使用互斥锁保护。

## 文件轮转

当日志文件达到指定大小时，系统会自动进行文件轮转：
- 现有文件重命名为 `.1`
- 旧文件依次重命名为 `.2`, `.3`, 等
- 创建新的日志文件
- 保留指定数量的历史文件

## 注意事项

1. 确保有足够的文件系统权限来创建和写入日志文件
2. 在多线程环境中，建议启用线程ID显示以便调试
3. 生产环境中建议使用文件输出而不是控制台输出
4. 定期清理旧的日志文件以节省磁盘空间

## 依赖

- C++17 或更高版本
- pthread 库（Unix/Linux/macOS）
- 标准C++库

## 许可证

请参考项目根目录的LICENSE文件。
