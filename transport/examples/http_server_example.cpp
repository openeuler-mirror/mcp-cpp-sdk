#include "../include/http_transport.h"
#include "../include/transport_factory.h"
#include "../../log/include/logger.h"
#include "../../net/include/netio.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <random>
#include <sstream>
#include <iomanip>

using namespace mcp_transport;
using namespace netio;

// 示例SSE服务端请求处理器
class ExampleSseServerRequestHandler : public SseServerRequestHandler {
public:
    bool handleRequest(const netio::Message& request, netio::Message& response) override {
        auto logger = mcp_logger::getModuleLogger("http_server");
        
        // 初始化响应
        response.type = netio::MessageType::RESPONSE;
        response.id = request.id;
        response.timestamp = std::chrono::system_clock::now();
        response.headers["Content-Type"] = "application/json";
        response.status_code = 200;
        
        logger->info("📥 Received request: method=" + request.method + ", path=" + request.path);
        
        // 解析JSON-RPC请求
        nlohmann::json request_json;
        try {
            if (!request.body.empty()) {
                request_json = nlohmann::json::parse(request.body);
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
            nlohmann::json params = request_json.value("params", nlohmann::json::object());
            
            logger->info("📢 Handling notification: " + method);
            handleNotification(method, params);
            
            // 通知不需要响应
            response.status_code = 204;
            response.body.clear();
            return false;
        }
        
        // 处理请求
        std::string method = request_json.value("method", "");
        nlohmann::json params = request_json.value("params", nlohmann::json::object());
        std::string request_id = request_json["id"].is_string() 
            ? request_json["id"].get<std::string>() 
            : request_json["id"].dump();
        
        logger->info("🔍 Handling request: method=" + method + ", id=" + request_id);
        
        nlohmann::json response_json;
        response_json["jsonrpc"] = "2.0";
        response_json["id"] = request_json["id"];
        
        if (method == "ping") {
            nlohmann::json result;
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
                nlohmann::json result;
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
            nlohmann::json result;
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
    void handleNotification(const std::string& method, const nlohmann::json& params) {
        auto logger = mcp_logger::getModuleLogger("http_server");
        
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

// 示例SSE连接请求处理器
class ExampleSseConnectHandler : public SseServerConnectHandler {
public:
    bool handleRequest(const netio::Message& request, netio::Message& response) override {
        auto logger = mcp_logger::getModuleLogger("http_server");
        logger->info("🔌 Handling SSE connection request: method=" + request.method + ", path=" + request.path);
        
        // 生成 session_id
        std::string session_id = generateSessionId();
        
        // 初始化响应
        response = netio::Message{};
        response.type = netio::MessageType::RESPONSE;
        response.id = request.id;
        response.timestamp = std::chrono::system_clock::now();
        response.status_code = 200;
        response.headers["Content-Type"] = "application/json";
        response.headers["Cache-Control"] = "no-cache";
        response.headers["Connection"] = "keep-alive";
        response.headers["X-Session-Id"] = session_id;  // 添加 session_id 到响应头
        response.body = R"({"jsonrpc":"2.0","result":{"status":"ready"}})";
        
        logger->info("✅ SSE connection accepted for path: " + request.path + ", session_id: " + session_id);
        return true;
    }
};

// 全局变量用于信号处理
std::unique_ptr<HttpTransport> g_transport = nullptr;

void signalHandler(int signal) {
    if (g_transport) {
        auto logger = mcp_logger::getModuleLogger("http_server");
        logger->info("🛑 Received signal " + std::to_string(signal) + ", shutting down...");
        g_transport->stop();
    }
    exit(0);
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/http_server_example.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] [{module}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    auto logger = mcp_logger::getModuleLogger("http_server");
    logger->info("🚀 Starting HttpTransport Server Example");
    
    // 解析命令行参数
    std::string host = "localhost";
    int port = 8080;
    std::string endpoint = "/";
    
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
    
    // 创建传输实例（使用工厂类）
    g_transport = TransportFactory::createHttpTransport();
    
    // 配置为服务端模式
    TransportConfig config;
    config.type = TransportType::HTTP;
    config.mode = TransportMode::SERVER;
    config.sse.host = host;
    config.sse.port = port;
    config.sse.protocol = netio::Protocol::HTTP;
    config.sse.endpoint = endpoint;
    config.sse.keep_alive = true;
    
    // 设置SSE服务端请求处理器
    auto handler = std::make_shared<ExampleSseServerRequestHandler>();
    g_transport->setSseServerRequestHandler(handler);
    
    // 设置连接请求处理器
    auto connect_handler = std::make_shared<ExampleSseConnectHandler>();
    g_transport->setConnectionRequestHandler(connect_handler);
    
    // 设置连接回调
    g_transport->setConnectionCallback([](const mcp_transport::ConnectionInfo& info) {
        auto logger = mcp_logger::getModuleLogger("http_server");
        logger->info("🔗 Connection status: " + std::to_string(static_cast<int>(info.status)));
        if (info.status == TransportStatus::CONNECTED) {
            logger->info("✅ Server started on " + info.netio_info.local_address + 
                        ":" + std::to_string(info.netio_info.local_port));
        }
    });
    
    // 设置错误回调
    g_transport->setErrorCallback([](const std::string& error) {
        auto logger = mcp_logger::getModuleLogger("http_server");
        logger->error("❌ Transport error: " + error);
    });
    
    // 启动服务器
    if (!g_transport->start(config)) {
        logger->error("❌ Failed to start HttpTransport server");
        return 1;
    }
    
    logger->info("✅ HttpTransport Server started successfully");
    logger->info("📡 Server is listening on http://" + host + ":" + std::to_string(port) + endpoint);
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

