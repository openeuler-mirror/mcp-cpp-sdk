/**
 * @file mcp_filesystem_client_http_example.cpp
 * @brief MCP FileSystem Client HTTP 传输示例
 * 
 * 这个示例展示了如何使用MCPClient类通过HTTP传输方式连接到文件系统服务器。
 * 演示了基于HTTP的文件系统操作和资源管理功能。
 */

#include "../../client/include/mcp_client.h"
#include "../../transport/include/transport_base.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <ctime>
#include <atomic>

using namespace mcp;
using namespace mcp_transport;
using namespace mcp_logger;

// 全局变量用于优雅关闭
static std::atomic<bool> g_running{true};

// 信号处理函数
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        auto logger = getModuleLogger("mcp_filesystem_client_http");
        logger->info("🛑 收到停止信号，正在关闭客户端...");
        g_running = false;
    }
}

// 打印客户端信息
void printClientInfo() {
    auto logger = getModuleLogger("mcp_filesystem_client_http");
    logger->info("🚀 MCP FileSystem Client HTTP 示例");
    logger->info("=================================");
    logger->info("📋 功能特性:");
    logger->info("  ✅ 基于HTTP传输的文件系统客户端");
    logger->info("  ✅ 文件读取和写入操作");
    logger->info("  ✅ 目录列表和创建");
    logger->info("  ✅ 文件信息查询");
    logger->info("  ✅ 文件删除操作");
    logger->info("  ✅ 资源发现和读取");
    logger->info("  ✅ HTTP/RESTful API 支持");
    logger->info("=================================");
}

// 打印使用说明
void printUsage() {
    auto logger = getModuleLogger("mcp_filesystem_client_http");
    logger->info("📖 使用说明:");
    logger->info("  1. 客户端连接到HTTP服务器进行通信");
    logger->info("  2. 支持的文件系统工具:");
    logger->info("     - read_file: 读取文件内容");
    logger->info("     - write_file: 写入文件内容");
    logger->info("     - list_directory: 列出目录内容");
    logger->info("     - create_directory: 创建目录");
    logger->info("     - delete_file: 删除文件或目录");
    logger->info("     - file_info: 获取文件/目录信息");
    logger->info("  3. 支持的文件系统资源:");
    logger->info("     - file://. : 根目录");
    logger->info("     - file://path : 指定路径的文件或目录");
    logger->info("  4. HTTP端点: http://<host>:<port><endpoint>");
    logger->info("  5. 按 Ctrl+C 停止客户端");
}

// 初始化日志系统
void initializeLogger() {
    mcp_logger::LogConfig config;
    config.level = mcp_logger::LogLevel::DEBUG;
    config.target = mcp_logger::LogTarget::BOTH;  // 同时输出到文件和控制台
    config.file_path = "logs/mcp_filesystem_client_http.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    config.auto_flush = true;
    
    Logger::getInstance().initialize(config);
}

// 格式化JSON输出
std::string formatJson(const nlohmann::json& json, int indent = 2) {
    return json.dump(indent);
}

// 测试工具调用
void testToolCall(MCPClient& client, const std::string& tool_name, const nlohmann::json& arguments, const std::string& description) {
    auto logger = getModuleLogger("mcp_filesystem_client_http");
    
    logger->info("📤 测试: " + description);
    logger->info("  工具: " + tool_name);
    logger->info("  参数: " + formatJson(arguments));
    
    // 构造并打印请求JSON
    nlohmann::json request_json;
    request_json["jsonrpc"] = "2.0";
    request_json["id"] = client.getSessionId();
    request_json["method"] = "tools/call";
    request_json["params"] = {
        {"name", tool_name},
        {"arguments", arguments}
    };
    
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    logger->info("📤 发送请求:");
    logger->info("  Method: " + request_json["method"].get<std::string>());
    logger->info("  ID: " + request_json["id"].get<std::string>());
    logger->info("  完整请求JSON:");
    logger->info(formatJson(request_json, 2));
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    try {
        auto result = client.callTool(tool_name, arguments);
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("📥 收到响应:");
        logger->info("  ✅ 调用成功");
        logger->info("  完整响应JSON:");
        nlohmann::json response_json;
        response_json["jsonrpc"] = "2.0";
        response_json["id"] = request_json["id"];
        response_json["result"] = result.toJson();
        logger->info(formatJson(response_json, 2));
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("  结果: " + formatJson(result.toJson()));
        logger->info("工具调用成功: " + tool_name);
    } catch (const std::exception& e) {
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("📥 收到响应:");
        logger->error("  ❌ 调用失败: " + std::string(e.what()));
        nlohmann::json error_response;
        error_response["jsonrpc"] = "2.0";
        error_response["id"] = request_json["id"];
        error_response["error"] = {
            {"code", -32603},
            {"message", e.what()}
        };
        logger->error("  完整错误响应JSON:");
        logger->error(formatJson(error_response, 2));
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("工具调用失败: " + tool_name + " - " + e.what());
    }
}

// 测试资源读取
void testResourceRead(MCPClient& client, const std::string& uri, const std::string& description) {
    auto logger = getModuleLogger("mcp_filesystem_client_http");
    
    logger->info("📤 测试: " + description);
    logger->info("  资源URI: " + uri);
    
    // 构造并打印请求JSON
    nlohmann::json request_json;
    request_json["jsonrpc"] = "2.0";
    request_json["id"] = client.getSessionId();
    request_json["method"] = "resources/read";
    request_json["params"] = {
        {"uri", uri}
    };
    
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    logger->info("📤 发送请求:");
    logger->info("  Method: " + request_json["method"].get<std::string>());
    logger->info("  ID: " + request_json["id"].get<std::string>());
    logger->info("  完整请求JSON:");
    logger->info(formatJson(request_json, 2));
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    try {
        auto result = client.readResource(uri);
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("📥 收到响应:");
        logger->info("  ✅ 读取成功");
        logger->info("  完整响应JSON:");
        nlohmann::json response_json;
        response_json["jsonrpc"] = "2.0";
        response_json["id"] = request_json["id"];
        response_json["result"] = result;
        logger->info(formatJson(response_json, 2));
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("  内容: " + formatJson(result));
        logger->info("资源读取成功: " + uri);
    } catch (const std::exception& e) {
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("📥 收到响应:");
        logger->error("  ❌ 读取失败: " + std::string(e.what()));
        nlohmann::json error_response;
        error_response["jsonrpc"] = "2.0";
        error_response["id"] = request_json["id"];
        error_response["error"] = {
            {"code", -32603},
            {"message", e.what()}
        };
        logger->error("  完整错误响应JSON:");
        logger->error(formatJson(error_response, 2));
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("资源读取失败: " + uri + " - " + e.what());
    }
}

// 演示客户端功能
void demonstrateClientFeatures(MCPClient& client) {
    auto logger = getModuleLogger("mcp_filesystem_client_http");
    logger->info("开始演示HTTP客户端功能");
    
    logger->info("🔧 HTTP客户端功能演示:");
    
    // 1. 获取工具列表
    logger->info("📋 获取可用工具列表...");
    nlohmann::json list_tools_request;
    list_tools_request["jsonrpc"] = "2.0";
    list_tools_request["id"] = client.getSessionId();
    list_tools_request["method"] = "tools/list";
    list_tools_request["params"] = nlohmann::json::object();
    
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    logger->info("📤 发送请求:");
    logger->info("  Method: " + list_tools_request["method"].get<std::string>());
    logger->info("  ID: " + list_tools_request["id"].get<std::string>());
    logger->info("  完整请求JSON:");
    logger->info(formatJson(list_tools_request, 2));
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    try {
        auto tools = client.listTools();
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("📥 收到响应:");
        logger->info("  ✅ 获取成功，共 " + std::to_string(tools.size()) + " 个工具:");
        nlohmann::json response_json;
        response_json["jsonrpc"] = "2.0";
        response_json["id"] = list_tools_request["id"];
        nlohmann::json tools_array = nlohmann::json::array();
        for (const auto& tool : tools) {
            nlohmann::json tool_json;
            tool_json["name"] = tool.name;
            tool_json["description"] = tool.description;
            tools_array.push_back(tool_json);
        }
        response_json["result"] = {{"tools", tools_array}};
        logger->info("  完整响应JSON:");
        logger->info(formatJson(response_json, 2));
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        for (const auto& tool : tools) {
            logger->info("    - " + tool.name + ": " + tool.description);
        }
        logger->info("工具列表获取成功，共 " + std::to_string(tools.size()) + " 个工具");
    } catch (const std::exception& e) {
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("📥 收到响应:");
        logger->error("  ❌ 获取失败: " + std::string(e.what()));
        nlohmann::json error_response;
        error_response["jsonrpc"] = "2.0";
        error_response["id"] = list_tools_request["id"];
        error_response["error"] = {
            {"code", -32603},
            {"message", e.what()}
        };
        logger->error("  完整错误响应JSON:");
        logger->error(formatJson(error_response, 2));
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("工具列表获取失败: " + std::string(e.what()));
    }
    
    // 2. 获取资源列表
    logger->info("📋 获取可用资源列表...");
    nlohmann::json list_resources_request;
    list_resources_request["jsonrpc"] = "2.0";
    list_resources_request["id"] = client.getSessionId();
    list_resources_request["method"] = "resources/list";
    list_resources_request["params"] = nlohmann::json::object();
    
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    logger->info("📤 发送请求:");
    logger->info("  Method: " + list_resources_request["method"].get<std::string>());
    logger->info("  ID: " + list_resources_request["id"].get<std::string>());
    logger->info("  完整请求JSON:");
    logger->info(formatJson(list_resources_request, 2));
    logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    
    try {
        auto resources = client.listResources();
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("📥 收到响应:");
        logger->info("  ✅ 获取成功，共 " + std::to_string(resources.size()) + " 个资源:");
        nlohmann::json response_json;
        response_json["jsonrpc"] = "2.0";
        response_json["id"] = list_resources_request["id"];
        nlohmann::json resources_array = nlohmann::json::array();
        for (const auto& resource : resources) {
            nlohmann::json resource_json;
            resource_json["uri"] = resource.uri;
            resource_json["name"] = resource.name;
            resource_json["description"] = resource.description;
            resources_array.push_back(resource_json);
        }
        response_json["result"] = {{"resources", resources_array}};
        logger->info("  完整响应JSON:");
        logger->info(formatJson(response_json, 2));
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        for (const auto& resource : resources) {
            logger->info("    - " + resource.name + " (" + resource.uri + ")");
            logger->info("      描述: " + resource.description);
        }
        logger->info("资源列表获取成功，共 " + std::to_string(resources.size()) + " 个资源");
    } catch (const std::exception& e) {
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("📥 收到响应:");
        logger->error("  ❌ 获取失败: " + std::string(e.what()));
        nlohmann::json error_response;
        error_response["jsonrpc"] = "2.0";
        error_response["id"] = list_resources_request["id"];
        error_response["error"] = {
            {"code", -32603},
            {"message", e.what()}
        };
        logger->error("  完整错误响应JSON:");
        logger->error(formatJson(error_response, 2));
        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->error("资源列表获取失败: " + std::string(e.what()));
    }
    
    // 3. 测试文件系统工具调用
    logger->info("🔧 测试文件系统工具调用:");
    
    // 3.1 列出根目录
    testToolCall(client, "list_directory", 
        nlohmann::json{{"path", "."}}, 
        "列出根目录内容");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.2 读取文件
    testToolCall(client, "read_file", 
        nlohmann::json{{"path", "test_files/sample.txt"}}, 
        "读取示例文件");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.3 获取文件信息
    testToolCall(client, "file_info", 
        nlohmann::json{{"path", "test_files/sample.txt"}}, 
        "获取文件信息");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.4 列出子目录
    testToolCall(client, "list_directory", 
        nlohmann::json{{"path", "test_files"}}, 
        "列出test_files目录内容");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.5 创建新文件
    std::string test_content = "这是通过HTTP客户端创建的文件\n";
    test_content += "创建时间: " + std::to_string(std::time(nullptr)) + "\n";
    test_content += "传输方式: HTTP (超文本传输协议)\n";
    
    testToolCall(client, "write_file", 
        nlohmann::json{
            {"path", "test_files/client_created.txt"},
            {"content", test_content}
        }, 
        "创建新文件");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.6 读取刚创建的文件
    testToolCall(client, "read_file", 
        nlohmann::json{{"path", "test_files/client_created.txt"}}, 
        "读取刚创建的文件");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.7 创建新目录
    testToolCall(client, "create_directory", 
        nlohmann::json{{"path", "test_files/client_dir"}}, 
        "创建新目录");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.8 在新目录中创建文件
    testToolCall(client, "write_file", 
        nlohmann::json{
            {"path", "test_files/client_dir/test.txt"},
            {"content", "这是在新目录中创建的文件\n"}
        }, 
        "在新目录中创建文件");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 3.9 列出新创建的目录
    testToolCall(client, "list_directory", 
        nlohmann::json{{"path", "test_files/client_dir"}}, 
        "列出新创建的目录");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 4. 测试资源读取
    logger->info("📚 测试资源读取:");
    
    testResourceRead(client, "file://.", "读取根目录资源");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    testResourceRead(client, "file://test_files/sample.txt", "读取文件资源");
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 5. 显示服务器信息
    logger->info("📊 服务器信息:");
    try {
        auto server_info = client.getServerInfo();
        auto server_caps = client.getServerCapabilities();
        logger->info("  服务器名称: " + server_info.name);
        logger->info("  服务器版本: " + server_info.version);
        logger->info("  服务器能力: " + formatJson(server_caps.toJson()));
        logger->info("服务器信息获取成功");
    } catch (const std::exception& e) {
        logger->error("  ❌ 获取失败: " + std::string(e.what()));
        logger->error("服务器信息获取失败: " + std::string(e.what()));
    }
    
    logger->info("HTTP客户端功能演示完成");
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 打印客户端信息
    printClientInfo();
    
    // 解析命令行参数
    std::string host = "localhost";
    int port = 8080;
    std::string endpoint = "/mcp";
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--endpoint" && i + 1 < argc) {
            endpoint = argv[++i];
            if (endpoint.empty() || endpoint.front() != '/') {
                endpoint = "/" + endpoint;
            }
        } else if (arg == "--help") {
            // 初始化日志系统以便输出帮助信息
            initializeLogger();
            auto logger = getModuleLogger("mcp_filesystem_client_http");
            logger->info("用法: " + std::string(argv[0]) + " [选项]");
            logger->info("选项:");
            logger->info("  --host <host>       设置服务器主机 (默认: localhost)");
            logger->info("  --port <port>       设置服务器端口 (默认: 8080)");
            logger->info("  --endpoint <path>   设置HTTP端点 (默认: /mcp)");
            logger->info("  --help             显示此帮助信息");
            Logger::getInstance().shutdown();
            return 0;
        }
    }
    
    // 初始化日志系统
    initializeLogger();
    auto logger = getModuleLogger("mcp_filesystem_client_http");
    
    logger->info("🌐 服务器: " + host + ":" + std::to_string(port));
    logger->info("📍 端点: " + endpoint);
    
    logger->info("🚀 启动 MCP FileSystem Client HTTP 示例");
    
    try {
        // 创建 MCP 客户端实例
        auto client = std::make_unique<MCPClient>();
        
        // 创建HTTP传输配置（客户端模式）
        mcp_transport::TransportConfig http_config;
        
        // 基本配置
        http_config.type = mcp_transport::TransportType::HTTP;
        http_config.mode = mcp_transport::TransportMode::CLIENT;
        http_config.name = "mcp_filesystem_client_http";
        
        // 日志配置
        http_config.enable_logging = true;
        http_config.log_level = 0;  // 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR
        http_config.log_file_path = "logs/mcp_filesystem_client_http_transport.log";
        
        // HTTP/SSE连接配置
        http_config.sse.host = host;
        http_config.sse.port = port;
        http_config.sse.protocol = netio::Protocol::HTTP;
        http_config.sse.endpoint = endpoint;
        http_config.sse.timeout_seconds = 30;
        http_config.sse.max_retries = 3;
        http_config.sse.keep_alive = true;
        http_config.sse.auto_reconnect = true;
        http_config.sse.reconnect_interval = 5;
        http_config.sse.user_agent = "MCP-FileSystem-Client-HTTP/1.0";
        
        // HTTP请求头配置
        http_config.sse.headers["Content-Type"] = "application/json; charset=utf-8";
        http_config.sse.headers["Accept"] = "application/json, text/event-stream";
        http_config.sse.headers["User-Agent"] = "MCP-FileSystem-Client-HTTP/1.0";
        
        // 设置端点列表（用于端点轮询）
        std::vector<std::string> endpoints = {endpoint};
        client->setEndpoints(endpoints);
        
        logger->info("📋 HTTP传输配置:");
        logger->info("  - 传输类型: HTTP");
        logger->info("  - 模式: CLIENT");
        logger->info("  - 主机: " + host);
        logger->info("  - 端口: " + std::to_string(port));
        logger->info("  - 端点: " + endpoint);
        logger->info("  - 协议: HTTP");
        logger->info("  - 超时: " + std::to_string(http_config.sse.timeout_seconds) + "秒");
        logger->info("  - 最大重试: " + std::to_string(http_config.sse.max_retries));
        logger->info("  - Keep-Alive: " + std::string(http_config.sse.keep_alive ? "启用" : "禁用"));
        logger->info("  - 自动重连: " + std::string(http_config.sse.auto_reconnect ? "启用" : "禁用"));
        
        // 启动连接
        logger->info("🚀 连接到服务器...");
        logger->info("🚀 正在连接到服务器...");
        
        if (!client->connectWithConfig(http_config)) {
            logger->error("❌ 连接失败");
            logger->error("❌ 连接失败，请检查:");
            logger->error("  1. 服务器是否正在运行");
            logger->error("  2. 服务器地址是否正确: http://" + host + ":" + std::to_string(port) + endpoint);
            logger->error("  3. 网络连接是否正常");
            return 1;
        }
        
        logger->info("✅ 连接成功");
        
        // 等待连接建立
        logger->info("⏳ 等待连接建立与服务初始化...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // 检查连接状态
        if (!client->isConnected()) {
            logger->error("❌ 错误: 客户端未连接");
            return 1;
        }
        
        // 设置客户端信息
        client->setClientInfo("mcp-filesystem-client-http", "1.0.0");
        
        // 创建初始化参数
        InitializeParams init_params;
        init_params.protocolVersion = "2025-11-25";
        init_params.clientInfo.name = "mcp-filesystem-client-http";
        init_params.clientInfo.version = "1.0.0";
        init_params.capabilities = mcp::utils::createDefaultClientCapabilities();
        
        // 初始化 MCP 协议
        logger->info("📤 初始化 MCP 协议...");
        
        // 构造并打印初始化请求
        nlohmann::json init_request;
        init_request["jsonrpc"] = "2.0";
        init_request["id"] = std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        init_request["method"] = "initialize";
        init_request["params"] = {
            {"protocolVersion", init_params.protocolVersion},
            {"capabilities", init_params.capabilities.toJson()},
            {"clientInfo", {
                {"name", init_params.clientInfo.name},
                {"version", init_params.clientInfo.version}
            }}
        };
        
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("📤 发送请求:");
        logger->info("  Method: " + init_request["method"].get<std::string>());
        logger->info("  ID: " + init_request["id"].get<std::string>());
        logger->info("  完整请求JSON:");
        logger->info(formatJson(init_request, 2));
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        try {
            auto init_result = client->initialize(init_params);
            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            logger->info("📥 收到响应:");
            logger->info("  ✅ 初始化成功");
            nlohmann::json init_response;
            init_response["jsonrpc"] = "2.0";
            init_response["id"] = init_request["id"];
            init_response["result"] = {
                {"protocolVersion", init_result.protocolVersion},
                {"capabilities", init_result.capabilities.toJson()},
                {"serverInfo", {
                    {"name", init_result.serverInfo.name},
                    {"version", init_result.serverInfo.version}
                }}
            };
            logger->info("  完整响应JSON:");
            logger->info(formatJson(init_response, 2));
            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            logger->info("  服务器: " + init_result.serverInfo.name + " v" + init_result.serverInfo.version);
            logger->info("  协议版本: " + init_result.protocolVersion);
            
            // 从响应头中提取 session_id（已在 initialize 内部自动提取）
            std::string session_id = client->getSessionId();
            if (!session_id.empty()) {
                logger->info("  📋 Session ID: " + session_id);
                logger->info("  ✅ Session ID 已提取，将作为后续请求的 reqid");
            } else {
                logger->warning("  ⚠️  未从响应头中提取到 Session ID");
            }
        } catch (const std::exception& e) {
            logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            logger->error("📥 收到响应:");
            logger->error("  ❌ 初始化失败: " + std::string(e.what()));
            nlohmann::json error_response;
            error_response["jsonrpc"] = "2.0";
            error_response["id"] = init_request["id"];
            error_response["error"] = {
                {"code", -32603},
                {"message", e.what()}
            };
            logger->error("  完整错误响应JSON:");
            logger->error(formatJson(error_response, 2));
            logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            return 1;
        }
        
        // 发送初始化完成通知
        logger->info("📤 发送初始化完成通知...");
        nlohmann::json initialized_notification;
        initialized_notification["jsonrpc"] = "2.0";
        initialized_notification["method"] = "notifications/initialized";
        initialized_notification["params"] = nlohmann::json::object();
        
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        logger->info("📤 发送通知 (无响应):");
        logger->info("  Method: " + initialized_notification["method"].get<std::string>());
        logger->info("  完整通知JSON:");
        logger->info(formatJson(initialized_notification, 2));
        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
        
        try {
            client->initialized();
            logger->info("✅ 初始化完成通知发送成功");
        } catch (const std::exception& e) {
            logger->error("❌ 初始化完成通知发送失败: " + std::string(e.what()));
        }
        
        printUsage();
        
        // 演示客户端功能
        demonstrateClientFeatures(*client);
        
        // 测试连接稳定性
        logger->info("🔧 测试连接稳定性:");
        for (int i = 0; i < 3; ++i) {
            logger->info("  稳定性测试 " + std::to_string(i + 1) + "/3");
            try {
                auto result = client->ping();
                logger->info("    ✅ Ping " + std::to_string(i + 1) + " 成功");
                logger->info("Ping " + std::to_string(i + 1) + " 成功");
            } catch (const std::exception& e) {
                logger->error("    ❌ Ping " + std::to_string(i + 1) + " 失败: " + e.what());
                logger->error("Ping " + std::to_string(i + 1) + " 失败: " + e.what());
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // 断开连接
        logger->info("🛑 断开连接...");
        logger->info("🛑 正在断开连接...");
        client->disconnect();
        logger->info("✅ 连接已断开");
        
    } catch (const std::exception& e) {
        logger->error("❌ 客户端错误: " + std::string(e.what()));
        return 1;
    }
    
    // 关闭日志系统
    Logger::getInstance().shutdown();
    return 0;
}

