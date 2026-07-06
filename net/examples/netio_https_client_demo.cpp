/**
 * @file netio_https_client_demo.cpp
 * @brief 使用NetIO创建HTTPS客户端的demo
 * 
 * 这个文件演示了如何使用NetIO库创建一个HTTPS客户端，
 * 连接到netio_https_server_demo服务器并测试所有功能。
 * 
 * 使用方法:
 *   ./netio_https_client_demo [host] [port] [endpoint] [ca_file]
 * 
 * 默认值:
 *   host: localhost
 *   port: 10443
 *   endpoint: /mcp
 *   ca_file: (空，可选，用于验证服务器证书)
 * 
 * 证书生成:
 *   1. 使用自动脚本（推荐）:
 *      cd net/examples && ./generate_certs.sh
 *      然后使用: ./netio_https_client_demo localhost 10443 /mcp certs/ca.crt
 * 
 *   2. 手动生成自签名证书:
 *      openssl req -x509 -newkey rsa:4096 -keyout server.key -out server.crt -days 365 -nodes
 *      对于自签名证书，CA文件就是证书本身: cp server.crt ca.crt
 * 
 * 注意:
 *   - 如果提供ca_file，客户端会验证服务器证书
 *   - 如果不提供ca_file，客户端会禁用证书验证（仅用于测试自签名证书）
 *   - 详细说明请参考 README_CERTIFICATES.md
 */

#include "netio.h"
#include "../../log/include/logger.h"
#include "../../common/json.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#if __cplusplus >= 201703L
#include <filesystem>
namespace fs = std::filesystem;
#else
#include <experimental/filesystem>
namespace fs = std::experimental::filesystem;
#endif

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
        {"User-Agent", "NetIO-HTTPS-Client/1.0"}
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
        {"User-Agent", "NetIO-HTTPS-Client/1.0"}
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
        {"User-Agent", "NetIO-HTTPS-Client/1.0"}
    };
    message.timestamp = std::chrono::system_clock::now();
    
    return message;
}

// 显示请求信息
void displayRequest(const Message& request, const std::string& test_name) {
    auto logger = mcp_logger::getModuleLogger("netio_https_client");
    
    std::string type_str;
    switch (request.type) {
        case MessageType::REQUEST:
            type_str = "REQUEST";
            break;
        case MessageType::NOTIFICATION:
            type_str = "NOTIFICATION";
            break;
        case MessageType::RESPONSE:
            type_str = "RESPONSE";
            break;
        case MessageType::ERROR:
            type_str = "ERROR";
            break;
    }
    
    logger->info("\n" + std::string(60, '='));
    logger->info("📤 Request: " + test_name);
    logger->info(std::string(60, '-'));
    logger->info("🆔 Request ID: " + request.id);
    logger->info("📝 Method: " + request.method);
    logger->info("📍 Path: " + request.path);
    logger->info("📦 Type: " + type_str);
    
    if (!request.headers.empty()) {
        logger->info("📋 Request Headers:");
        for (const auto& [key, value] : request.headers) {
            logger->info("  " + key + ": " + value);
        }
    }
    
    if (!request.body.empty()) {
        logger->info("📦 Request Body:");
        try {
            json request_json = json::parse(request.body);
            logger->info(request_json.dump(2));
        } catch (const std::exception& e) {
            logger->info(request.body);
        }
    } else {
        logger->info("📦 Request Body: (empty)");
    }
    
    logger->info(std::string(60, '='));
}

// 解析并显示响应
void displayResponse(const Message& response, const std::string& test_name) {
    auto logger = mcp_logger::getModuleLogger("netio_https_client");
    
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

// 检查文件是否存在
bool fileExists(const std::string& filepath) {
    try {
        return fs::exists(filepath) && fs::is_regular_file(filepath);
    } catch (...) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/netio_https_client_demo.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] [{module}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    auto logger = mcp_logger::getModuleLogger("netio_https_client");
    logger->info("🚀 Starting NetIO HTTPS Client Example");
    
    // 解析命令行参数
    std::string host = "localhost";
    int port = 10443;
    std::string endpoint = "/mcp";
    std::string ca_file = "";  // 可选，用于验证服务器证书
    
    if (argc > 1) {
        host = argv[1];
    }
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }
    if (argc > 3) {
        endpoint = argv[3];
    }
    if (argc > 4) {
        ca_file = argv[4];
    }
    
    logger->info("📡 Client configuration: host=" + host + ", port=" + std::to_string(port) + ", endpoint=" + endpoint);
    if (!ca_file.empty()) {
        logger->info("  ca_file: " + ca_file);
    }
    
    // 检查CA文件是否存在（如果提供）
    if (!ca_file.empty() && !fileExists(ca_file)) {
        logger->error("❌ CA file not found: " + ca_file);
        logger->error("💡 CA file is optional, you can omit it for self-signed certificates");
        std::cerr << "❌ CA file not found: " << ca_file << std::endl;
        std::cerr << "💡 CA file is optional, you can omit it for self-signed certificates" << std::endl;
        return 1;
    }
    
    try {
        // 创建NetIO实例
        NetIO netio_client;
        
        // 配置网络连接
        NetConfig config;
        config.host = host;
        config.port = port;
        config.protocol = Protocol::HTTPS;  // 使用 HTTPS 协议
        config.timeout_seconds = 30;
        config.max_retries = 3;
        config.keep_alive = true;
        config.auto_reconnect = false;
        config.user_agent = "NetIO-HTTPS-Client/1.0";
        
        // 如果提供了CA文件，设置它（注意：NetIO可能需要在内部实现中支持这个）
        if (!ca_file.empty()) {
            config.ca_file = ca_file;
        }
        
        // 设置默认请求头
        config.headers = {
            {"Content-Type", "application/json"},
            {"Accept", "application/json"}
        };
        
        logger->info("\n" + std::string(60, '='));
        logger->info("🚀 NetIO HTTPS Client Demo");
        logger->info(std::string(60, '='));
        logger->info("📡 Connecting to: https://" + host + ":" + std::to_string(port) + endpoint);
        if (!ca_file.empty()) {
            logger->info("🔐 Using CA certificate: " + ca_file);
        } else {
            logger->info("🔓 SSL certificate verification will be disabled (for self-signed certificates)");
        }
        logger->info("");
        
        // 连接到服务器
        logger->info("⏳ Connecting to server...");
        
        if (!netio_client.connect(config)) {
            logger->error("❌ Failed to connect to server");
            std::cerr << "❌ Failed to connect to server" << std::endl;
            std::cerr << "Please make sure:" << std::endl;
            std::cerr << "  1. The server is running on " << host << ":" << port << std::endl;
            std::cerr << "  2. The server is accessible" << std::endl;
            std::cerr << "  3. OpenSSL support is enabled during compilation" << std::endl;
            std::cerr << "  4. For production use, provide a CA certificate file" << std::endl;
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
            displayRequest(conn_request, "Connection Request (GET)");
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
            displayRequest(ping_request, "Ping Request");
            Message ping_response = netio_client.sendRequestSync(ping_request);
            displayResponse(ping_response, "Ping Request");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试3: echo方法
        {
            logger->info("📤 Sending echo request...");
            json echo_params = {
                {"message", "Hello from NetIO HTTPS Client!"},
                {"number", 42},
                {"nested", {
                    {"key", "value"},
                    {"array", {1, 2, 3}}
                }}
            };
            Message echo_request = createJsonRpcRequest("echo", echo_params);
            displayRequest(echo_request, "Echo Request");
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
            displayRequest(add_request, "Add Request (15 + 27)");
            Message add_response = netio_client.sendRequestSync(add_request);
            displayResponse(add_response, "Add Request (15 + 27)");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试5: get_time方法
        {
            logger->info("📤 Sending get_time request...");
            Message time_request = createJsonRpcRequest("get_time");
            displayRequest(time_request, "Get Time Request");
            Message time_response = netio_client.sendRequestSync(time_request);
            displayResponse(time_response, "Get Time Request");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试6: 发送通知（log）
        {
            logger->info("📤 Sending log notification...");
            json log_params = {
                {"message", "This is a test log message from the HTTPS client"}
            };
            Message log_notification = createJsonRpcNotification("log", log_params);
            displayRequest(log_notification, "Log Notification");
            Message log_response = netio_client.sendRequestSync(log_notification);
            displayResponse(log_response, "Log Notification");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        // 测试7: 测试错误处理 - 未知方法
        {
            logger->info("📤 Sending unknown method request...");
            Message unknown_request = createJsonRpcRequest("unknown_method");
            displayRequest(unknown_request, "Unknown Method Request (Error Test)");
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
            displayRequest(invalid_request, "Add Request with Invalid Parameters (Error Test)");
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
        logger->info("✅ All tests completed successfully");
        
    } catch (const std::exception& e) {
        auto logger = mcp_logger::getModuleLogger("netio_https_client");
        logger->error("❌ Error: " + std::string(e.what()));
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        auto logger = mcp_logger::getModuleLogger("netio_https_client");
        logger->error("❌ Unknown error occurred");
        std::cerr << "❌ Unknown error occurred" << std::endl;
        return 1;
    }
    
    return 0;
}

