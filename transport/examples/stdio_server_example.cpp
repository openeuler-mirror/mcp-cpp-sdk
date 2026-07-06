#include "../include/stdio_transport.h"
#include "../include/transport_factory.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>

using namespace mcp_transport;

// 示例服务端请求处理器
class ExampleServerRequestHandler : public TransportServerRequestHandler {
public:
    std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) override {
        auto logger = mcp_logger::getModuleLogger("stdio_server");
        logger->info("📥 Handling request: method=" + method + ", id=" + request_id);
        
        if (method == "ping") {
            nlohmann::json result;
            result["message"] = "pong";
            result["timestamp"] = std::time(nullptr);
            logger->info("✅ Ping request handled");
            return result.dump();
        }
        else if (method == "echo") {
            nlohmann::json result;
            try {
                auto json_params = nlohmann::json::parse(params);
                result["echo"] = json_params;
            } catch (...) {
                result["echo"] = params;
            }
            logger->info("✅ Echo request handled");
            return result.dump();
        }
        else if (method == "add") {
            try {
                auto json_params = nlohmann::json::parse(params);
                int a = json_params["a"].get<int>();
                int b = json_params["b"].get<int>();
                int sum = a + b;
                nlohmann::json result;
                result["result"] = sum;
                logger->info("✅ Add request handled: " + std::to_string(a) + " + " + std::to_string(b) + " = " + std::to_string(sum));
                return result.dump();
            } catch (const std::exception& e) {
                nlohmann::json error;
                error["error"] = "Invalid parameters: " + std::string(e.what());
                logger->error("❌ Add request error: " + std::string(e.what()));
                return error.dump();
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
            nlohmann::json result;
            result["time"] = time_str;
            logger->info("✅ Get time request handled");
            return result.dump();
        }
        else {
            nlohmann::json error;
            error["error"] = "Unknown method: " + method;
            logger->warning("⚠️ Unknown method: " + method);
            return error.dump();
        }
    }
    
    void handleNotification(const std::string& method, const std::string& params) override {
        auto logger = mcp_logger::getModuleLogger("stdio_server");
        logger->info("📢 Handling notification: method=" + method);
        
        if (method == "log") {
            try {
                auto json_params = nlohmann::json::parse(params);
                std::string message = json_params["message"].get<std::string>();
                logger->info("📝 Client log: " + message);
            } catch (const std::exception& e) {
                logger->error("❌ Invalid log notification: " + std::string(e.what()));
            }
        }
    }
};

// 全局变量用于信号处理
std::unique_ptr<StdioTransport> g_transport = nullptr;

void signalHandler(int signal) {
    if (g_transport) {
        auto logger = mcp_logger::getModuleLogger("stdio_server");
        logger->info("🛑 Received signal " + std::to_string(signal) + ", shutting down...");
        g_transport->stop();
    }
    exit(0);
}

int main() {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/stdio_server_example.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] [{module}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    auto logger = mcp_logger::getModuleLogger("stdio_server");
    logger->info("🚀 Starting StdioTransport Server Example");
    
    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 创建传输实例（使用工厂类）
    auto transport = TransportFactory::createStdioTransport();
    g_transport = std::move(transport);
    
    // 配置为服务端模式
    TransportConfig config;
    config.type = TransportType::IPC;
    config.mode = TransportMode::SERVER;
    config.server.server_name = "stdio_example_server";
    config.server.server_version = "1.0.0";
    config.server.description = "StdioTransport Example Server";
    
    // IPC配置（服务器模式下command为空）
    config.ipc.command = "";  // 空命令表示当前进程就是服务器
    config.ipc.buffer_size = 4096;
    config.ipc.read_timeout_ms = 100;
    config.ipc.max_retry_count = 10;
    config.ipc.enable_logging = true;
    
    // 设置服务端请求处理器
    auto handler = std::make_shared<ExampleServerRequestHandler>();
    g_transport->setServerRequestHandler(handler);
    
    // 设置连接回调
    g_transport->setConnectionCallback([](const mcp_transport::ConnectionInfo& info) {
        auto logger = mcp_logger::getModuleLogger("stdio_server");
        logger->info("🔗 Connection status changed: " + std::to_string(static_cast<int>(info.status)));
    });
    
    // 设置错误回调
    g_transport->setErrorCallback([](const std::string& error) {
        auto logger = mcp_logger::getModuleLogger("stdio_server");
        logger->error("❌ Transport error: " + error);
    });
    
    // 启动服务器
    if (!g_transport->start(config)) {
        logger->error("❌ Failed to start StdioTransport server");
        return 1;
    }
    
    logger->info("✅ StdioTransport Server started successfully");
    logger->info("📡 Server is listening for requests on stdin...");
    logger->info("💡 Send requests via stdin, responses will be sent to stdout");
    logger->info("🛑 Press Ctrl+C to stop the server");
    
    // 保持运行，等待请求
    while (g_transport->isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 检查是否有消息
        if (g_transport->hasMessage()) {
            auto msg = g_transport->getMessage();
            logger->info("📨 Received message: " + msg.content);
        }
    }
    
    logger->info("👋 Server stopped");
    return 0;
}

