/**
 * @file mcp_client_ipc_parent_child.cpp
 * @brief MCP Client IPC 父子进程通信示例
 * 
 * 这个示例展示了如何使用 MCP Client 通过 IPC 传输方式：
 * 1. 使用 command 配置启动 server 作为子进程
 * 2. 实现父进程（客户端）和子进程（服务器）的通信
 * 3. 演示完整的 MCP 协议通信流程
 */

#include "mcp_client.h"
#include "../../log/include/logger.h"
#include "../../mcp_protocol/include/mcp_protocol_core.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <signal.h>
#include <atomic>
#include <filesystem>

using namespace mcp;
using namespace mcp_logger;

// 全局变量用于优雅关闭
static std::atomic<bool> g_running{true};
// 全局 logger 指针，用于信号处理函数
static std::shared_ptr<Logger::ModuleLogger> g_logger = nullptr;

// 信号处理函数
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        if (g_logger) {
            g_logger->warning("\n🛑 收到停止信号，正在关闭客户端...");
        }
        g_running = false;
    }
}

// 打印客户端信息
void printClientInfo(std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("🚀 MCP Client IPC 父子进程通信示例");
    logger->info("===================================");
    logger->info("📋 功能特性:");
    logger->info("  ✅ 通过 command 配置启动 server 子进程");
    logger->info("  ✅ 父进程（客户端）与子进程（服务器）IPC 通信");
    logger->info("  ✅ 完整的 MCP 协议通信流程");
    logger->info("  ✅ 文件系统操作工具调用");
    logger->info("  ✅ 资源管理和发现");
    logger->info("===================================");
}

// 打印使用说明
void printUsage(std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📖 使用说明:");
    logger->info("  1. 客户端将自动启动 server 作为子进程");
    logger->info("  2. 通过 IPC 管道进行父子进程通信");
    logger->info("  3. 支持的文件系统操作:");
    logger->info("     - read_file: 读取文件内容");
    logger->info("     - write_file: 写入文件内容");
    logger->info("     - list_directory: 列出目录内容");
    logger->info("     - create_directory: 创建目录");
    logger->info("     - delete_file: 删除文件或目录");
    logger->info("     - file_info: 获取文件/目录信息");
    logger->info("  4. 按 Ctrl+C 停止客户端（会自动停止子进程）");
}

// 初始化日志系统
void initializeLogger() {
    LogConfig config;
    config.level = mcp_logger::LogLevel::INFO;
    config.target = mcp_logger::LogTarget::BOTH;
    config.file_path = "logs/mcp_client_ipc_parent_child.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    config.auto_flush = true;
    
    // 确保日志目录存在
    std::filesystem::create_directories("logs");
    
    initializeLogger(config);
}

// 测试初始化协议
bool testInitialize(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 1: 初始化 MCP 协议");
    try {
        // 设置客户端信息
        client.setClientInfo("mcp-client-ipc-parent", "1.0.0");
        
        // 创建初始化参数
        InitializeParams init_params;
        init_params.protocolVersion = "2025-11-25";
        init_params.clientInfo.name = "mcp-client-ipc-parent";
        init_params.clientInfo.version = "1.0.0";
        init_params.capabilities = utils::createDefaultClientCapabilities();
        
        auto result = client.initialize(init_params);
        logger->info("  ✅ 初始化成功");
        logger->info("  服务器名称: " + result.serverInfo.name);
        logger->info("  服务器版本: " + result.serverInfo.version);
        logger->info("  协议版本: " + result.protocolVersion);
        logger->info("  ✅ 初始化成功 - 服务器: " + result.serverInfo.name + " v" + result.serverInfo.version);
        return true;
    } catch (const std::exception& e) {
        logger->error("  ❌ 初始化失败: " + std::string(e.what()));
        return false;
    }
}

// 测试发送初始化完成通知
bool testInitialized(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 2: 发送初始化完成通知");
    try {
        client.initialized();
        logger->info("  ✅ 初始化完成通知发送成功");
        return true;
    } catch (const std::exception& e) {
        logger->error("  ❌ 初始化完成通知发送失败: " + std::string(e.what()));
        return false;
    }
}

// 测试获取工具列表
bool testListTools(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 3: 获取可用工具列表");
    try {
        auto tools = client.listTools();
        logger->info("  ✅ 获取工具列表成功");
        logger->info("  工具数量: " + std::to_string(tools.size()));
        logger->info("  ✅ 获取工具列表成功 - 工具数量: " + std::to_string(tools.size()));
        for (const auto& tool : tools) {
            logger->info("    - " + tool.name + ": " + tool.description);
        }
        return true;
    } catch (const std::exception& e) {
        logger->error("  ❌ 获取工具列表失败: " + std::string(e.what()));
        return false;
    }
}

// 测试获取资源列表
bool testListResources(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 4: 获取可用资源列表");
    try {
        auto resources = client.listResources();
        logger->info("  ✅ 获取资源列表成功");
        logger->info("  资源数量: " + std::to_string(resources.size()));
        logger->info("  ✅ 获取资源列表成功 - 资源数量: " + std::to_string(resources.size()));
        for (const auto& resource : resources) {
            logger->info("    - " + resource.name + " (" + resource.uri + ")");
        }
        return true;
    } catch (const std::exception& e) {
        logger->error("  ❌ 获取资源列表失败: " + std::string(e.what()));
        return false;
    }
}

// 测试调用工具 - 读取文件
bool testReadFile(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 5: 调用工具 - 读取文件");
    try {
        auto result = client.callTool("read_file", {{"path", "README.md"}});
        if (result.isError) {
            logger->error("  ❌ 工具调用失败: " + result.errorMessage);
            return false;
        } else {
            logger->info("  ✅ 工具调用成功");
            logger->info("  结果: " + result.content.dump(2));
            logger->info("  ✅ 工具调用成功 - 读取文件 README.md");
            // 打印部分内容
            if (result.content.contains("contents") && result.content["contents"].is_array()) {
                auto contents = result.content["contents"];
                if (!contents.empty() && contents[0].contains("text")) {
                    std::string text = contents[0]["text"];
                    std::string preview = text.substr(0, std::min(100, (int)text.length()));
                    logger->info("  预览: " + preview + (text.length() > 100 ? "..." : ""));
                }
            }
            return true;
        }
    } catch (const std::exception& e) {
        logger->error("  ❌ 工具调用异常: " + std::string(e.what()));
        return false;
    }
}

// 测试调用工具 - 列出目录
bool testListDirectory(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 6: 调用工具 - 列出目录");
    try {
        auto result = client.callTool("list_directory", {{"path", "."}});
        if (result.isError) {
            logger->error("  ❌ 工具调用失败: " + result.errorMessage);
            return false;
        } else {
            logger->info("  ✅ 工具调用成功");
            logger->info("  结果: " + result.content.dump(2));
            logger->info("  ✅ 工具调用成功 - 列出当前目录");
            if (result.content.contains("entries") && result.content["entries"].is_array()) {
                auto entries = result.content["entries"];
                logger->info("  目录项数量: " + std::to_string(entries.size()));
                for (size_t i = 0; i < std::min(5UL, entries.size()); ++i) {
                    if (entries[i].contains("name")) {
                        logger->info("    - " + entries[i]["name"].get<std::string>());
                    }
                }
                if (entries.size() > 5) {
                    logger->info("    ... (还有 " + std::to_string(entries.size() - 5) + " 项)");
                }
            }
            return true;
        }
    } catch (const std::exception& e) {
        logger->error("  ❌ 工具调用异常: " + std::string(e.what()));
        return false;
    }
}

// 测试调用工具 - 写入文件
bool testWriteFile(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 7: 调用工具 - 写入文件");
    try {
        std::string test_content = "这是通过 IPC 父子进程通信写入的测试文件\n";
        test_content += "创建时间: " + std::to_string(std::time(nullptr)) + "\n";
        test_content += "传输方式: IPC (进程间通信)\n";
        test_content += "进程关系: 父进程(客户端) -> 子进程(服务器)\n";
        
        auto result = client.callTool("write_file", {
            {"path", "test_ipc_parent_child.txt"},
            {"contents", nlohmann::json::array({nlohmann::json::object({{"text", test_content}})})}
        });
        
        if (result.isError) {
            logger->error("  ❌ 工具调用失败: " + result.errorMessage);
            return false;
        } else {
            logger->info("  ✅ 工具调用成功");
            logger->info("  结果: " + result.content.dump(2));
            logger->info("  ✅ 工具调用成功 - 写入文件 test_ipc_parent_child.txt");
            return true;
        }
    } catch (const std::exception& e) {
        logger->error("  ❌ 工具调用异常: " + std::string(e.what()));
        return false;
    }
}

// 测试 Ping
bool testPing(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📤 测试 8: 发送 Ping 测试");
    try {
        auto result = client.ping();
        logger->info("  ✅ Ping 成功");
        logger->info("  响应: " + result.dump());
        return true;
    } catch (const std::exception& e) {
        logger->error("  ❌ Ping 失败: " + std::string(e.what()));
        return false;
    }
}

// 显示服务器信息
void displayServerInfo(MCPClient& client, std::shared_ptr<Logger::ModuleLogger> logger) {
    logger->info("\n📊 服务器信息:");
    try {
        auto server_info = client.getServerInfo();
        auto server_caps = client.getServerCapabilities();
        logger->info("  服务器名称: " + server_info.name);
        logger->info("  服务器版本: " + server_info.version);
        logger->info("  服务器能力: " + server_caps.toJson().dump(2));
    } catch (const std::exception& e) {
        logger->error("  获取服务器信息失败: " + std::string(e.what()));
    }
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 解析命令行参数
    std::string server_command = "./build/bin/mcp_filesystem_server_ipc_example";
    std::string root_path = ".";
    bool enable_write = true;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--server" && i + 1 < argc) {
            server_command = argv[++i];
        } else if (arg == "--root" && i + 1 < argc) {
            root_path = argv[++i];
        } else if (arg == "--read-only") {
            enable_write = false;
        } else if (arg == "--help") {
            // 帮助信息需要先初始化日志系统
            initializeLogger();
            auto help_logger = getModuleLogger("mcp_client_ipc_parent_child");
            help_logger->info("\n用法: " + std::string(argv[0]) + " [选项]");
            help_logger->info("选项:");
            help_logger->info("  --server <path>     设置服务器可执行文件路径");
            help_logger->info("                      (默认: ./build/bin/mcp_filesystem_server_ipc_example)");
            help_logger->info("  --root <path>       设置服务器根路径 (默认: 当前目录)");
            help_logger->info("  --read-only         禁用服务器写操作");
            help_logger->info("  --help             显示此帮助信息");
            return 0;
        }
    }
    
    // 初始化日志系统
    initializeLogger();
    auto logger = getModuleLogger("mcp_client_ipc_parent_child");
    g_logger = logger;  // 设置全局 logger 供信号处理函数使用
    
    logger->info("🔗 服务器命令: " + server_command);
    logger->info("📁 根路径: " + root_path);
    logger->info("✏️  写操作: " + std::string(enable_write ? "启用" : "禁用"));
    
    logger->info("🚀 启动 MCP Client IPC 父子进程通信示例");
    logger->info("服务器命令: " + server_command);
    logger->info("根路径: " + root_path);
    logger->info("写操作: " + std::string(enable_write ? "启用" : "禁用"));
    
    // 打印客户端信息
    printClientInfo(logger);
    
    try {
        // 创建 MCP 客户端实例
        auto client = std::make_unique<MCPClient>();
        
        // 创建 IPC 传输配置
        mcp_transport::TransportConfig config;
        config.type = mcp_transport::TransportType::IPC;
        config.mode = mcp_transport::TransportMode::CLIENT;
        config.enable_logging = true;
        config.log_level = 1;
        config.log_file_path = "logs/mcp_client_ipc_parent_child_transport.log";
        
        // IPC 配置 - 使用 command 启动 server 作为子进程
        config.ipc.command = server_command;
        // 添加服务器启动参数
        if (!root_path.empty()) {
            config.ipc.args.push_back("--root");
            config.ipc.args.push_back(root_path);
        }
        if (!enable_write) {
            config.ipc.args.push_back("--read-only");
        }
        
        // IPC 通信配置
        config.ipc.env_vars["NODE_ENV"] = "development";
        config.ipc.buffer_size = 8192;
        config.ipc.read_timeout_ms = 200;
        config.ipc.max_retry_count = 10;
        config.ipc.enable_logging = true;
        config.ipc.log_file_path = "logs/mcp_client_ipc_parent_child_pipe.log";
        config.ipc.log_level = mcp_logger::LogLevel::INFO;
        
        // 启动连接（会自动启动 server 子进程）
        logger->info("🚀 启动 MCP 客户端连接...");
        logger->info("📋 配置信息:");
        logger->info("  传输类型: IPC (父子进程通信)");
        logger->info("  服务器命令: " + config.ipc.command);
        std::string args_str = "  参数: ";
        for (const auto& arg : config.ipc.args) {
            args_str += arg + " ";
        }
        logger->info(args_str);
        logger->info("  缓冲区大小: " + std::to_string(config.ipc.buffer_size));
        logger->info("  读取超时: " + std::to_string(config.ipc.read_timeout_ms) + "ms");
        logger->info("  ⚠️  注意: 服务器将作为子进程自动启动");
        
        if (!client->connectWithConfig(config)) {
            logger->error("❌ 启动 MCP 客户端连接失败");
            logger->error("💡 提示: 请确保服务器可执行文件存在: " + server_command);
            return 1;
        }
        
        logger->info("✅ MCP 客户端连接成功");
        
        // 等待连接建立和服务初始化
        logger->info("⏳ 等待连接建立与服务初始化...");
        std::this_thread::sleep_for(std::chrono::milliseconds(3000));
        
        // 检查连接状态
        logger->info("🔍 连接状态检查:");
        logger->info("  连接状态: " + std::string(client->isConnected() ? "已连接" : "未连接"));
        logger->info("  传输类型: " + std::to_string(static_cast<int>(client->getTransportType())));
        logger->info("  传输状态: " + std::to_string(static_cast<int>(client->getTransportStatus())));
        
        if (!client->isConnected()) {
            logger->error("❌ 错误: 客户端未连接");
            return 1;
        }
        
        printUsage(logger);
        
        // 执行测试用例
        std::vector<std::function<bool()>> test_cases = {
            [&]() { return testInitialize(*client, logger); },
            [&]() { return testInitialized(*client, logger); },
            [&]() { return testListTools(*client, logger); },
            [&]() { return testListResources(*client, logger); },
            [&]() { return testReadFile(*client, logger); },
            [&]() { return testListDirectory(*client, logger); },
            [&]() { return testWriteFile(*client, logger); },
            [&]() { return testPing(*client, logger); }
        };
        
        int success_count = 0;
        int total_count = test_cases.size();
        
        logger->info("\n🧪 开始执行测试用例...");
        for (size_t i = 0; i < test_cases.size() && g_running; ++i) {
            if (test_cases[i]()) {
                success_count++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // 显示服务器信息
        displayServerInfo(*client, logger);
        
        // 显示测试结果
        logger->info("\n📊 测试结果:");
        logger->info("  成功: " + std::to_string(success_count) + "/" + std::to_string(total_count));
        
        // 保持连接一段时间，演示持续通信
        if (g_running) {
            logger->info("\n⏳ 保持连接 10 秒，演示持续通信...");
            for (int i = 0; i < 10 && g_running; ++i) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                if (i % 3 == 0) {
                    logger->info("💓 连接保持正常 (已运行 " + std::to_string(i) + " 秒)");
                }
            }
        }
        
        // 断开连接（会自动停止子进程）
        logger->info("\n🛑 断开 MCP 客户端连接...");
        logger->info("  ⚠️  注意: 服务器子进程将自动停止");
        client->disconnect();
        logger->info("✅ MCP 客户端连接已断开");
        
    } catch (const std::exception& e) {
        logger->error("❌ 错误: " + std::string(e.what()));
        return 1;
    }
    
    logger->info("\n✅ MCP Client IPC 父子进程通信示例完成");
    
    // 关闭日志系统
    Logger::getInstance().shutdown();
    return 0;
}

