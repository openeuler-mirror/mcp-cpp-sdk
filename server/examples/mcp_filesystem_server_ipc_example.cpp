/**
 * @file mcp_filesystem_server_ipc_example.cpp
 * @brief MCP FileSystem Server IPC 传输示例
 * 
 * 这个示例展示了如何使用MCPFileSystemServer类通过IPC传输方式创建文件系统服务器。
 * 演示了基于IPC的文件系统操作和资源管理功能。
 */

#include "mcp_filesystem_server.h"
#include "../../transport/include/transport_base.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <filesystem>
#include <random>
#include <sstream>
#include <atomic>

using namespace mcp;
using namespace mcp_transport;
using namespace mcp_logger;

// 全局变量用于优雅关闭
static std::atomic<bool> g_running{true};
// 全局 logger 指针，用于信号处理函数
static std::shared_ptr<Logger::ModuleLogger> g_logger = nullptr;

// 信号处理函数
void signalHandler(int signal) {
    static std::atomic<bool> signal_received{false};
    
    if (signal == SIGINT || signal == SIGTERM) {
        // 使用 exchange 确保只处理一次信号
        if (!signal_received.exchange(true)) {
            if (g_logger) {
                g_logger->warning("\n🛑 收到停止信号，正在关闭服务器...");
            }
            g_running = false;
        }
    }
}

// 打印服务器信息
void printServerInfo(std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("🚀 MCP FileSystem Server IPC 示例");
    logger->info("=================================");
    logger->info("📋 功能特性:");
    logger->info("  ✅ 基于IPC传输的文件系统服务器");
    logger->info("  ✅ 安全的路径验证和解析");
    logger->info("  ✅ 可配置的读写权限控制");
    logger->info("  ✅ 完整的文件系统操作工具");
    logger->info("  ✅ 自动资源发现和管理");
    logger->info("  ✅ 进程间通信支持");
    logger->info("=================================");
}

// 打印使用说明
void printUsage(std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📖 使用说明:");
    logger->info("  1. 服务器启动后，通过IPC与客户端通信");
    logger->info("  2. 支持的文件系统工具:");
    logger->info("     - read_file: 读取文件内容");
    logger->info("     - write_file: 写入文件内容");
    logger->info("     - list_directory: 列出目录内容");
    logger->info("     - create_directory: 创建目录");
    logger->info("     - delete_file: 删除文件或目录");
    logger->info("     - file_info: 获取文件/目录信息");
    logger->info("  3. 支持的文件系统资源:");
    logger->info("     - file://. : 根目录");
    logger->info("     - file://path : 指定路径的文件或目录");
    logger->info("  4. 按 Ctrl+C 停止服务器");
}

// 初始化日志系统
void initializeLogger() {
    mcp_logger::LogConfig config;
    config.level = mcp_logger::LogLevel::INFO;
    // 同时输出到文件和控制台（stderr），不会干扰stdout的JSON-RPC消息
    config.target = mcp_logger::LogTarget::BOTH;
    config.file_path = "logs/mcp_filesystem_server_ipc.log";
    config.enable_timestamp = true;
    config.format = "[{timestamp}] [{level}] {message}";
    config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(config);
}

// 创建测试环境
void createTestEnvironment(const std::string& root_path, std::shared_ptr<Logger::ModuleLogger> logger) {
    try {
        // 创建测试目录结构
        std::filesystem::create_directories(root_path + "/test_files");
        std::filesystem::create_directories(root_path + "/test_files/subdir");
        std::filesystem::create_directories(root_path + "/docs");
        std::filesystem::create_directories(root_path + "/logs");
        
        // 创建测试文件
        std::ofstream sample_file(root_path + "/test_files/sample.txt");
        if (sample_file.is_open()) {
            sample_file << "这是一个IPC测试文件\n";
            sample_file << "用于演示MCP文件系统服务器的IPC功能\n";
            sample_file << "创建时间: " << std::time(nullptr) << "\n";
            sample_file << "传输方式: IPC (进程间通信)\n";
            sample_file.close();
        }
        
        std::ofstream readme_file(root_path + "/README.md");
        if (readme_file.is_open()) {
            readme_file << "# MCP FileSystem Server IPC Demo\n\n";
            readme_file << "这个目录包含用于测试MCP文件系统服务器IPC功能的示例文件和目录。\n\n";
            readme_file << "## 传输方式\n\n";
            readme_file << "- **IPC (进程间通信)**: 通过管道或共享内存进行通信\n";
            readme_file << "- **优势**: 低延迟、高安全性、适合本地通信\n";
            readme_file << "- **适用场景**: 本地工具、开发环境、单机部署\n\n";
            readme_file << "## 目录结构\n\n";
            readme_file << "- `test_files/`: 测试文件目录\n";
            readme_file << "  - `sample.txt`: 示例文本文件\n";
            readme_file << "  - `subdir/`: 子目录\n";
            readme_file << "- `docs/`: 文档目录\n";
            readme_file << "- `logs/`: 日志目录\n";
            readme_file << "- `README.md`: 本说明文件\n\n";
            readme_file << "## 测试方法\n\n";
            readme_file << "1. 使用 `list_directory` 工具列出目录内容\n";
            readme_file << "2. 使用 `read_file` 工具读取文件内容\n";
            readme_file << "3. 使用 `write_file` 工具创建新文件\n";
            readme_file << "4. 使用 `create_directory` 工具创建新目录\n";
            readme_file << "5. 使用 `file_info` 工具获取文件信息\n\n";
            readme_file << "## IPC 通信特点\n\n";
            readme_file << "- 进程间直接通信，无需网络\n";
            readme_file << "- 数据安全性高，不会泄露到网络\n";
            readme_file << "- 响应速度快，延迟低\n";
            readme_file << "- 适合本地开发和测试环境\n";
            readme_file.close();
        }
        
        std::ofstream config_file(root_path + "/test_files/ipc_config.json");
        if (config_file.is_open()) {
            config_file << "{\n";
            config_file << "  \"server_name\": \"MCP FileSystem Server IPC\",\n";
            config_file << "  \"version\": \"1.0.0\",\n";
            config_file << "  \"transport_type\": \"IPC\",\n";
            config_file << "  \"features\": [\n";
            config_file << "    \"file_operations\",\n";
            config_file << "    \"directory_management\",\n";
            config_file << "    \"path_validation\",\n";
            config_file << "    \"resource_discovery\",\n";
            config_file << "    \"ipc_communication\"\n";
            config_file << "  ],\n";
            config_file << "  \"ipc_settings\": {\n";
            config_file << "    \"buffer_size\": 4096,\n";
            config_file << "    \"read_timeout_ms\": 100,\n";
            config_file << "    \"max_retry_count\": 10\n";
            config_file << "  },\n";
            config_file << "  \"created_at\": " << std::time(nullptr) << "\n";
            config_file << "}\n";
            config_file.close();
        }
        
        logger->info("✅ IPC测试环境创建完成");
    } catch (const std::exception& e) {
        logger->warning("⚠️  创建测试环境失败: " + std::string(e.what()));
    }
}

// 演示服务器功能
void demonstrateServerFeatures(MCPFileSystemServer& server, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("开始演示IPC服务器功能");
    
    logger->info("\n🔧 IPC服务器功能演示:");
    
    // 显示根路径
    logger->info("  📁 根路径: " + server.getRootPath());
    
    // 显示写权限状态
    logger->info("  ✏️  写操作: " + std::string(server.isWriteEnabled() ? "启用" : "禁用"));
    
    // 显示文件系统资源
    auto resources = server.listFileSystemResources();
    logger->info("  📋 文件系统资源 (" + std::to_string(resources.size()) + "个):");
    for (const auto& resource : resources) {
        logger->info("    - " + resource);
    }
    
    // 添加IPC特定的资源
    server.addFileSystemResource("test_files/ipc_config.json", "IPC配置", "IPC传输配置文件");
    server.addFileSystemResource("logs", "日志目录", "服务器日志文件目录");
    logger->info("  ➕ 添加了IPC特定资源");
    
    logger->info("IPC服务器功能演示完成");
}

// 运行服务器统计信息
void runServerStatistics(MCPFileSystemServer& server) {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_ipc");
    
    int heartbeat_counter = 0;
    int stats_counter = 0;
    
    while (g_running && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        heartbeat_counter++;
        stats_counter++;
        
        // 每30秒发送一次心跳
        if (heartbeat_counter >= 300) { // 100ms * 300 = 30s
            heartbeat_counter = 0;
            logger->info("💓 IPC服务器心跳正常");
        }
        
        // 每5分钟显示一次统计信息
        if (stats_counter >= 3000) { // 100ms * 3000 = 5分钟
            stats_counter = 0;
            logger->info("📊 IPC服务器运行统计:");
            logger->info("  - 运行时间: " + std::to_string(std::time(nullptr)) + " 秒");
            logger->info("  - 根路径: " + server.getRootPath());
            //logger->info("  - 写权限: " + std::string(server.isWriteEnabled() ? "启用" : "禁用"))；
            logger->info("  - 资源数量: " + std::to_string(server.listFileSystemResources().size()));
            logger->info("  - 传输方式: IPC (进程间通信)");
        }
    }
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 解析命令行参数
    std::string root_path = ".";
    bool enable_write = true;
    std::string ipc_name = "mcp_filesystem_server_ipc";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            root_path = argv[++i];
        } else if (arg == "--read-only") {
            enable_write = false;
        } else if (arg == "--ipc-name" && i + 1 < argc) {
            ipc_name = argv[++i];
        } else if (arg == "--help") {
            // 帮助信息需要先初始化日志系统
            initializeLogger();
            auto help_logger = getModuleLogger("mcp_filesystem_server_ipc");
            help_logger->info("\n用法: " + std::string(argv[0]) + " [选项]");
            help_logger->info("选项:");
            help_logger->info("  --root <path>       设置根路径 (默认: 当前目录)");
            help_logger->info("  --read-only         禁用写操作");
            help_logger->info("  --ipc-name <name>   设置IPC名称 (默认: mcp_filesystem_server_ipc)");
            help_logger->info("  --help             显示此帮助信息");
            return 0;
        }
    }
    
    // 初始化日志系统
    initializeLogger();
    auto logger = getModuleLogger("mcp_filesystem_server_ipc");
    g_logger = logger;  // 设置全局 logger 供信号处理函数使用
    
    logger->info("🚀 启动 MCP FileSystem Server IPC 示例");
    logger->info("📁 根路径: " + root_path);
    logger->info("✏️  写操作: " + std::string(enable_write ? "启用" : "禁用"));
    logger->info("🔗 IPC名称: " + ipc_name);
    
    // 打印服务器信息
    printServerInfo(logger);
    
    try {
        // 创建测试环境
        createTestEnvironment(root_path, logger);
        
        // 创建文件系统服务器实例
        MCPFileSystemServer server(root_path, enable_write);
        
        // 演示服务器功能
        demonstrateServerFeatures(server, logger);
        
        // 启动服务器 (使用IPC配置)
        logger->info("🚀 启动文件系统服务器 (IPC模式)...");
        logger->info("📋 IPC配置信息:");
        logger->info("  - IPC名称: " + ipc_name);
        logger->info("  - 缓冲区大小: 8192");
        logger->info("  - 读取超时: 200ms");
        logger->info("  - 最大重试次数: 5");
        logger->info("  - 日志级别: INFO");
        
        // 创建IPC传输配置
        mcp_transport::TransportConfig ipc_config;
        ipc_config.type = mcp_transport::TransportType::IPC;
        ipc_config.mode = mcp_transport::TransportMode::SERVER;
        ipc_config.name = ipc_name;
        ipc_config.enable_logging = true;
        ipc_config.log_level = 1;
        ipc_config.log_file_path = "logs/mcp_filesystem_server_ipc_transport.log";
        
        // IPC特定配置
        // 服务器模式下 command 必须为空字符串，表示当前进程就是服务器
        ipc_config.ipc.command = "";  // 空命令表示当前进程就是服务器
        ipc_config.ipc.buffer_size = 8192;
        ipc_config.ipc.read_timeout_ms = 200;
        ipc_config.ipc.max_retry_count = 5;
        ipc_config.ipc.enable_logging = true;
        ipc_config.ipc.log_file_path = "logs/mcp_filesystem_server_ipc_pipe.log";
        ipc_config.ipc.log_level = mcp_logger::LogLevel::INFO;
        
        // 服务端配置
        ipc_config.server.server_name = "MCP FileSystem Server IPC";
        ipc_config.server.server_version = "1.0.0";
        ipc_config.server.description = "MCP FileSystem Server with IPC transport";
        
        logger->info("🔧 应用IPC传输配置:");
        logger->info("  - 传输类型: IPC");
        logger->info("  - 服务端模式: 启用");
        logger->info("  - 缓冲区大小: " + std::to_string(ipc_config.ipc.buffer_size));
        logger->info("  - 读取超时: " + std::to_string(ipc_config.ipc.read_timeout_ms) + "ms");
        logger->info("  - 最大重试次数: " + std::to_string(ipc_config.ipc.max_retry_count));
        
        if (server.startWithConfig(ipc_config)) {
            logger->info("✅ 文件系统服务器启动成功 (IPC模式)");
            logger->info("✅ 文件系统服务器启动成功，正在运行...");
            logger->info("🔗 IPC通信通道: " + ipc_name);
            printUsage(logger);
            
            // 运行服务器统计信息
            runServerStatistics(server);
            
            // 停止服务器
            logger->info("🛑 正在停止文件系统服务器...");
            server.stop();
            logger->info("✅ 文件系统服务器已停止");
            
        } else {
            logger->error("❌ 文件系统服务器启动失败 (IPC模式)");
            return 1;
        }
        
    } catch (const std::exception& e) {
        logger->error("❌ 服务器错误: " + std::string(e.what()));
        return 1;
    }
    
    // 关闭日志系统
    Logger::getInstance().shutdown();
    return 0;
}
