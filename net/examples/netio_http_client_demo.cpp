/**
 * @file netio_http_client_demo.cpp
 * @brief 使用NetIO创建HTTP客户端的demo
 * 
 * 这个文件演示了如何使用NetIO库创建一个HTTP客户端，
 * 连接到netio_http_server_demo服务器并测试所有功能。
 */

#include "netio.h"
#include "../../log/include/logger.h"
#include "../../common/json.hpp"
#include <string>
#include <chrono>
#include <thread>

using namespace netio;
using json = nlohmann::json;

// 生成请求ID的辅助函数
std::string generateRequestId() {
    static int counter = 0;
    return "req_" + std::to_string(++counter) + "_" + 
           std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count());
}

// 创建JSON-RPC请求
Message createJsonRpcRequest(const std::string& method, const json& params = json::object(), const std::string& id = "") {
    json request_json;
    request_json["jsonrpc"] = "2.0";
    request_json["method"] = method;
    if (!params.empty()) {
        request_json["params"] = params;
    }
    if (!id.empty()) {
        request_json["id"] = id;
    } else {
        request_json["id"] = generateRequestId();
    }
    
    Message message;
    message.id = request_json["id"].is_string() 
        ? request_json["id"].get<std::string>() 
        : request_json["id"].dump();
    message.type = MessageType::REQUEST;
    message.method = "POST";
    message.path = "/mcp";
    message.body = request_json.dump();
    message.headers = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"User-Agent", "NetIO-HTTP-Client/1.0"}
    };
    message.timestamp = std::chrono::system_clock::now();
    
    return message;
}

// 创建JSON-RPC通知（无ID）
Message createJsonRpcNotification(const std::string& method, const json& params = json::object()) {
    json request_json;
    request_json["jsonrpc"] = "2.0";
    request_json["method"] = method;
    if (!params.empty()) {
        request_json["params"] = params;
    }
    // 通知不包含id字段
    
    Message message;
    message.id = generateRequestId();
    message.type = MessageType::NOTIFICATION;
    message.method = "POST";
    message.path = "/mcp";
    message.body = request_json.dump();
    message.headers = {
        {"Content-Type", "application/json"},
        {"Accept", "application/json"},
        {"User-Agent", "NetIO-HTTP-Client/1.0"}
    };
    message.timestamp = std::chrono::system_clock::now();
    
    return message;
}

// 创建连接请求（GET请求）
Message createConnectionRequest(const std::string& endpoint = "/mcp") {
    Message message;
    message.id = generateRequestId();
    message.type = MessageType::REQUEST;
    message.method = "GET";
    message.path = endpoint;
    message.headers = {
        {"Accept", "application/json"},
        {"User-Agent", "NetIO-HTTP-Client/1.0"}
    };
    message.timestamp = std::chrono::system_clock::now();
    
    return message;
}

// 解析并显示响应
void displayResponse(const Message& response, const std::string& test_name) {
    auto logger = mcp_logger::getModuleLogger("netio_http_client");
    
    logger->info("\n" + std::string(60, '='));
    logger->info("📋 Test: " + test_name);
    logger->info(std::string(60, '-'));
    logger->info("📥 Response Status: " + std::to_string(response.status_code));
    
    if (!response.headers.empty()) {
        logger->info("📋 Response Headers:");
        for (const auto& [key, value] : response.headers) {
            logger->info("  " + key + ": " + value);
        }
    }
    
    if (response.status_code == 204) {
        logger->info("✅ Notification sent successfully (No Content)");
        return;
    }
    
    if (response.body.empty()) {
        logger->warning("⚠️ Empty response body");
        return;
    }
    
    logger->info("📦 Response Body:");
    
    try {
        json response_json = json::parse(response.body);
        logger->info(response_json.dump(2));
        
        // 解析JSON-RPC响应
        if (response_json.contains("result")) {
            logger->info("\n✅ Request successful");
            logger->info("📊 Result: " + response_json["result"].dump(2));
        } else if (response_json.contains("error")) {
            logger->error("\n❌ Request failed");
            auto error = response_json["error"];
            logger->error("   Code: " + std::to_string(error.value("code", -1)));
            logger->error("   Message: " + error.value("message", "Unknown error"));
        } else {
            logger->warning("\n⚠️ Unknown response format");
        }
    } catch (const std::exception& e) {
        logger->warning("⚠️ Response is not valid JSON: " + std::string(e.what()));
        logger->info("Raw response: " + response.body);
    }
    
    logger->info(std::string(60, '='));
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/netio_http_client_demo.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] [{module}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    auto logger = mcp_logger::getModuleLogger("netio_http_client");
    logger->info("🚀 Starting NetIO HTTP Client Example");
    
    // 解析命令行参数
    std::string host = "localhost";
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
    
    logger->info("📡 Client configuration: host=" + host + ", port=" + std::to_string(port) + ", endpoint=" + endpoint);
    
    try {
        // 创建NetIO实例
        NetIO netio_client;
        
        // 配置网络连接
        NetConfig config;
        config.host = host;
        config.port = port;
        config.protocol = Protocol::HTTP;
        config.timeout_seconds = 30;
        config.max_retries = 3;
        config.keep_alive = true;
        config.auto_reconnect = false;
        config.user_agent = "NetIO-HTTP-Client/1.0";
        
        // 设置默认请求头
        config.headers = {
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}
        };
        
        logger->info("\n" + std::string(60, '='));
        logger->info("🚀 NetIO HTTP Client Demo");
        logger->info(std::string(60, '='));
        logger->info("📡 Connecting to: http://" + host + ":" + std::to_string(port) + endpoint);
        logger->info("");
        
        // 连接到服务器
        logger->info("⏳ Connecting to server...");
        
        if (!netio_client.connect(config)) {
            logger->error("❌ Failed to connect to server");
            logger->error("Please make sure the server is running on " + host + ":" + std::to_string(port));
            return 1;
        }
        
        // 检查连接状态
        auto conn_info = netio_client.getConnectionInfo();
        logger->info("✅ Connected successfully");
        logger->info("   Connection ID: " + conn_info.connection_id);
        logger->info("   Remote: " + conn_info.remote_address + ":" + std::to_string(conn_info.remote_port));
        logger->info("   Local: " + conn_info.local_address + ":" + std::to_string(conn_info.local_port));
        logger->info("");
        
        // 测试1: 连接请求（GET请求，获取session_id）
        {
            logger->info("📤 Sending connection request (GET)...");
            Message conn_request = createConnectionRequest(endpoint);
            Message conn_response = netio_client.sendRequestSync(conn_request);
            
            std::string session_id;
            if (conn_response.headers.find("X-Session-Id") != conn_response.headers.end()) {
                session_id = conn_response.headers.at("X-Session-Id");
                logger->info("✅ Received session_id: " + session_id);
            }
            
            displayResponse(conn_response, "Connection Request (GET)");
        }
        
        // 等待一小段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试2: ping方法
        {
            logger->info("📤 Sending ping request...");
            Message ping_request = createJsonRpcRequest("ping");
            Message ping_response = netio_client.sendRequestSync(ping_request);
            displayResponse(ping_response, "Ping Request");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试3: echo方法
        {
            logger->info("📤 Sending echo request...");
            json echo_params = {
                {"message", "Hello from NetIO Client!"},
                {"number", 42},
                {"nested", {
                    {"key", "value"},
                    {"array", {1, 2, 3}}
                }}
            };
            Message echo_request = createJsonRpcRequest("echo", echo_params);
            Message echo_response = netio_client.sendRequestSync(echo_request);
            displayResponse(echo_response, "Echo Request");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试4: add方法
        {
            logger->info("📤 Sending add request...");
            json add_params = {
                {"a", 15},
                {"b", 27}
            };
            Message add_request = createJsonRpcRequest("add", add_params);
            Message add_response = netio_client.sendRequestSync(add_request);
            displayResponse(add_response, "Add Request (15 + 27)");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试5: get_time方法
        {
            logger->info("📤 Sending get_time request...");
            Message time_request = createJsonRpcRequest("get_time");
            Message time_response = netio_client.sendRequestSync(time_request);
            displayResponse(time_response, "Get Time Request");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试6: 发送通知（log）
        {
            logger->info("📤 Sending log notification...");
            json log_params = {
                {"message", "This is a test log message from the client"}
            };
            Message log_notification = createJsonRpcNotification("log", log_params);
            Message log_response = netio_client.sendRequestSync(log_notification);
            displayResponse(log_response, "Log Notification");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试7: 测试错误处理 - 未知方法
        {
            logger->info("📤 Sending unknown method request...");
            Message unknown_request = createJsonRpcRequest("unknown_method");
            Message unknown_response = netio_client.sendRequestSync(unknown_request);
            displayResponse(unknown_response, "Unknown Method Request (Error Test)");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试8: 测试错误处理 - 无效参数
        {
            logger->info("📤 Sending add request with invalid parameters...");
            json invalid_params = {
                {"a", "not_a_number"},
                {"b", 27}
            };
            Message invalid_request = createJsonRpcRequest("add", invalid_params);
            Message invalid_response = netio_client.sendRequestSync(invalid_request);
            displayResponse(invalid_response, "Add Request with Invalid Parameters (Error Test)");
        }
        
        // 显示统计信息
        logger->info("\n" + std::string(60, '='));
        logger->info("📊 Statistics");
        logger->info(std::string(60, '-'));
        auto stats = netio_client.getStatistics();
        logger->info(stats.dump(2));
        logger->info(std::string(60, '='));
        
        // 断开连接
        logger->info("⏳ Disconnecting...");
        logger->info("\n⏳ Disconnecting...");
        netio_client.disconnect();
        logger->info("✅ Disconnected");
        
        logger->info("\n✅ All tests completed successfully!");
        
    } catch (const std::exception& e) {
        auto logger = mcp_logger::getModuleLogger("netio_http_client");
        logger->error("❌ Error: " + std::string(e.what()));
        return 1;
    } catch (...) {
        auto logger = mcp_logger::getModuleLogger("netio_http_client");
        logger->error("❌ Unknown error occurred");
        return 1;
    }
    
    return 0;
}

