/**
 * @file ipc_example.cpp
 * @brief IPC 通信测试示例
 * 
 * 这个文件专门演示如何使用 transport 模块进行 IPC 通信，
 * 包括本地 npx 服务器启动和文件系统操作测试。
 */

#include "transport.h"
#include "../log/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include "mcp_protocol_messages.h"

using namespace mcp_transport;
using namespace mcp_logger;

// 初始化日志系统
void initialize_logger() {
    Logger& logger = Logger::getInstance();
    LogConfig config;
    config.level = LogLevel::INFO;
    config.target = LogTarget::BOTH;
    config.file_path = "ipc_example.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    logger.initialize(config);
}

int main() {
    // 初始化日志系统
    initialize_logger();
    TRANSPORT_LOG_INFO("🔧 MCP Transport IPC 通信测试");
    TRANSPORT_LOG_INFO("=============================");
    
    try {
        // 创建传输实例
        auto transport = std::make_unique<Transport>();
        
        // 创建 IPC 专用配置
        TransportConfig config;
        config.type = TransportType::IPC;
        config.enable_logging = true;
        config.log_level = 1;
        config.log_file_path = "ipc_test.log";
        
        // IPC 配置 - 本地启动 npx 文件系统服务器（与 pipe_communication_example 对应）
        config.ipc.command = "npx";
        config.ipc.args = {"-y", "@modelcontextprotocol/server-filesystem", "."};
        config.ipc.env_vars["NODE_ENV"] = "development";
        config.ipc.buffer_size = 2048;  // 与 pipe_communication_example 一致
        config.ipc.read_timeout_ms = 100;  // 与 pipe_communication_example 一致
        config.ipc.max_retry_count = 10;
        config.ipc.enable_logging = true;
        config.ipc.log_file_path = "ipc.log";
        config.ipc.log_level = mcp::LogLevel::INFO;
        
        // 启动传输
        TRANSPORT_LOG_INFO("🚀 启动 IPC 传输...");
        TRANSPORT_LOG_INFO("📋 配置信息:");
        TRANSPORT_LOG_INFO("  传输类型: IPC (本地 npx)");
        TRANSPORT_LOG_INFO("  命令: " + config.ipc.command);
        std::string args_str = "  参数: ";
        for (const auto& arg : config.ipc.args) {
            args_str += arg + " ";
        }
        TRANSPORT_LOG_INFO(args_str);
        TRANSPORT_LOG_INFO("  工作目录: 当前目录");
        
        if (!transport->start(config)) {
            TRANSPORT_LOG_INFO("❌ 启动 IPC 传输失败");
            TRANSPORT_LOG_INFO("💡 提示: 请确保已安装 Node.js 和 npm");
            TRANSPORT_LOG_INFO("💡 提示: 可以尝试运行: npm install -g @modelcontextprotocol/server-filesystem");
            return 1;
        }
        
        TRANSPORT_LOG_INFO("✅ IPC 传输启动成功");
        
        // 等待连接建立
        TRANSPORT_LOG_INFO("⏳ 等待连接建立与服务初始化...");
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));
        
        // 检查连接状态
        auto conn_info = transport->get_connection_info();
        TRANSPORT_LOG_INFO("🔍 连接状态检查:");
        TRANSPORT_LOG_INFO("  状态: " + std::to_string(static_cast<int>(conn_info.status)));
        TRANSPORT_LOG_INFO("  类型: " + std::to_string(static_cast<int>(conn_info.type)));
        TRANSPORT_LOG_INFO("  运行中: " + std::string(transport->is_running() ? "是" : "否"));
        TRANSPORT_LOG_INFO("  健康检查: " + std::string(transport->health_check() ? "通过" : "失败"));
        
        if (conn_info.type != TransportType::IPC) {
            TRANSPORT_LOG_INFO("❌ 错误: 传输类型不是 IPC");
            return 1;
        }
        
        // 使用协议构造函数生成 IPC 测试消息
        using namespace mcp_protocol;
        std::vector<std::string> ipc_test_messages = {
            create_initialize_message("ipc-example", "1.0.0"),
            //create_notifications_initialized(),
            create_list_tools_message(2),
            create_list_resources_message(3),
            create_read_file_message("README.md", -1, -1, 4),
            create_list_directory_message(".", 5),
            create_ping_message(6),
            create_list_directory_message(".", 7),
            create_write_file_message("test.txt", "Hello World", 8),
        };
        
        std::vector<std::string> test_descriptions = {
            "初始化 MCP 协议",
            //"发送初始化完成通知",
            "获取可用工具列表",
            "获取可用资源列表",
            "读取 README.md 文件",
            "列出当前目录内容",
            "发送 ping 测试"
        };
        
        for (size_t i = 0; i < ipc_test_messages.size(); ++i) {
            TRANSPORT_LOG_INFO("\n📤 测试 " + std::to_string(i + 1) + ": " + test_descriptions[i]);
            
            const int max_attempts = 5;
            int attempt = 0;
            TransportMessage response;
            while (attempt < max_attempts) {
                ++attempt;
                response = transport->send_request_sync(ipc_test_messages[i]);
                if (response.status_code == 200) break;
                TRANSPORT_LOG_WARNING("  重试(" + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ")，状态: " + std::to_string(response.status_code));
                std::this_thread::sleep_for(std::chrono::milliseconds(800 * attempt));
            }
            
            TRANSPORT_LOG_INFO("📥 收到响应:");
            TRANSPORT_LOG_INFO("  状态码: " + std::to_string(response.status_code));
            
            if (response.status_code == 200) {
                TRANSPORT_LOG_INFO("  ✅ 成功");
                try {
                    auto json_response = nlohmann::json::parse(response.content);
                    if (json_response.contains("result")) {
                        TRANSPORT_LOG_INFO("  结果: " + json_response["result"].dump(2));
                    } else if (json_response.contains("error")) {
                        TRANSPORT_LOG_ERROR("  错误: " + json_response["error"].dump(2));
                    } else {
                        TRANSPORT_LOG_INFO("  内容: " + response.content);
                    }
                } catch (const std::exception& e) {
                    TRANSPORT_LOG_INFO("  内容: " + response.content);
                }
            } else {
                TRANSPORT_LOG_INFO("  ❌ 失败(重试已用尽)");
                TRANSPORT_LOG_ERROR("  错误: " + response.error_message);
                TRANSPORT_LOG_INFO("  内容: " + response.content);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(800));
        }
        
        // 显示统计信息
        TRANSPORT_LOG_INFO("\n📊 IPC 统计信息:");
        auto stats = transport->get_statistics();
        TRANSPORT_LOG_INFO(stats.dump(2));
        
        // 停止传输
        TRANSPORT_LOG_INFO("\n🛑 停止 IPC 传输...");
        transport->stop();
        TRANSPORT_LOG_INFO("✅ IPC 传输已停止");
        
    } catch (const std::exception& e) {
        TRANSPORT_LOG_ERROR("❌ 错误: " + std::string(e.what()));
        return 1;
    }
    
    TRANSPORT_LOG_INFO("\n✅ IPC 通信测试完成");
    return 0;
}
