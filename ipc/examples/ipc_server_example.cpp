#include "pipe_communication.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>

using namespace mcp;

// 示例服务端请求处理器
class ExampleServerRequestHandler : public ServerRequestHandler {
public:
    std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) override {
        mcp_logger::getModuleLogger("ipc_server")->info("Handling request: " + method + " (ID: " + request_id + ")");
        
        if (method == "ping") {
            return "{\"message\": \"pong\", \"timestamp\": " + std::to_string(std::time(nullptr)) + "}";
        }
        else if (method == "echo") {
            return "{\"echo\": " + params + "}";
        }
        else if (method == "add") {
            try {
                auto json_params = nlohmann::json::parse(params);
                int a = json_params["a"].get<int>();
                int b = json_params["b"].get<int>();
                int result = a + b;
                return "{\"result\": " + std::to_string(result) + "}";
            } catch (const std::exception& e) {
                return "{\"error\": \"Invalid parameters: " + std::string(e.what()) + "\"}";
            }
        }
        else if (method == "get_time") {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string time_str = std::ctime(&time_t);
            // Remove the trailing newline character from ctime()
            if (!time_str.empty() && time_str.back() == '\n') {
                time_str.pop_back();
            }
            return "{\"time\": \"" + time_str + "\"}";
        }
        else {
            return "{\"error\": \"Unknown method: " + method + "\"}";
        }
    }
    
    void handleNotification(const std::string& method, const std::string& params) override {
        mcp_logger::getModuleLogger("ipc_server")->info("Handling notification: " + method);
        
        if (method == "log") {
            try {
                auto json_params = nlohmann::json::parse(params);
                std::string message = json_params["message"].get<std::string>();
                mcp_logger::getModuleLogger("ipc_server")->info("Client log: " + message);
            } catch (const std::exception& e) {
                mcp_logger::getModuleLogger("ipc_server")->error("Invalid log notification: " + std::string(e.what()));
            }
        }
    }
};

int main() {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::FILE;  // 只输出到文件，不输出到控制台
    log_config.file_path = "logs/ipc_server_example.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    mcp_logger::getModuleLogger("ipc_server")->info("🚀 Starting IPC Server Example");
    
    // 创建管道通信实例
    PipeCommunication pipe_comm;
    
    // 配置为服务端模式
    PipeConfig config;
    config.mode = CommunicationMode::SERVER;
    config.server_name = "example_server";
    config.server_version = "1.0.0";
    config.enable_logging = true;
    
    // 设置服务端请求处理器
    auto handler = std::make_shared<ExampleServerRequestHandler>();
    pipe_comm.setServerRequestHandler(handler);
    
    // 启动服务端
    if (!pipe_comm.start(config)) {
        mcp_logger::getModuleLogger("ipc_server")->error("❌ Failed to start IPC server");
        return 1;
    }
    
    mcp_logger::getModuleLogger("ipc_server")->info("✅ IPC Server started successfully");
    mcp_logger::getModuleLogger("ipc_server")->info("📡 Server is listening for requests...");
    
    // 阻塞等待用户输入，打印启动日志
    mcp_logger::getModuleLogger("ipc_server")->info("server stdio start ....");
    //std::cout << "Server is ready. Press Enter to continue or Ctrl+C to exit..." << std::endl;
    //std::cin.get();  // 阻塞等待用户按回车键
    
    // 定期发送心跳通知
    std::thread heartbeat_thread([&pipe_comm]() {
        int counter = 0;
        while (pipe_comm.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (pipe_comm.isRunning()) {
                nlohmann::json heartbeat;
                heartbeat["message"] = "Server heartbeat #" + std::to_string(++counter);
                heartbeat["timestamp"] = std::time(nullptr);
                
                pipe_comm.sendNotification("heartbeat", heartbeat.dump());
                mcp_logger::getModuleLogger("ipc_server")->info("💓 Sent heartbeat #" + std::to_string(counter));
            }
        }
    });
    
    // 主循环 - 保持服务运行
    try {
        while (pipe_comm.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 检查是否有消息需要处理
            if (pipe_comm.hasMessage()) {
                auto message = pipe_comm.getMessage();
                mcp_logger::getModuleLogger("ipc_server")->debug("Received message: " + message.content);
            }
        }
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc_server")->error("❌ Server error: " + std::string(e.what()));
    }
    
    // 等待心跳线程结束
    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }
    
    // 停止服务
    mcp_logger::getModuleLogger("ipc_server")->info("🛑 Stopping IPC server...");
    pipe_comm.stop();
    mcp_logger::getModuleLogger("ipc_server")->info("✅ IPC Server stopped");
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}
