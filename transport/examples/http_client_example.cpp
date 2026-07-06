#include "../include/http_transport.h"
#include "../include/transport_factory.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <cctype>

using namespace mcp_transport;

// 客户端状态，存储 session_id
struct ClientState {
    std::string session_id;
};

// 辅助函数：将字符串转换为小写
std::string toLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    return result;
}

// 从响应头中提取 session_id
std::string extractSessionId(const std::map<std::string, std::string>& headers) {
    for (const auto& [key, value] : headers) {
        if (toLower(key) == "x-session-id") {
            return value;
        }
    }
    return {};
}

// 发送请求并处理 session_id
TransportMessage sendRequestWithSession(TransportBase& transport, ClientState& state,
                                       const nlohmann::json& request_json,
                                       const std::string& label,
                                       const std::string& endpoint) {
    auto logger = mcp_logger::getModuleLogger("http_client");
    
    // 复制请求以便修改
    nlohmann::json request = request_json;
    
    // 如果有 session_id，添加到请求头和请求 ID
    if (!state.session_id.empty()) {
        request["id"] = state.session_id;
    }
    
    TransportMessage msg;
    // 设置消息 ID：优先使用 session_id，否则使用请求中的 id（如果存在）
    if (!state.session_id.empty()) {
        msg.id = state.session_id;
    } else if (request.contains("id") && !request["id"].is_null()) {
        if (request["id"].is_string()) {
            msg.id = request["id"].get<std::string>();
        } else {
            msg.id = request["id"].dump();
        }
    } else {
        // 如果没有 id，生成一个临时的
        msg.id = label + "_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    msg.content = request.dump();
    msg.method = "POST";
    msg.path = endpoint;  // 使用配置的 endpoint
    msg.headers["Content-Type"] = "application/json";
    
    // 添加 session_id 到请求头
    if (!state.session_id.empty()) {
        msg.headers["X-Session-Id"] = state.session_id;
        logger->info("➡️  Sending " + label + " request with session_id=" + state.session_id);
    } else {
        logger->info("➡️  Sending " + label + " request (no session yet)");
    }
    
    msg.timestamp = std::chrono::system_clock::now();
    
    auto response = transport.sendRequestSync(msg);
    
    // 从响应头中提取 session_id
    const auto new_session_id = extractSessionId(response.headers);
    if (!new_session_id.empty() && new_session_id != state.session_id) {
        logger->info("📦 Updated session id from response header: " + new_session_id);
        state.session_id = new_session_id;
    }
    
    if (response.status_code == 200) {
        logger->info("✅ " + label + " response: " + response.content);
    } else {
        logger->error("❌ " + label + " failed: status=" + std::to_string(response.status_code) + 
                     ", error=" + response.error_message);
    }
    
    return response;
}

void testPingRequest(TransportBase& transport, ClientState& state, const std::string& endpoint) {
    auto logger = mcp_logger::getModuleLogger("http_client");
    logger->info("🔍 Testing ping request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "ping";
    request["params"] = nlohmann::json::object();
    
    sendRequestWithSession(transport, state, request, "ping", endpoint);
}

void testEchoRequest(TransportBase& transport, ClientState& state, const std::string& endpoint) {
    auto logger = mcp_logger::getModuleLogger("http_client");
    logger->info("🔍 Testing echo request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "echo";
    request["params"] = "Hello from HTTP client!";
    
    sendRequestWithSession(transport, state, request, "echo", endpoint);
}

void testAddRequest(TransportBase& transport, ClientState& state, const std::string& endpoint) {
    auto logger = mcp_logger::getModuleLogger("http_client");
    logger->info("🔍 Testing add request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "add";
    
    nlohmann::json params;
    params["a"] = 42;
    params["b"] = 58;
    request["params"] = params;
    
    sendRequestWithSession(transport, state, request, "add", endpoint);
}

void testGetTimeRequest(TransportBase& transport, ClientState& state, const std::string& endpoint) {
    auto logger = mcp_logger::getModuleLogger("http_client");
    logger->info("🔍 Testing get_time request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["method"] = "get_time";
    request["params"] = nlohmann::json::object();
    
    sendRequestWithSession(transport, state, request, "get_time", endpoint);
}

void testNotification(TransportBase& transport, ClientState& state) {
    auto logger = mcp_logger::getModuleLogger("http_client");
    logger->info("🔍 Testing notification...");
    
    nlohmann::json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "log";
    
    nlohmann::json params;
    params["message"] = "This is a test log message from HTTP client";
    notification["params"] = params;
    
    // 如果有 session_id，添加到请求头
    std::string notification_str = notification.dump();
    bool success = transport.sendJsonRpcMessage(notification_str);
    
    if (success) {
        if (!state.session_id.empty()) {
            logger->info("✅ Notification sent successfully with session_id=" + state.session_id);
        } else {
            logger->info("✅ Notification sent successfully");
        }
    } else {
        logger->error("❌ Failed to send notification");
    }
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/http_client_example.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] [{module}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    auto logger = mcp_logger::getModuleLogger("http_client");
    logger->info("🚀 Starting HttpTransport Client Example");
    
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
    
    logger->info("📡 Connecting to server: http://" + host + ":" + std::to_string(port) + endpoint);
    
    // 创建客户端状态
    ClientState client_state;
    
    // 创建传输实例（使用工厂类）
    std::unique_ptr<TransportBase> transport = TransportFactory::createHttpTransport();
    
    // 配置为客户端模式
    TransportConfig config;
    config.type = TransportType::HTTP;
    config.mode = TransportMode::CLIENT;
    config.sse.host = host;
    config.sse.port = port;
    config.sse.protocol = netio::Protocol::HTTP;
    config.sse.endpoint = endpoint;
    config.sse.timeout_seconds = 30;
    config.sse.keep_alive = true;
    config.sse.auto_reconnect = false;
    
    // 设置消息回调
    transport->setMessageCallback([](const TransportMessage& message) {
        auto logger = mcp_logger::getModuleLogger("http_client");
        logger->info("📨 Received message: " + message.content);
    });
    
    // 设置连接回调，从连接信息中获取 session_id
    transport->setConnectionCallback([&client_state](const mcp_transport::ConnectionInfo& info) {
        auto logger = mcp_logger::getModuleLogger("http_client");
        logger->info("🔗 Connection status: " + std::to_string(static_cast<int>(info.status)));
        if (info.status == TransportStatus::CONNECTED) {
            logger->info("✅ Connected to server: " + info.netio_info.remote_address + 
                        ":" + std::to_string(info.netio_info.remote_port));
            
            // 从连接信息中获取 session_id
            if (!info.netio_info.session_id.empty()) {
                client_state.session_id = info.netio_info.session_id;
                logger->info("📋 Session ID from connection: " + client_state.session_id);
            } else {
                logger->info("⚠️  No session ID in connection info");
            }
        } else if (info.status == TransportStatus::DISCONNECTED) {
            logger->info("❌ Disconnected from server");
            client_state.session_id.clear();
        }
    });
    
    // 设置错误回调
    transport->setErrorCallback([](const std::string& error) {
        auto logger = mcp_logger::getModuleLogger("http_client");
        logger->error("❌ Transport error: " + error);
    });
    
    // 启动客户端
    if (!transport->start(config)) {
        logger->error("❌ Failed to start HttpTransport client");
        logger->error("💡 Make sure the server is running on " + host + ":" + std::to_string(port));
        return 1;
    }
    
    logger->info("✅ HttpTransport Client started successfully");
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (!transport->isRunning()) {
        logger->error("❌ Client is not running");
        return 1;
    }
    
    // 执行测试请求
    logger->info("🧪 Starting test requests...");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testPingRequest(*transport, client_state, endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testEchoRequest(*transport, client_state, endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testAddRequest(*transport, client_state, endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testGetTimeRequest(*transport, client_state, endpoint);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    testNotification(*transport, client_state);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    logger->info("✅ All tests completed");
    
    // 停止客户端
    logger->info("🛑 Stopping client...");
    transport->stop();
    
    logger->info("👋 Client stopped");
    return 0;
}

