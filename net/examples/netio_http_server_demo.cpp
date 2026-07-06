/**
 * @file netio_http_server_demo.cpp
 * @brief 使用NetIO创建HTTP服务器的demo
 * 
 * 这个文件演示了如何使用NetIO库创建一个HTTP服务器。
 * 服务器监听指定端口，处理JSON-RPC请求。
 */

#include "netio.h"
#include "../../log/include/logger.h"
#include "../../common/json.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <signal.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <atomic>

using namespace netio;
using json = nlohmann::json;

// 全局变量用于优雅关闭
static std::unique_ptr<NetIO> g_netio = nullptr;
static std::atomic<bool> g_running(true);

// 信号处理函数
void signalHandler(int signal) {
    // 在信号处理函数中只设置标志，不调用可能持有锁的函数
    auto logger = mcp_logger::getModuleLogger("netio_http_server");
    logger->info("🛑 Received signal " + std::to_string(signal) + ", shutting down...");
    g_running = false;
}

// 生成 session_id 的辅助函数
std::string generateSessionId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    
    std::ostringstream oss;
    oss << std::hex;
    for (int i = 0; i < 32; ++i) {
        oss << dis(gen);
    }
    return oss.str();
}

// 自定义服务器请求处理器
class DemoServerRequestHandler : public ServerRequestHandler {
public:
    bool handleRequest(const Message& request, Message& response) override {
        auto logger = mcp_logger::getModuleLogger("netio_http_server");
        
        // 初始化响应
        response.type = MessageType::RESPONSE;
        response.id = request.id;
        response.timestamp = std::chrono::system_clock::now();
        response.headers["Content-Type"] = "application/json";
        response.headers["X-Server"] = "NetIO-HTTP-Server/1.0";
        response.status_code = 200;
        
        logger->info("📥 Received request: method=" + request.method + ", path=" + request.path);
        
        // 解析JSON-RPC请求
        json request_json;
        try {
            if (!request.body.empty()) {
                request_json = json::parse(request.body);
                logger->info("📦 Request body: " + request.body);
            } else {
                logger->warning("⚠️ Empty request body");
                response.status_code = 400;
                response.body = R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request: empty body"}})";
                return true;
            }
        } catch (const std::exception& e) {
            logger->error("❌ Invalid JSON: " + std::string(e.what()));
            response.status_code = 400;
            response.body = R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"}})";
            return true;
        }
        
        // 检查是否是通知（无ID）
        bool is_notification = !request_json.contains("id") || request_json["id"].is_null();
        
        if (is_notification) {
            // 处理通知
            std::string method = request_json.value("method", "");
            json params = request_json.value("params", json::object());
            
            logger->info("📢 Handling notification: " + method);
            handleNotification(method, params);
            
            // 通知不需要响应
            response.status_code = 204;
            response.body.clear();
            return false;
        }
        
        // 处理请求
        std::string method = request_json.value("method", "");
        json params = request_json.value("params", json::object());
        std::string request_id = request_json["id"].is_string() 
            ? request_json["id"].get<std::string>() 
            : request_json["id"].dump();
        
        logger->info("🔍 Handling request: method=" + method + ", id=" + request_id);
        
        json response_json;
        response_json["jsonrpc"] = "2.0";
        response_json["id"] = request_json["id"];
        
        if (method == "ping") {
            json result;
            result["message"] = "pong";
            result["timestamp"] = std::time(nullptr);
            response_json["result"] = result;
            logger->info("✅ Ping request handled");
        }
        else if (method == "echo") {
            response_json["result"] = params;
            logger->info("✅ Echo request handled");
        }
        else if (method == "add") {
            try {
                int a = params["a"].get<int>();
                int b = params["b"].get<int>();
                int sum = a + b;
                json result;
                result["result"] = sum;
                response_json["result"] = result;
                logger->info("✅ Add request handled: " + std::to_string(a) + " + " + std::to_string(b) + " = " + std::to_string(sum));
            } catch (const std::exception& e) {
                response_json["error"] = {
                    {"code", -32602},
                    {"message", "Invalid parameters: " + std::string(e.what())}
                };
                response.status_code = 400;
                logger->error("❌ Add request error: " + std::string(e.what()));
            }
        }
        else if (method == "get_time") {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::string time_str = std::ctime(&time_t);
            if (!time_str.empty() && time_str.back() == '\n') {
                time_str.pop_back();
            }
            json result;
            result["time"] = time_str;
            response_json["result"] = result;
            logger->info("✅ Get time request handled");
        }
        else {
            response_json["error"] = {
                {"code", -32601},
                {"message", "Method not found: " + method}
            };
            response.status_code = 404;
            logger->warning("⚠️ Unknown method: " + method);
        }
        
        response.body = response_json.dump();
        return true;
    }
    
private:
    void handleNotification(const std::string& method, const json& params) {
        auto logger = mcp_logger::getModuleLogger("netio_http_server");
        
        if (method == "log") {
            try {
                std::string message = params["message"].get<std::string>();
                logger->info("📝 Client log: " + message);
            } catch (const std::exception& e) {
                logger->error("❌ Invalid log notification: " + std::string(e.what()));
            }
        }
    }
};

// 自定义连接请求处理器（处理GET请求）
class DemoConnectionRequestHandler : public ConnectionRequestHandler {
public:
    bool handleRequest(const Message& request, Message& response) override {
        auto logger = mcp_logger::getModuleLogger("netio_http_server");
        logger->info("🔌 Handling connection request: method=" + request.method + ", path=" + request.path);
        
        // 生成 session_id
        std::string session_id = generateSessionId();
        
        // 初始化响应
        response = Message{};
        response.type = MessageType::RESPONSE;
        response.id = request.id;
        response.timestamp = std::chrono::system_clock::now();
        response.status_code = 200;
        response.headers["Content-Type"] = "application/json";
        response.headers["Cache-Control"] = "no-cache";
        response.headers["Connection"] = "keep-alive";
        response.headers["X-Session-Id"] = session_id;  // 添加 session_id 到响应头
        response.body = R"({"jsonrpc":"2.0","result":{"status":"ready"}})";
        
        logger->info("✅ Connection accepted for path: " + request.path + ", session_id: " + session_id);
        return true;
    }
};

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/netio_http_server_demo.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] [{module}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    auto logger = mcp_logger::getModuleLogger("netio_http_server");
    logger->info("🚀 Starting NetIO HTTP Server Example");
    
    // 解析命令行参数
    std::string host = "0.0.0.0";
    int port = 10080;
    std::string endpoint = "/mcp";
    
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }
    if (argc > 3) {
        endpoint = argv[3];
    }
    
    logger->info("📡 Server configuration: host=" + host + ", port=" + std::to_string(port) + ", endpoint=" + endpoint);
    
    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    try {
        // 创建NetIO实例
        g_netio = std::make_unique<NetIO>();
        
        // 配置服务器
        NetConfig config;
        config.host = host;
        config.port = port;
        config.protocol = Protocol::HTTP;
        config.timeout_seconds = 30;
        config.endpoint = endpoint;
        config.keep_alive = true;
        
        // 设置请求头
        config.headers = {
            {"Server", "NetIO-HTTP-Server/1.0"}
        };
        
        // 设置请求处理器
        auto server_handler = std::make_shared<DemoServerRequestHandler>();
        g_netio->setServerRequestHandler(server_handler);
        
        auto connection_handler = std::make_shared<DemoConnectionRequestHandler>();
        g_netio->setConnectionRequestHandler(connection_handler);
        
        // 设置回调函数
        g_netio->setMessageCallback([](const Message& message) {
            auto logger = mcp_logger::getModuleLogger("netio_http_server");
            logger->debug("📨 Message callback: " + message.id);
        });
        
        g_netio->setConnectionCallback([](const ConnectionInfo& info) {
            auto logger = mcp_logger::getModuleLogger("netio_http_server");
            logger->info("🔗 Connection callback: " + info.connection_id);
            if (info.status == ConnectionStatus::CONNECTED) {
                logger->info("✅ Server started on " + info.local_address + 
                            ":" + std::to_string(info.local_port));
            }
        });
        
        g_netio->setErrorCallback([](const std::string& error) {
            auto logger = mcp_logger::getModuleLogger("netio_http_server");
            logger->error("❌ Error callback: " + error);
        });
        
        // 启动服务器
        logger->info("⏳ Starting server...");
        if (!g_netio->startServer(config)) {
            logger->error("❌ Failed to start server");
            g_netio.reset();
            return 1;
        }
        
        // 等待服务器启动 - 增加等待时间和重试机制
        logger->info("⏳ Waiting for server to start...");
        int wait_count = 0;
        const int max_wait = 100; // 最多等待10秒（因为 wait_until_ready 已经在 startServer 中调用）
        bool server_started = false;
        
        // 给服务器一点额外时间完成启动
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        while (wait_count < max_wait) {
            if (g_netio && g_netio->isServerRunning()) {
                server_started = true;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        if (!server_started) {
            logger->error("❌ Server failed to start after waiting");
            logger->error("Possible reasons:");
            logger->error("  1. Port " + std::to_string(port) + " is already in use");
            logger->error("  2. Permission denied, cannot bind to port");
            logger->error("  3. Network configuration issue");
            logger->error("  4. Server thread encountered an error");
            // 等待一小段时间，让服务器线程完成日志输出
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // 确保清理资源
            if (g_netio) {
                try {
                    if (g_netio->isServerRunning()) {
                        g_netio->stopServer();
                    }
                } catch (...) {
                    // 忽略停止服务器时的异常
                }
                g_netio.reset();
            }
            return 1;
        }
        
        logger->info("✅ NetIO HTTP Server started successfully");
        logger->info("📡 Server is listening on http://" + host + ":" + std::to_string(port) + endpoint);
        logger->info("🛑 Press Ctrl+C to stop the server");
        logger->info("📋 Supported JSON-RPC methods: ping, echo, add, get_time");
        logger->info("📋 Supported notifications: log");
        
        // 保持运行，等待请求
        while (g_running && g_netio->isServerRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 定期显示统计信息
            static int counter = 0;
            if (++counter % 100 == 0) {  // 每10秒显示一次
                auto stats = g_netio->getStatistics();
                logger->info("📊 Statistics: " + stats.dump());
            }
        }
        
        // 优雅关闭服务器
        if (g_netio) {
            try {
                if (g_netio->isServerRunning()) {
                    logger->info("⏳ Stopping server...");
                    g_netio->stopServer();
                }
            } catch (const std::exception& e) {
                logger->error("❌ Error stopping server: " + std::string(e.what()));
            } catch (...) {
                logger->error("❌ Unknown error stopping server");
            }
            // 重置指针，触发析构
            g_netio.reset();
        }
        
        logger->info("👋 Server stopped");
        
    } catch (const std::exception& e) {
        auto logger = mcp_logger::getModuleLogger("netio_http_server");
        logger->error("❌ Error: " + std::string(e.what()));
        // 确保清理资源
        if (g_netio) {
            try {
                g_netio->stopServer();
            } catch (...) {
                // 忽略停止服务器时的异常
            }
            g_netio.reset();
        }
        return 1;
    } catch (...) {
        auto logger = mcp_logger::getModuleLogger("netio_http_server");
        logger->error("❌ Unknown error occurred");
        // 确保清理资源
        if (g_netio) {
            try {
                g_netio->stopServer();
            } catch (...) {
                // 忽略停止服务器时的异常
            }
            g_netio.reset();
        }
        return 1;
    }
    
    return 0;
}

