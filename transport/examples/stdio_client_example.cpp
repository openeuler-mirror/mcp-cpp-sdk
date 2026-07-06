#include "../include/stdio_transport.h"
#include "../include/transport_factory.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <fstream>
#include <sstream>

using namespace mcp_transport;

// 示例消息处理器
class ExampleMessageHandler {
public:
    void onMessage(const TransportMessage& message) {
        auto logger = mcp_logger::getModuleLogger("stdio_client");
        logger->info("📨 Received message: " + message.content);
    }
};

void testPingRequest(TransportBase& transport) {
    auto logger = mcp_logger::getModuleLogger("stdio_client");
    logger->info("🔍 Testing ping request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "ping_1";
    request["method"] = "ping";
    request["params"] = nlohmann::json::object();
    
    TransportMessage msg;
    msg.id = "ping_1";
    msg.content = request.dump();  // content已经是JSON-RPC格式，不需要设置method和path
    msg.timestamp = std::chrono::system_clock::now();
    
    auto response = transport.sendRequestSync(msg);
    
    if (response.status_code == 200) {
        logger->info("✅ Ping response: " + response.content);
    } else {
        logger->error("❌ Ping failed: " + response.error_message);
    }
}

void testEchoRequest(TransportBase& transport) {
    auto logger = mcp_logger::getModuleLogger("stdio_client");
    logger->info("🔍 Testing echo request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "echo_1";
    request["method"] = "echo";
    request["params"] = "Hello from client!";
    
    TransportMessage msg;
    msg.id = "echo_1";
    msg.content = request.dump();  // content已经是JSON-RPC格式，不需要设置method和path
    msg.timestamp = std::chrono::system_clock::now();
    
    auto response = transport.sendRequestSync(msg);
    
    if (response.status_code == 200) {
        logger->info("✅ Echo response: " + response.content);
    } else {
        logger->error("❌ Echo failed: " + response.error_message);
    }
}

void testAddRequest(TransportBase& transport) {
    auto logger = mcp_logger::getModuleLogger("stdio_client");
    logger->info("🔍 Testing add request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "add_1";
    request["method"] = "add";
    
    nlohmann::json params;
    params["a"] = 42;
    params["b"] = 58;
    request["params"] = params;
    
    TransportMessage msg;
    msg.id = "add_1";
    msg.content = request.dump();  // content已经是JSON-RPC格式，不需要设置method和path
    msg.timestamp = std::chrono::system_clock::now();
    
    auto response = transport.sendRequestSync(msg);
    
    if (response.status_code == 200) {
        logger->info("✅ Add response: " + response.content);
    } else {
        logger->error("❌ Add failed: " + response.error_message);
    }
}

void testGetTimeRequest(TransportBase& transport) {
    auto logger = mcp_logger::getModuleLogger("stdio_client");
    logger->info("🔍 Testing get_time request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "time_1";
    request["method"] = "get_time";
    request["params"] = nlohmann::json::object();
    
    TransportMessage msg;
    msg.id = "time_1";
    msg.content = request.dump();  // content已经是JSON-RPC格式，不需要设置method和path
    msg.timestamp = std::chrono::system_clock::now();
    
    auto response = transport.sendRequestSync(msg);
    
    if (response.status_code == 200) {
        logger->info("✅ Time response: " + response.content);
    } else {
        logger->error("❌ Get time failed: " + response.error_message);
    }
}

void testNotification(TransportBase& transport) {
    auto logger = mcp_logger::getModuleLogger("stdio_client");
    logger->info("🔍 Testing notification...");
    
    nlohmann::json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "log";
    
    nlohmann::json params;
    params["message"] = "This is a test log message from client";
    notification["params"] = params;
    
    bool success = transport.sendJsonRpcMessage(notification.dump());
    if (success) {
        logger->info("✅ Notification sent successfully");
    } else {
        logger->error("❌ Failed to send notification");
    }
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/stdio_client_example.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] [{module}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    auto logger = mcp_logger::getModuleLogger("stdio_client");
    logger->info("🚀 Starting StdioTransport Client Example");
    
    // 获取服务器程序路径（默认使用当前目录下的stdio_server_example）
    std::string server_path = "./bin/stdio_server_example";
    if (argc > 1) {
        server_path = argv[1];
    }
    
    logger->info("📡 Connecting to server: " + server_path);
    
    // 创建传输实例（使用工厂类）
    std::unique_ptr<TransportBase> transport = TransportFactory::createStdioTransport();
    
    // 配置为客户端模式
    TransportConfig config;
    config.type = TransportType::IPC;
    config.mode = TransportMode::CLIENT;
    
    // IPC配置：指定服务器程序路径
    config.ipc.command = server_path;
    config.ipc.args = {};  // 可以添加命令行参数
    config.ipc.buffer_size = 4096;
    config.ipc.read_timeout_ms = 1000;
    config.ipc.max_retry_count = 3;
    config.ipc.enable_logging = true;
    
    // 设置消息回调
    transport->setMessageCallback([](const TransportMessage& message) {
        auto logger = mcp_logger::getModuleLogger("stdio_client");
        logger->info("📨 Received message: " + message.content);
    });
    
    // 设置连接回调
    transport->setConnectionCallback([](const mcp_transport::ConnectionInfo& info) {
        auto logger = mcp_logger::getModuleLogger("stdio_client");
        logger->info("🔗 Connection status: " + std::to_string(static_cast<int>(info.status)));
        if (info.status == TransportStatus::CONNECTED) {
            logger->info("✅ Connected to server");
        } else if (info.status == TransportStatus::DISCONNECTED) {
            logger->info("❌ Disconnected from server");
        }
    });
    
    // 设置错误回调
    transport->setErrorCallback([](const std::string& error) {
        auto logger = mcp_logger::getModuleLogger("stdio_client");
        logger->error("❌ Transport error: " + error);
    });
    
    // 启动客户端
    if (!transport->start(config)) {
        logger->error("❌ Failed to start StdioTransport client");
        logger->error("💡 Make sure the server program exists at: " + server_path);
        return 1;
    }
    
    logger->info("✅ StdioTransport Client started successfully");
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (!transport->isRunning()) {
        logger->error("❌ Client is not running");
        return 1;
    }
    
    // 执行测试请求
    logger->info("🧪 Starting test requests...");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testPingRequest(*transport);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testEchoRequest(*transport);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testAddRequest(*transport);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testGetTimeRequest(*transport);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testNotification(*transport);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    logger->info("✅ All tests completed");
    
    // 停止客户端
    logger->info("🛑 Stopping client...");
    transport->stop();
    
    logger->info("👋 Client stopped");
    return 0;
}

