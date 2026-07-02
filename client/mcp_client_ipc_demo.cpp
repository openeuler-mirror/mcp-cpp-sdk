/**
 * @file ipc_example.cpp
 * @brief MCP Client IPC 通信测试示例
 * 
 * 这个文件专门演示如何使用 mcp_client 进行 IPC 通信，
 * 包括本地 npx 服务器启动和文件系统操作测试。
 */

#include "mcp_client.h"
#include "../log/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

using namespace mcp;
using namespace mcp_logger;

// 初始化日志系统
void initialize_logger() {
    Logger& logger = Logger::getInstance();
    LogConfig config;
    config.level = mcp_logger::LogLevel::INFO;
    config.target = mcp_logger::LogTarget::BOTH;
    config.file_path = "mcp_client_ipc_example.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    logger.initialize(config);
}

int main() {
    // 初始化日志系统
    initialize_logger();
    MCP_LOG_INFO("🔧 MCP Client IPC 通信测试");
    MCP_LOG_INFO("=============================");
    
    try {
        // 创建 MCP 客户端实例
        auto client = std::make_unique<MCPClient>();
        
        // 创建 IPC 传输配置
        mcp_transport::TransportConfig config;
        config.type = mcp_transport::TransportType::IPC;
        config.enable_logging = true;
        config.log_level = 1;
        config.log_file_path = "mcp_client_ipc_test.log";
        
        // IPC 配置 - 本地启动 npx 文件系统服务器
        config.ipc.command = "npx";
        config.ipc.args = {"-y", "@modelcontextprotocol/server-filesystem", "."};
        config.ipc.env_vars["NODE_ENV"] = "development";
        config.ipc.buffer_size = 2048;
        config.ipc.read_timeout_ms = 100;
        config.ipc.max_retry_count = 10;
        config.ipc.enable_logging = true;
        config.ipc.log_file_path = "mcp_client_ipc.log";
        config.ipc.log_level = mcp::LogLevel::INFO;
        
        // 启动连接
        MCP_LOG_INFO("🚀 启动 MCP 客户端连接...");
        MCP_LOG_INFO("📋 配置信息:");
        MCP_LOG_INFO("  传输类型: IPC (本地 npx)");
        MCP_LOG_INFO("  命令: " + config.ipc.command);
        std::string args_str = "  参数: ";
        for (const auto& arg : config.ipc.args) {
            args_str += arg + " ";
        }
        MCP_LOG_INFO(args_str);
        MCP_LOG_INFO("  工作目录: 当前目录");
        
        if (!client->connect_with_config(config)) {
            MCP_LOG_ERROR("❌ 启动 MCP 客户端连接失败");
            MCP_LOG_INFO("💡 提示: 请确保已安装 Node.js 和 npm");
            MCP_LOG_INFO("💡 提示: 可以尝试运行: npm install -g @modelcontextprotocol/server-filesystem");
            return 1;
        }
        
        MCP_LOG_INFO("✅ MCP 客户端连接成功");
        
        // 等待连接建立
        MCP_LOG_INFO("⏳ 等待连接建立与服务初始化...");
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        
        // 检查连接状态
        MCP_LOG_INFO("🔍 连接状态检查:");
        MCP_LOG_INFO("  连接状态: " + std::string(client->is_connected() ? "已连接" : "未连接"));
        MCP_LOG_INFO("  传输类型: " + std::to_string(static_cast<int>(client->get_transport_type())));
        MCP_LOG_INFO("  传输状态: " + std::to_string(static_cast<int>(client->get_transport_status())));
        
        if (!client->is_connected()) {
            MCP_LOG_ERROR("❌ 错误: 客户端未连接");
            return 1;
        }
        
        // 设置客户端信息
        client->set_client_info("mcp-client-ipc-example", "1.0.0");
        
        // 创建初始化参数
        InitializeParams init_params;
        init_params.protocolVersion = "2024-11-05";
        init_params.clientInfo.name = "mcp-client-ipc-example";
        init_params.clientInfo.version = "1.0.0";
        init_params.capabilities = utils::create_default_client_capabilities();
        
        // 测试用例列表
        std::vector<std::function<void()>> test_cases = {
            [&]() {
                MCP_LOG_INFO("\n📤 测试 1: 初始化 MCP 协议");
                try {
                    auto result = client->initialize(init_params);
                    MCP_LOG_INFO("  ✅ 初始化成功");
                    MCP_LOG_INFO("  服务器: " + result.serverInfo.name + " v" + result.serverInfo.version);
                    MCP_LOG_INFO("  协议版本: " + result.protocolVersion);
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 初始化失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 2: 发送初始化完成通知");
                try {
                    client->initialized();
                    MCP_LOG_INFO("  ✅ 初始化完成通知发送成功");
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 初始化完成通知发送失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 3: 获取可用工具列表");
                try {
                    auto tools = client->list_tools();
                    MCP_LOG_INFO("  ✅ 获取工具列表成功");
                    MCP_LOG_INFO("  工具数量: " + std::to_string(tools.size()));
                    for (const auto& tool : tools) {
                        MCP_LOG_INFO("    - " + tool.name + ": " + tool.description);
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 获取工具列表失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 4: 获取可用资源列表");
                try {
                    auto resources = client->list_resources();
                    MCP_LOG_INFO("  ✅ 获取资源列表成功");
                    MCP_LOG_INFO("  资源数量: " + std::to_string(resources.size()));
                    for (const auto& resource : resources) {
                        MCP_LOG_INFO("    - " + resource.name + " (" + resource.uri + ")");
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 获取资源列表失败: " + std::string(e.what()));
                }
            },
             
            [&]() {
                MCP_LOG_INFO("\n📤 测试 6: 获取提示列表");
                try {
                    auto prompts = client->list_prompts();
                    MCP_LOG_INFO("  ✅ 获取提示列表成功");
                    MCP_LOG_INFO("  提示数量: " + std::to_string(prompts.size()));
                    for (const auto& prompt : prompts) {
                        MCP_LOG_INFO("    - " + prompt.name + ": " + prompt.description);
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 获取提示列表失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 7: 发送 ping 测试");
                try {
                    auto result = client->ping();
                    MCP_LOG_INFO("  ✅ Ping 成功");
                    MCP_LOG_INFO("  响应: " + result.dump());
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ Ping 失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 8: 调用工具示例");
                try {
                    // 使用新的 call_tool 重载版本
                    auto result = client->call_tool("read_file", {{"path", "README.md"}});
                    if (result.isError) {
                        MCP_LOG_ERROR("  ❌ 工具调用失败: " + result.errorMessage);
                    } else {
                        MCP_LOG_INFO("  ✅ 工具调用成功");
                        MCP_LOG_INFO("  结果: " + result.content.dump(2));
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 工具调用异常: " + std::string(e.what()));
                }
            }
        };
        
        // 执行测试用例
        for (size_t i = 0; i < test_cases.size(); ++i) {
            test_cases[i]();
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // 显示服务器信息
        MCP_LOG_INFO("\n📊 服务器信息:");
        try {
            auto server_info = client->get_server_info();
            auto server_caps = client->get_server_capabilities();
            MCP_LOG_INFO("  服务器名称: " + server_info.name);
            MCP_LOG_INFO("  服务器版本: " + server_info.version);
            MCP_LOG_INFO("  服务器能力: " + server_caps.to_json().dump(2));
        } catch (const std::exception& e) {
            MCP_LOG_ERROR("  获取服务器信息失败: " + std::string(e.what()));
        }
        
        // 断开连接
        MCP_LOG_INFO("\n🛑 断开 MCP 客户端连接...");
        client->disconnect();
        MCP_LOG_INFO("✅ MCP 客户端连接已断开");
        
    } catch (const std::exception& e) {
        MCP_LOG_ERROR("❌ 错误: " + std::string(e.what()));
        return 1;
    }
    
    MCP_LOG_INFO("\n✅ MCP Client IPC 通信测试完成");
    return 0;
}