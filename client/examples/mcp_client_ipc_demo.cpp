/**
 * @file ipc_example.cpp
 * @brief MCP Client IPC 通信测试示例
 * 
 * 这个文件专门演示如何使用 mcp_client 进行 IPC 通信，
 * 包括本地 npx 服务器启动和文件系统操作测试。
 */

#include "mcp_client.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

using namespace mcp;
using namespace mcp_logger;

int main() {
    // 初始化日志系统
    LogConfig config;
    config.level = mcp_logger::LogLevel::INFO;
    config.target = mcp_logger::LogTarget::BOTH;
    config.file_path = "./logs/mcp_client_ipc_demo.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    initializeLogger(config);
    
    getModuleLogger("mcp_client_ipc_demo")->info("🔧 MCP Client IPC 通信测试");
    getModuleLogger("mcp_client_ipc_demo")->info("=============================");
    
    try {
        // 创建 MCP 客户端实例
        auto client = std::make_unique<MCPClient>();
        
        // 创建 IPC 传输配置
        mcp_transport::TransportConfig config;
        config.type = mcp_transport::TransportType::IPC;
        config.mode = mcp_transport::TransportMode::CLIENT;
        config.enable_logging = false;  // 禁用transport层日志，只使用主日志文件
        config.log_level = 1;
        config.log_file_path = "";  // 清空transport层日志文件路径
        
        // IPC 配置 - 本地启动 npx 文件系统服务器
        //config.ipc.command = "npx";
        //config.ipc.args = {"-y", "@modelcontextprotocol/server-filesystem", "."};
        config.ipc.command = "../../server/build/bin/mcp_filesystem_server_ipc_example";
        config.ipc.env_vars["NODE_ENV"] = "development";
        config.ipc.buffer_size = 8192;
        config.ipc.read_timeout_ms = 200;
        config.ipc.max_retry_count = 10;
        config.ipc.enable_logging = true;
        config.ipc.log_file_path = "./logs/mcp_client_ipc.log";
        config.ipc.log_level = mcp_logger::LogLevel::INFO;
        
        // 启动连接
        getModuleLogger("mcp_client_ipc_demo")->info("🚀 启动 MCP 客户端连接...");
        getModuleLogger("mcp_client_ipc_demo")->info("📋 配置信息:");
        getModuleLogger("mcp_client_ipc_demo")->info("  传输类型: IPC (本地 npx)");
        getModuleLogger("mcp_client_ipc_demo")->info("  命令: " + config.ipc.command);
        std::string args_str = "  参数: ";
        for (const auto& arg : config.ipc.args) {
            args_str += arg + " ";
        }
        getModuleLogger("mcp_client_ipc_demo")->info(args_str);
        getModuleLogger("mcp_client_ipc_demo")->info("  工作目录: 当前目录");
        
        if (!client->connectWithConfig(config)) {
            getModuleLogger("mcp_client_ipc_demo")->error("❌ 启动 MCP 客户端连接失败");
            getModuleLogger("mcp_client_ipc_demo")->info("💡 提示: 请确保已安装 Node.js 和 npm");
            getModuleLogger("mcp_client_ipc_demo")->info("💡 提示: 可以尝试运行: npm install -g @modelcontextprotocol/server-filesystem");
            return 1;
        }
        
        getModuleLogger("mcp_client_ipc_demo")->info("✅ MCP 客户端连接成功");
        
        // 等待连接建立
        getModuleLogger("mcp_client_ipc_demo")->info("⏳ 等待连接建立与服务初始化...");
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        
        // 检查连接状态
        getModuleLogger("mcp_client_ipc_demo")->info("🔍 连接状态检查:");
        getModuleLogger("mcp_client_ipc_demo")->info("  连接状态: " + std::string(client->isConnected() ? "已连接" : "未连接"));
        getModuleLogger("mcp_client_ipc_demo")->info("  传输类型: " + std::to_string(static_cast<int>(client->getTransportType())));
        getModuleLogger("mcp_client_ipc_demo")->info("  传输状态: " + std::to_string(static_cast<int>(client->getTransportStatus())));
        
        if (!client->isConnected()) {
            getModuleLogger("mcp_client_ipc_demo")->error("❌ 错误: 客户端未连接");
            return 1;
        }
        
        // 设置客户端信息
        client->setClientInfo("mcp-client-ipc-example", "1.0.0");
        
        // 创建初始化参数
        InitializeParams init_params;
        init_params.protocolVersion = "2025-11-25";
        init_params.clientInfo.name = "mcp-client-ipc-example";
        init_params.clientInfo.version = "1.0.0";
        init_params.capabilities = utils::createDefaultClientCapabilities();
        
        // 测试用例列表
        std::vector<std::function<void()>> test_cases = {
            [&]() {
                getModuleLogger("mcp_client_ipc_demo")->info("\n📤 测试 1: 初始化 MCP 协议");
                try {
                    auto result = client->initialize(init_params);
                    getModuleLogger("mcp_client_ipc_demo")->info("  ✅ 初始化成功");
                    getModuleLogger("mcp_client_ipc_demo")->info("  服务器: " + result.serverInfo.name + " v" + result.serverInfo.version);
                    getModuleLogger("mcp_client_ipc_demo")->info("  协议版本: " + result.protocolVersion);
                } catch (const std::exception& e) {
                    getModuleLogger("mcp_client_ipc_demo")->error("  ❌ 初始化失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                getModuleLogger("mcp_client_ipc_demo")->info("\n📤 测试 2: 发送初始化完成通知");
                try {
                    client->initialized();
                    getModuleLogger("mcp_client_ipc_demo")->info("  ✅ 初始化完成通知发送成功");
                } catch (const std::exception& e) {
                    getModuleLogger("mcp_client_ipc_demo")->error("  ❌ 初始化完成通知发送失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                getModuleLogger("mcp_client_ipc_demo")->info("\n📤 测试 3: 获取可用工具列表");
                try {
                    auto tools = client->listTools();
                    getModuleLogger("mcp_client_ipc_demo")->info("  ✅ 获取工具列表成功");
                    getModuleLogger("mcp_client_ipc_demo")->info("  工具数量: " + std::to_string(tools.size()));
                    for (const auto& tool : tools) {
                        getModuleLogger("mcp_client_ipc_demo")->info("    - " + tool.name + ": " + tool.description);
                    }
                } catch (const std::exception& e) {
                    getModuleLogger("mcp_client_ipc_demo")->error("  ❌ 获取工具列表失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                getModuleLogger("mcp_client_ipc_demo")->info("\n📤 测试 4: 获取可用资源列表");
                try {
                    auto resources = client->listResources();
                    getModuleLogger("mcp_client_ipc_demo")->info("  ✅ 获取资源列表成功");
                    getModuleLogger("mcp_client_ipc_demo")->info("  资源数量: " + std::to_string(resources.size()));
                    for (const auto& resource : resources) {
                        getModuleLogger("mcp_client_ipc_demo")->info("    - " + resource.name + " (" + resource.uri + ")");
                    }
                } catch (const std::exception& e) {
                    getModuleLogger("mcp_client_ipc_demo")->error("  ❌ 获取资源列表失败: " + std::string(e.what()));
                }
            },
             
            [&]() {
                getModuleLogger("mcp_client_ipc_demo")->info("\n📤 测试 6: 获取提示列表");
                try {
                    auto prompts = client->listPrompts();
                    getModuleLogger("mcp_client_ipc_demo")->info("  ✅ 获取提示列表成功");
                    getModuleLogger("mcp_client_ipc_demo")->info("  提示数量: " + std::to_string(prompts.size()));
                    for (const auto& prompt : prompts) {
                        getModuleLogger("mcp_client_ipc_demo")->info("    - " + prompt.name + ": " + prompt.description);
                    }
                } catch (const std::exception& e) {
                    getModuleLogger("mcp_client_ipc_demo")->error("  ❌ 获取提示列表失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                getModuleLogger("mcp_client_ipc_demo")->info("\n📤 测试 7: 发送 ping 测试");
                try {
                    auto result = client->ping();
                    getModuleLogger("mcp_client_ipc_demo")->info("  ✅ Ping 成功");
                    getModuleLogger("mcp_client_ipc_demo")->info("  响应: " + result.dump());
                } catch (const std::exception& e) {
                    getModuleLogger("mcp_client_ipc_demo")->error("  ❌ Ping 失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                getModuleLogger("mcp_client_ipc_demo")->info("\n📤 测试 8: 调用工具示例");
                try {
                    // 使用新的 call_tool 重载版本
                    auto result = client->callTool("read_file", {{"path", "README.md"}});
                    if (result.isError) {
                        getModuleLogger("mcp_client_ipc_demo")->error("  ❌ 工具调用失败: " + result.errorMessage);
                    } else {
                        getModuleLogger("mcp_client_ipc_demo")->info("  ✅ 工具调用成功");
                        getModuleLogger("mcp_client_ipc_demo")->info("  结果: " + result.content.dump(2));
                    }
                } catch (const std::exception& e) {
                    getModuleLogger("mcp_client_ipc_demo")->error("  ❌ 工具调用异常: " + std::string(e.what()));
                }
            }
        };
        
        // 执行测试用例
        for (size_t i = 0; i < test_cases.size(); ++i) {
            test_cases[i]();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // 显示服务器信息
        getModuleLogger("mcp_client_ipc_demo")->info("\n📊 服务器信息:");
        try {
            auto server_info = client->getServerInfo();
            auto server_caps = client->getServerCapabilities();
            getModuleLogger("mcp_client_ipc_demo")->info("  服务器名称: " + server_info.name);
            getModuleLogger("mcp_client_ipc_demo")->info("  服务器版本: " + server_info.version);
            getModuleLogger("mcp_client_ipc_demo")->info("  服务器能力: " + server_caps.toJson().dump(2));
        } catch (const std::exception& e) {
            getModuleLogger("mcp_client_ipc_demo")->error("  获取服务器信息失败: " + std::string(e.what()));
        }
        
        // 断开连接
        getModuleLogger("mcp_client_ipc_demo")->info("\n🛑 断开 MCP 客户端连接...");
        client->disconnect();
        getModuleLogger("mcp_client_ipc_demo")->info("✅ MCP 客户端连接已断开");
        
    } catch (const std::exception& e) {
        getModuleLogger("mcp_client_ipc_demo")->error("❌ 错误: " + std::string(e.what()));
        return 1;
    }
    
    getModuleLogger("mcp_client_ipc_demo")->info("\n✅ MCP Client IPC 通信测试完成");
    return 0;
}