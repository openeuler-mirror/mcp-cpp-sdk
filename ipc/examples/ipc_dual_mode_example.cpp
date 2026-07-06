#include "pipe_communication.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>

using namespace mcp;

// 全局变量用于信号处理
std::atomic<bool> g_running{true};

// 信号处理函数
void signalHandler(int signal) {
    static std::atomic<bool> signal_received{false};
    
    if (signal == SIGINT || signal == SIGTERM) {
        if (!signal_received.exchange(true)) {
            g_running = false;
            mcp_logger::getModuleLogger("ipc_dual")->info("🛑 Received signal " + std::to_string(signal) + ", shutting down...");
        }
    }
}

// 服务端请求处理器
class DualModeServerHandler : public ServerRequestHandler {
public:
    std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) override {
        mcp_logger::getModuleLogger("ipc_dual")->info("🔧 Server handling request: " + method + " (ID: " + request_id + ")");
        
        if (method == "status") {
            return "{\"status\": \"running\", \"mode\": \"dual\", \"timestamp\": " + std::to_string(std::time(nullptr)) + "}";
        }
        else if (method == "info") {
            return "{\"name\": \"Dual Mode IPC\", \"version\": \"1.0.0\", \"capabilities\": [\"client\", \"server\"]}";
        }
        else if (method == "calculate") {
            try {
                auto json_params = nlohmann::json::parse(params);
                std::string operation = json_params["operation"].get<std::string>();
                double a = json_params["a"].get<double>();
                double b = json_params["b"].get<double>();
                
                double result = 0;
                if (operation == "add") {
                    result = a + b;
                } else if (operation == "subtract") {
                    result = a - b;
                } else if (operation == "multiply") {
                    result = a * b;
                } else if (operation == "divide") {
                    if (b != 0) {
                        result = a / b;
                    } else {
                        return "{\"error\": \"Division by zero\"}";
                    }
                } else {
                    return "{\"error\": \"Unknown operation: " + operation + "\"}";
                }
                
                return "{\"result\": " + std::to_string(result) + "}";
            } catch (const std::exception& e) {
                return "{\"error\": \"Invalid parameters: " + std::string(e.what()) + "\"}";
            }
        }
        else {
            return "{\"error\": \"Unknown method: " + method + "\"}";
        }
    }
    
    void handleNotification(const std::string& method, const std::string& params) override {
        mcp_logger::getModuleLogger("ipc_dual")->info("📢 Server handling notification: " + method);
        
        if (method == "log") {
            try {
                auto json_params = nlohmann::json::parse(params);
                std::string message = json_params["message"].get<std::string>();
                mcp_logger::getModuleLogger("ipc_dual")->info("📝 Client log: " + message);
            } catch (const std::exception& e) {
                mcp_logger::getModuleLogger("ipc_dual")->error("❌ Invalid log notification: " + std::string(e.what()));
            }
        }
    }
};

// 客户端消息处理器
class DualModeClientHandler : public MessageHandler {
public:
    void onMessage(const Message& message) override {
        mcp_logger::getModuleLogger("ipc_dual")->info("📨 Client received: " + message.content);
    }
    
    void onError(const std::string& error) override {
        mcp_logger::getModuleLogger("ipc_dual")->error("❌ Client error: " + error);
    }
};

void runServerMode() {
    mcp_logger::getModuleLogger("ipc_dual")->info("🚀 Starting Server Mode");
    
    PipeCommunication server_comm;
    
    // 配置服务端
    PipeConfig server_config;
    server_config.mode = CommunicationMode::SERVER;
    server_config.server_name = "dual_mode_server";
    server_config.server_version = "1.0.0";
    server_config.enable_logging = true;
    
    // 设置服务端处理器
    auto server_handler = std::make_shared<DualModeServerHandler>();
    server_comm.setServerRequestHandler(server_handler);
    
    // 启动服务端
    if (!server_comm.start(server_config)) {
        mcp_logger::getModuleLogger("ipc_dual")->error("❌ Failed to start server mode");
        return;
    }
    
    mcp_logger::getModuleLogger("ipc_dual")->info("✅ Server mode started successfully");
    
    // 定期发送状态通知
    std::thread status_thread([&server_comm]() {
        int counter = 0;
        while (g_running && server_comm.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            if (g_running && server_comm.isRunning()) {
                nlohmann::json status;
                status["message"] = "Server status update #" + std::to_string(++counter);
                status["timestamp"] = std::time(nullptr);
                status["uptime"] = counter * 10;
                
                server_comm.sendNotification("status_update", status.dump());
                mcp_logger::getModuleLogger("ipc_dual")->info("📊 Sent status update #" + std::to_string(counter));
            }
        }
        mcp_logger::getModuleLogger("ipc_dual")->info("📊 Status thread finished");
    });
    
    // 主循环
    while (g_running && server_comm.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        if (server_comm.hasMessage()) {
            auto message = server_comm.getMessage();
            mcp_logger::getModuleLogger("ipc_dual")->debug("Server received: " + message.content);
        }
    }
    
    // 等待状态线程结束
    if (status_thread.joinable()) {
        mcp_logger::getModuleLogger("ipc_dual")->info("🔄 Waiting for status thread to finish...");
        status_thread.join();
        mcp_logger::getModuleLogger("ipc_dual")->info("✅ Status thread joined");
    }
    
    // 停止服务端
    mcp_logger::getModuleLogger("ipc_dual")->info("🛑 Stopping server mode...");
    server_comm.stop();
    mcp_logger::getModuleLogger("ipc_dual")->info("✅ Server mode stopped");
}

void runClientMode() {
    mcp_logger::getModuleLogger("ipc_dual")->info("🚀 Starting Client Mode");
    
    PipeCommunication client_comm;
    
    // 设置客户端处理器
    auto client_handler = std::make_shared<DualModeClientHandler>();
    client_comm.setMessageHandler(client_handler);
    
    // 配置客户端
    PipeConfig client_config;
    client_config.mode = CommunicationMode::CLIENT;
    client_config.command = "./ipc_server_example";  // 启动另一个实例作为服务端
    //client_config.args = {"--server"};
    client_config.env_vars = {};
    client_config.enable_logging = true;
    
    // 启动客户端
    if (!client_comm.start(client_config)) {
        mcp_logger::getModuleLogger("ipc_dual")->error("❌ Failed to start client mode");
        return;
    }
    
    mcp_logger::getModuleLogger("ipc_dual")->info("✅ Client mode started successfully");
    
    // 等待服务端启动
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 执行测试请求
    try {
        // 测试状态请求
        {
            nlohmann::json request;
            request["jsonrpc"] = "2.0";
            request["id"] = "status_1";
            request["method"] = "status";
            request["params"] = nlohmann::json::object();
            
            auto future = client_comm.sendRequest(request.dump(), 10);
            auto response = future.get();
            mcp_logger::getModuleLogger("ipc_dual")->info("📨 Status response: " + response);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 测试计算请求
        {
            nlohmann::json request;
            request["jsonrpc"] = "2.0";
            request["id"] = "calc_1";
            request["method"] = "calculate";
            
            nlohmann::json params;
            params["operation"] = "multiply";
            params["a"] = 15.5;
            params["b"] = 4.2;
            request["params"] = params;
            
            auto future = client_comm.sendRequest(request.dump(), 10);
            auto response = future.get();
            mcp_logger::getModuleLogger("ipc_dual")->info("📨 Calculate response: " + response);
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 发送通知
        {
            nlohmann::json notification;
            notification["jsonrpc"] = "2.0";
            notification["method"] = "log";
            
            nlohmann::json params;
            params["message"] = "Hello from dual mode client!";
            notification["params"] = params;
            
            client_comm.sendMessage(notification.dump());
            mcp_logger::getModuleLogger("ipc_dual")->info("📤 Sent notification");
        }
        
        // 监听响应
        mcp_logger::getModuleLogger("ipc_dual")->info("👂 Listening for responses for 20 seconds...");
        auto start_time = std::chrono::steady_clock::now();
        while (g_running && std::chrono::steady_clock::now() - start_time < std::chrono::seconds(20)) {
            if (client_comm.hasMessage()) {
                auto message = client_comm.getMessage();
                mcp_logger::getModuleLogger("ipc_dual")->info("📨 Client received: " + message.content);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc_dual")->error("❌ Client test error: " + std::string(e.what()));
    }
    
    // 停止客户端
    mcp_logger::getModuleLogger("ipc_dual")->info("🛑 Stopping client mode...");
    client_comm.stop();
    mcp_logger::getModuleLogger("ipc_dual")->info("✅ Client mode stopped");
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    //log_config.target = mcp_logger::LogTarget::FILE;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/ipc_dual_mode_example.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    mcp_logger::getModuleLogger("ipc_dual")->info("🚀 Starting Dual Mode IPC Example");
    
    // 检查命令行参数
    bool server_mode = false;
    if (argc > 1 && std::string(argv[1]) == "--server") {
        server_mode = true;
    }
    
    if (server_mode) {
        // 运行服务端模式
        runServerMode();
    } else {
        // 运行客户端模式
        runClientMode();
    }
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}
