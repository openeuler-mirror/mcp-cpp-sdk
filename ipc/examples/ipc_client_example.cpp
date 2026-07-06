#include "pipe_communication.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace mcp;

// 示例消息处理器
class ExampleMessageHandler : public MessageHandler {
public:
    void onMessage(const Message& message) override {
        mcp_logger::getModuleLogger("ipc_client")->info("Received notification: " + message.content);
    }
    
    void onError(const std::string& error) override {
        mcp_logger::getModuleLogger("ipc_client")->error("Error: " + error);
    }
};

void testPingRequest(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_client")->info("🔍 Testing ping request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "ping_1";
    request["method"] = "ping";
    request["params"] = nlohmann::json::object();
    
    auto future = pipe_comm.sendRequest(request.dump(), 10);
    auto response = future.get();
    
    mcp_logger::getModuleLogger("ipc_client")->info("📨 Ping response: " + response);
}

void testEchoRequest(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_client")->info("🔍 Testing echo request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "echo_1";
    request["method"] = "echo";
    request["params"] = "Hello from client!";
    
    auto future = pipe_comm.sendRequest(request.dump(), 10);
    auto response = future.get();
    
    mcp_logger::getModuleLogger("ipc_client")->info("📨 Echo response: " + response);
}

void testAddRequest(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_client")->info("🔍 Testing add request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "add_1";
    request["method"] = "add";
    
    nlohmann::json params;
    params["a"] = 42;
    params["b"] = 58;
    request["params"] = params;
    
    auto future = pipe_comm.sendRequest(request.dump(), 10);
    auto response = future.get();
    
    mcp_logger::getModuleLogger("ipc_client")->info("📨 Add response: " + response);
}

void testGetTimeRequest(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_client")->info("🔍 Testing get_time request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "time_1";
    request["method"] = "get_time";
    request["params"] = nlohmann::json::object();
    
    auto future = pipe_comm.sendRequest(request.dump(), 10);
    auto response = future.get();
    
    mcp_logger::getModuleLogger("ipc_client")->info("📨 Time response: " + response);
}

void testNotification(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_client")->info("🔍 Testing notification...");
    
    nlohmann::json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "log";
    
    nlohmann::json params;
    params["message"] = "This is a test log message from client";
    notification["params"] = params;
    
    bool success = pipe_comm.sendMessage(notification.dump());
    if (success) {
        mcp_logger::getModuleLogger("ipc_client")->info("✅ Notification sent successfully");
    } else {
        mcp_logger::getModuleLogger("ipc_client")->error("❌ Failed to send notification");
    }
}

void testBatchRequests(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_client")->info("🔍 Testing batch requests...");
    
    std::vector<std::future<std::string>> futures;
    
    // 发送多个并发请求
    for (int i = 0; i < 5; ++i) {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["id"] = "batch_" + std::to_string(i);
        request["method"] = "ping";
        request["params"] = nlohmann::json::object();
        
        futures.push_back(pipe_comm.sendRequest(request.dump(), 10));
    }
    
    // 等待所有响应
    for (size_t i = 0; i < futures.size(); ++i) {
        auto response = futures[i].get();
        mcp_logger::getModuleLogger("ipc_client")->info("📨 Batch response " + std::to_string(i) + ": " + response);
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/ipc_client_example.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    mcp_logger::getModuleLogger("ipc_client")->info("🚀 Starting IPC Client Example");
    
    // 创建管道通信实例
    PipeCommunication pipe_comm;
    
    // 设置消息处理器
    auto handler = std::make_shared<ExampleMessageHandler>();
    pipe_comm.setMessageHandler(handler);
    
    // 配置为客户端模式
    PipeConfig config;
    config.mode = CommunicationMode::CLIENT;
    config.command = "./bin/ipc_server_example";  // 启动服务端进程
    config.args = {};
    config.env_vars = {};
    config.enable_logging = true;
    config.buffer_size = 4096;
    config.read_timeout_ms = 10;
    
    // 启动客户端
    if (!pipe_comm.start(config)) {
        mcp_logger::getModuleLogger("ipc_client")->error("❌ Failed to start IPC client");
        return 1;
    }
    
    mcp_logger::getModuleLogger("ipc_client")->info("✅ IPC Client started successfully");
    
    // 等待服务端启动
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 执行各种测试
    try {
        testPingRequest(pipe_comm);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testEchoRequest(pipe_comm);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testAddRequest(pipe_comm);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testGetTimeRequest(pipe_comm);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testNotification(pipe_comm);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testBatchRequests(pipe_comm);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 监听通知
        mcp_logger::getModuleLogger("ipc_client")->info("👂 Listening for notifications for 30 seconds...");
        auto start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30)) {
            if (pipe_comm.hasMessage()) {
                auto message = pipe_comm.getMessage();
                mcp_logger::getModuleLogger("ipc_client")->info("📨 Received: " + message.content);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc_client")->error("❌ Test error: " + std::string(e.what()));
    }
    
    // 停止客户端
    mcp_logger::getModuleLogger("ipc_client")->info("🛑 Stopping IPC client...");
    pipe_comm.stop();
    mcp_logger::getModuleLogger("ipc_client")->info("✅ IPC Client stopped");
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}
