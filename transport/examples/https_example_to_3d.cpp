/**
 * @file sse_example.cpp
 * @brief SSE 通信测试示例
 * 
 * 这个文件专门演示如何使用 transport 模块进行 SSE 通信，
 * 包括 HTTPS 连接、API 调用和错误处理测试。
 */

#include "../include/https_transport.h"
#include "../include/transport_factory.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

using namespace mcp_transport;
using namespace mcp_logger;

// 初始化日志系统
void initializeLogger() {
    Logger& logger = Logger::getInstance();
    LogConfig config;
    config.level = LogLevel::INFO;
    config.target = LogTarget::BOTH;
    config.file_path = "sse_https_example.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    logger.initialize(config);
}

int main() {
    // 初始化日志系统
    initializeLogger();
    mcp_logger::getModuleLogger("sse_https_example")->info("🌐 MCP Transport SSE 通信测试");
    mcp_logger::getModuleLogger("sse_https_example")->info("=============================");
    
    try {
        // 创建传输实例
        auto transport = TransportFactory::createHttpsTransport();
        
        // 创建 SSE 专用配置
        TransportConfig config;
        config.type = TransportType::HTTPS;
        config.enable_logging = true;
        config.log_level = 1;
        config.log_file_path = "sse_test.log";
        
        // SSE 配置 - 连接到远程 MCP 服务器
        config.sse.host = "mcp.didichuxing.com";
        config.sse.port = 443;
        config.sse.protocol = netio::Protocol::HTTPS;
        config.sse.timeout_seconds = 30;
        config.sse.max_retries = 3;
        config.sse.user_agent = "MCP-Transport-SSE/1.0";
        config.sse.keep_alive = true;
        config.sse.auto_reconnect = true;
        config.sse.reconnect_interval = 5;
        config.sse.headers["Content-Type"] = "application/json; charset=utf-8";
        config.sse.headers["Accept"] = "application/json, text/event-stream";
        config.sse.headers["User-Agent"] = "MCP-Transport-SSE/1.0";
        
        // 启动传输
        mcp_logger::getModuleLogger("sse_https_example")->info("🚀 启动 SSE 传输...");
        mcp_logger::getModuleLogger("sse_https_example")->info("📋 配置信息:");
        mcp_logger::getModuleLogger("sse_https_example")->info("  传输类型: SSE");
        mcp_logger::getModuleLogger("sse_https_example")->info("  主机: " + config.sse.host);
        mcp_logger::getModuleLogger("sse_https_example")->info("  端口: " + std::to_string(config.sse.port));
        mcp_logger::getModuleLogger("sse_https_example")->info("  协议: " + std::string(config.sse.protocol == netio::Protocol::HTTPS ? "HTTPS" : "HTTP"));
        mcp_logger::getModuleLogger("sse_https_example")->info("  超时: " + std::to_string(config.sse.timeout_seconds) + " 秒");
        mcp_logger::getModuleLogger("sse_https_example")->info("  最大重试: " + std::to_string(config.sse.max_retries));
        
        if (!transport->start(config)) {
            mcp_logger::getModuleLogger("sse_https_example")->info("❌ 启动 SSE 传输失败");
            mcp_logger::getModuleLogger("sse_https_example")->info("💡 提示: 请检查网络连接和服务器地址");
            return 1;
        }
        
        mcp_logger::getModuleLogger("sse_https_example")->info("✅ HTTP 传输启动成功");
        
        // 等待连接建立
        mcp_logger::getModuleLogger("sse_https_example")->info("⏳ 等待连接建立...");
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        
        // 检查连接状态
        auto conn_info = transport->getConnectionInfo();
        mcp_logger::getModuleLogger("sse_https_example")->info("🔍 连接状态检查:");
        mcp_logger::getModuleLogger("sse_https_example")->info("  状态: " + std::to_string(static_cast<int>(conn_info.status)));
        mcp_logger::getModuleLogger("sse_https_example")->info("  类型: " + std::to_string(static_cast<int>(conn_info.type)));
        mcp_logger::getModuleLogger("sse_https_example")->info("  运行中: " + std::string(transport->isRunning() ? "是" : "否"));
        mcp_logger::getModuleLogger("sse_https_example")->info("  健康检查: " + std::string(transport->healthCheck() ? "通过" : "失败"));
        
        if (conn_info.type != TransportType::HTTPS) {
            mcp_logger::getModuleLogger("sse_https_example")->info("❌ 错误: 传输类型不是 HTTPS");
            return 1;
        }
        
        // 发送 SSE 测试消息
        std::vector<std::string> http_test_messages = {
            R"({"jsonrpc": "2.0", "method": "initialize", "id": 1, "params": {"protocolVersion": "2024-11-05", "capabilities": {}, "clientInfo": {"name": "http-example", "version": "1.0.0"}}})",
            R"({"jsonrpc": "2.0", "method": "tools/list", "id": 2, "params": {}})",
            R"({"jsonrpc": "2.0", "method": "resources/list", "id": 3, "params": {}})",
            R"({"jsonrpc": "2.0", "method": "ping", "id": 4, "params": {}})"
        };
        
        std::vector<std::string> test_descriptions = {
            "初始化 MCP 协议",
            "获取可用工具列表",
            "获取可用资源列表",
            "发送 ping 测试"
        };
        
        std::vector<std::string> test_endpoints = {
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ"
        };
        
        for (size_t i = 0; i < http_test_messages.size(); ++i) {
            mcp_logger::getModuleLogger("sse_https_example")->info("\n📤 测试 " + std::to_string(i + 1) + ": " + test_descriptions[i]);
            mcp_logger::getModuleLogger("sse_https_example")->info("  端点: " + test_endpoints[i]);
            
            // 使用 HTTP 方式发送消息
            TransportMessage msg;
            msg.content = http_test_messages[i];
            msg.method = "POST";
            msg.path = test_endpoints[i];
            msg.timestamp = std::chrono::system_clock::now();
            TransportMessage response = transport->sendRequestSync(msg);
            
            mcp_logger::getModuleLogger("sse_https_example")->info("📥 收到响应:");
            mcp_logger::getModuleLogger("sse_https_example")->info("  状态码: " + std::to_string(response.status_code));
            mcp_logger::getModuleLogger("sse_https_example")->info("  HTTP 状态: " + std::to_string(response.status_code));
            
            if (response.status_code == 200) {
                mcp_logger::getModuleLogger("sse_https_example")->info("  ✅ 成功");
                // 尝试解析 JSON 响应
                try {
                    auto json_response = nlohmann::json::parse(response.content);
                    if (json_response.contains("result")) {
                        mcp_logger::getModuleLogger("sse_https_example")->info("  结果: " + json_response["result"].dump(2));
                    } else if (json_response.contains("error")) {
                        mcp_logger::getModuleLogger("sse_https_example")->error("  错误: " + json_response["error"].dump(2));
                    } else {
                        mcp_logger::getModuleLogger("sse_https_example")->info("  内容: " + response.content);
                    }
                } catch (const std::exception& e) {
                    mcp_logger::getModuleLogger("sse_https_example")->info("  内容: " + response.content);
                }
            } else {
                mcp_logger::getModuleLogger("sse_https_example")->info("  ❌ 失败");
                mcp_logger::getModuleLogger("sse_https_example")->error("  错误: " + response.error_message);
                mcp_logger::getModuleLogger("sse_https_example")->info("  内容: " + response.content);
                
                // 根据状态码提供具体建议
                if (response.status_code == 404) {
                    mcp_logger::getModuleLogger("sse_https_example")->info("  💡 建议: 检查 API 端点是否正确");
                } else if (response.status_code == 401) {
                    mcp_logger::getModuleLogger("sse_https_example")->info("  💡 建议: 检查 API 密钥是否正确");
                } else if (response.status_code == 500) {
                    mcp_logger::getModuleLogger("sse_https_example")->info("  💡 建议: 服务器内部错误，请稍后重试");
                }
            }
            
            // 等待一下再发送下一条消息
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // 测试不同的 HTTP 方法
        mcp_logger::getModuleLogger("sse_https_example")->info("\n🔧 测试不同 HTTP 方法:");
        
        // GET 请求测试
        mcp_logger::getModuleLogger("sse_https_example")->info("\n📤 测试 GET 请求...");
        TransportMessage get_msg;
        get_msg.content = "";
        get_msg.method = "GET";
        get_msg.path = "/status";
        get_msg.timestamp = std::chrono::system_clock::now();
        auto get_response = transport->sendRequestSync(get_msg);
        mcp_logger::getModuleLogger("sse_https_example")->info("📥 GET 响应: " + std::to_string(get_response.status_code) + " - " + get_response.content);
        
        // PUT 请求测试
        mcp_logger::getModuleLogger("sse_https_example")->info("\n📤 测试 PUT 请求...");
        TransportMessage put_msg;
        put_msg.content = R"({"test": "data"})";
        put_msg.method = "PUT";
        put_msg.path = "/test";
        put_msg.timestamp = std::chrono::system_clock::now();
        auto put_response = transport->sendRequestSync(put_msg);
        mcp_logger::getModuleLogger("sse_https_example")->info("📥 PUT 响应: " + std::to_string(put_response.status_code) + " - " + put_response.content);
        
        // 显示统计信息
        mcp_logger::getModuleLogger("sse_https_example")->info("\n📊 HTTP 统计信息:");
        auto stats = transport->getStatistics();
        mcp_logger::getModuleLogger("sse_https_example")->info(stats.dump(2));
        
        // 停止传输
        mcp_logger::getModuleLogger("sse_https_example")->info("\n🛑 停止 HTTP 传输...");
        transport->stop();
        mcp_logger::getModuleLogger("sse_https_example")->info("✅ HTTP 传输已停止");
        
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("sse_https_example")->error("❌ 错误: " + std::string(e.what()));
        return 1;
    }
    
    mcp_logger::getModuleLogger("sse_https_example")->info("\n✅ HTTP 通信测试完成");
    return 0;
}
