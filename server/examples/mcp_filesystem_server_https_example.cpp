/**
 * @file mcp_filesystem_server_https_example.cpp
 * @brief MCP FileSystem Server HTTPS 传输示例
 * 
 * 这个示例展示了如何使用MCPFileSystemServer类通过HTTPS传输方式创建文件系统服务器。
 * 演示了基于HTTPS的文件系统操作和资源管理功能。
 */

#include "mcp_filesystem_server.h"
#include "../../transport/include/https_transport.h"
#include "../../transport/include/transport_base.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <filesystem>
#include <random>
#include <sstream>

using namespace mcp;
using namespace mcp_transport;

// 全局变量用于优雅关闭
static std::atomic<bool> g_running{true};

// 信号处理函数
void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
        logger->info("🛑 收到停止信号，正在关闭服务器...");
        g_running = false;
    }
}

// 打印服务器信息
void printServerInfo() {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
    logger->info("🚀 MCP FileSystem Server HTTPS 示例");
    logger->info("=================================");
    logger->info("📋 功能特性:");
    logger->info("  ✅ 基于HTTPS传输的文件系统服务器");
    logger->info("  ✅ SSL/TLS加密通信");
    logger->info("  ✅ 安全的路径验证和解析");
    logger->info("  ✅ 可配置的读写权限控制");
    logger->info("  ✅ 完整的文件系统操作工具");
    logger->info("  ✅ 自动资源发现和管理");
    logger->info("  ✅ HTTPS/RESTful API 支持");
    logger->info("=================================");
}

// 打印使用说明
void printUsage() {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
    logger->info("📖 使用说明:");
    logger->info("  1. 服务器启动后，通过HTTPS与客户端进行加密通信");
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
    logger->info("  4. HTTPS端点: https://localhost:<port><endpoint>");
    logger->info("  5. 按 Ctrl+C 停止服务器");
}

// 初始化日志系统
void initializeLogger() {
    mcp_logger::LogConfig config;
    config.level = mcp_logger::LogLevel::DEBUG;
    config.target = mcp_logger::LogTarget::BOTH;  // 同时输出到文件和控制台
    config.file_path = "logs/mcp_filesystem_server_https.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(config);
}

// 创建测试环境
void createTestEnvironment(const std::string& root_path) {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
    try {
        // 创建测试目录结构
        std::filesystem::create_directories(root_path + "/test_files");
        std::filesystem::create_directories(root_path + "/test_files/subdir");
        std::filesystem::create_directories(root_path + "/docs");
        std::filesystem::create_directories(root_path + "/logs");
        
        // 创建测试文件
        std::ofstream sample_file(root_path + "/test_files/sample.txt");
        if (sample_file.is_open()) {
            sample_file << "这是一个HTTPS测试文件\n";
            sample_file << "用于演示MCP文件系统服务器的HTTPS功能\n";
            sample_file << "创建时间: " << std::time(nullptr) << "\n";
            sample_file << "传输方式: HTTPS (安全超文本传输协议)\n";
            sample_file.close();
        }
        
        std::ofstream readme_file(root_path + "/README.md");
        if (readme_file.is_open()) {
            readme_file << "# MCP FileSystem Server HTTPS Demo\n\n";
            readme_file << "这个目录包含用于测试MCP文件系统服务器HTTPS功能的示例文件和目录。\n\n";
            readme_file << "## 传输方式\n\n";
            readme_file << "- **HTTPS (安全超文本传输协议)**: 通过HTTPS协议进行加密通信\n";
            readme_file << "- **优势**: 标准化、跨平台、易于调试、适合网络部署、SSL/TLS加密\n";
            readme_file << "- **适用场景**: Web应用、远程访问、分布式系统、需要安全通信的场景\n\n";
            readme_file << "## 目录结构\n\n";
            readme_file << "- `test_files/`: 测试文件目录\n";
            readme_file << "  - `sample.txt`: 示例文本文件\n";
            readme_file << "  - `subdir/`: 子目录\n";
            readme_file << "- `docs/`: 文档目录\n";
            readme_file << "- `logs/`: 日志目录\n";
            readme_file << "- `README.md`: 本说明文件\n\n";
            readme_file << "## 测试方法\n\n";
            readme_file << "1. 使用 `list_directory` 工具列出目录内容\n";
            readme_file << "2. 使用 `read_file` 工具读取文件内容\n";
            readme_file << "3. 使用 `write_file` 工具创建新文件\n";
            readme_file << "4. 使用 `create_directory` 工具创建新目录\n";
            readme_file << "5. 使用 `file_info` 工具获取文件信息\n\n";
            readme_file << "## HTTPS 通信特点\n\n";
            readme_file << "- 标准化协议，易于集成\n";
            readme_file << "- 支持跨网络访问\n";
            readme_file << "- SSL/TLS加密，保证通信安全\n";
            readme_file << "- 可以使用标准HTTPS工具测试（curl, Postman等）\n";
            readme_file << "- 适合Web应用和远程客户端\n";
            readme_file << "- 需要SSL证书配置\n";
            readme_file.close();
        }
        
        std::ofstream config_file(root_path + "/test_files/https_config.json");
        if (config_file.is_open()) {
            config_file << "{\n";
            config_file << "  \"server_name\": \"MCP FileSystem Server HTTPS\",\n";
            config_file << "  \"version\": \"1.0.0\",\n";
            config_file << "  \"transport_type\": \"HTTPS\",\n";
            config_file << "  \"features\": [\n";
            config_file << "    \"file_operations\",\n";
            config_file << "    \"directory_management\",\n";
            config_file << "    \"path_validation\",\n";
            config_file << "    \"resource_discovery\",\n";
            config_file << "    \"https_communication\",\n";
            config_file << "    \"ssl_tls_encryption\"\n";
            config_file << "  ],\n";
            config_file << "  \"https_settings\": {\n";
            config_file << "    \"protocol\": \"HTTPS\",\n";
            config_file << "    \"timeout_seconds\": 30,\n";
            config_file << "    \"max_retries\": 3,\n";
            config_file << "    \"keep_alive\": true,\n";
            config_file << "    \"ssl_enabled\": true\n";
            config_file << "  },\n";
            config_file << "  \"created_at\": " << std::time(nullptr) << "\n";
            config_file << "}\n";
            config_file.close();
        }
        
        logger->info("✅ HTTPS测试环境创建完成");
    } catch (const std::exception& e) {
        logger->warning("⚠️  创建测试环境失败: " + std::string(e.what()));
    }
}

// 演示服务器功能
void demonstrateServerFeatures(MCPFileSystemServer& server) {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
    logger->info("开始演示HTTPS服务器功能");
    
    logger->info("🔧 HTTPS服务器功能演示:");
    
    // 显示根路径
    logger->info("  📁 根路径: " + server.getRootPath());
    
    // 显示写权限状态
    logger->info("  ✏️  写操作: " + std::string(server.isWriteEnabled() ? "启用" : "禁用"));
    
    // 显示文件系统资源
    auto resources = server.listFileSystemResources();
    logger->info("  📋 文件系统资源 (" + std::to_string(resources.size()) + "个):");
    for (const auto& resource : resources) {
        logger->info("    - " + resource);
    }
    
    // 添加HTTPS特定的资源
    server.addFileSystemResource("test_files/https_config.json", "HTTPS配置", "HTTPS传输配置文件");
    server.addFileSystemResource("logs", "日志目录", "服务器日志文件目录");
    logger->info("  ➕ 添加了HTTPS特定资源");
    
    logger->info("HTTPS服务器功能演示完成");
}

// 运行服务器统计信息
void runServerStatistics(MCPFileSystemServer& server) {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
    
    int heartbeat_counter = 0;
    int stats_counter = 0;
    
    while (g_running && server.isRunning()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        heartbeat_counter++;
        stats_counter++;
        
        // 每30秒发送一次心跳
        if (heartbeat_counter >= 300) { // 100ms * 300 = 30s
            heartbeat_counter = 0;
            logger->info("💓 HTTPS服务器心跳正常");
        }
        
        // 每5分钟显示一次统计信息
        if (stats_counter >= 3000) { // 100ms * 3000 = 5分钟
            stats_counter = 0;
            logger->info("📊 HTTPS服务器运行统计:");
            logger->info("  - 运行时间: " + std::to_string(std::time(nullptr)) + " 秒");
            logger->info("  - 根路径: " + server.getRootPath());
            logger->info("  - 资源数量: " + std::to_string(server.listFileSystemResources().size()));
            logger->info("  - 传输方式: HTTPS (安全超文本传输协议)");
        }
    }
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 打印服务器信息
    printServerInfo();
    
    // 解析命令行参数
    std::string root_path = ".";
    bool enable_write = true;
    std::string host = "0.0.0.0";
    int port = 8443;
    std::string endpoint = "/mcp";
    std::string cert_file = "";
    std::string key_file = "";
    std::string ca_file = "";
    bool verify_peer = true;
    bool verify_hostname = true;
    bool disable_session_validation = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--root" && i + 1 < argc) {
            root_path = argv[++i];
        } else if (arg == "--read-only") {
            enable_write = false;
        } else if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--port" && i + 1 < argc) {
            port = std::stoi(argv[++i]);
        } else if (arg == "--endpoint" && i + 1 < argc) {
            endpoint = argv[++i];
            if (endpoint.empty() || endpoint.front() != '/') {
                endpoint = "/" + endpoint;
            }
        } else if (arg == "--cert-file" && i + 1 < argc) {
            cert_file = argv[++i];
        } else if (arg == "--key-file" && i + 1 < argc) {
            key_file = argv[++i];
        } else if (arg == "--ca-file" && i + 1 < argc) {
            ca_file = argv[++i];
        } else if (arg == "--no-verify-peer") {
            verify_peer = false;
        } else if (arg == "--no-verify-hostname") {
            verify_hostname = false;
        } else if (arg == "--disable-session-validation") {
            disable_session_validation = true;
        } else if (arg == "--help") {
            // 初始化日志系统以便输出帮助信息
            initializeLogger();
            auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
            logger->info("用法: " + std::string(argv[0]) + " [选项]");
            logger->info("选项:");
            logger->info("  --root <path>           设置根路径 (默认: 当前目录)");
            logger->info("  --read-only              禁用写操作");
            logger->info("  --host <host>           设置监听主机 (默认: 0.0.0.0)");
            logger->info("  --port <port>           设置监听端口 (默认: 8443)");
            logger->info("  --endpoint <path>       设置HTTPS端点 (默认: /mcp)");
            logger->info("  --cert-file <path>      设置SSL证书文件路径 (必需)");
            logger->info("  --key-file <path>       设置SSL私钥文件路径 (必需)");
            logger->info("  --ca-file <path>        设置CA证书文件路径 (可选)");
            logger->info("  --no-verify-peer        禁用对等方证书验证 (仅用于测试)");
            logger->info("  --no-verify-hostname    禁用主机名验证 (仅用于测试)");
            logger->info("  --disable-session-validation  禁用 session ID 验证 (仅用于测试)");
            logger->info("  --help                  显示此帮助信息");
            logger->info("");
            logger->info("注意: HTTPS服务器需要提供 --cert-file 和 --key-file");
            logger->info("      可以使用 transport/examples/generate_certs.sh 生成自签名证书");
            mcp_logger::Logger::getInstance().shutdown();
            return 0;
        }
    }
    
    // 初始化日志系统
    initializeLogger();
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server_https");
    
    logger->info("📁 根路径: " + root_path);
    logger->info("✏️  写操作: " + std::string(enable_write ? "启用" : "禁用"));
    logger->info("🌐 主机: " + host);
    logger->info("🔌 端口: " + std::to_string(port));
    logger->info("📍 端点: " + endpoint);
    if (!cert_file.empty()) {
        logger->info("🔐 证书文件: " + cert_file);
    }
    if (!key_file.empty()) {
        logger->info("🔑 私钥文件: " + key_file);
    }
    if (!ca_file.empty()) {
        logger->info("📜 CA证书文件: " + ca_file);
    }
    logger->info("🔐 Session验证: " + std::string(disable_session_validation ? "禁用" : "启用"));
    
    // 检查必需的SSL证书文件
    if (cert_file.empty() || key_file.empty()) {
        logger->error("❌ 错误: HTTPS服务器需要提供 --cert-file 和 --key-file");
        logger->error("💡 可以使用以下命令生成自签名证书:");
        logger->error("   cd transport/examples");
        logger->error("   ./generate_certs.sh");
        logger->error("   然后使用生成的 server.crt 和 server.key");
        return 1;
    }
    
    logger->info("🚀 启动 MCP FileSystem Server HTTPS 示例");
    
    try {
        // 创建测试环境
        createTestEnvironment(root_path);
        
        // 创建文件系统服务器实例
        MCPFileSystemServer server(root_path, enable_write);
        
        // 设置是否禁用 session 验证
        if (disable_session_validation) {
            server.setDisableSessionValidation(true);
            logger->info("⚠️  Session ID 验证已禁用（仅用于测试）");
        }
        
        // 设置 notifications/initialized 通知处理器
        server.setNotificationHandler("notifications/initialized", [&logger](const std::string& method, const json& params) {
            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            logger->info("📥 收到客户端初始化完成通知:");
            logger->info("  Method: " + method);
            logger->info("  Params: " + params.dump(2));
            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
            logger->info("✅ 客户端已成功初始化并准备好接收请求");
        });
        
        // 演示服务器功能
        demonstrateServerFeatures(server);
        
        // 创建HTTPS传输配置（服务端模式）
        mcp_transport::TransportConfig https_config;
        
        // 基本配置
        https_config.type = mcp_transport::TransportType::HTTPS;
        https_config.mode = mcp_transport::TransportMode::SERVER;
        https_config.name = "mcp_filesystem_server_https";
        
        // 日志配置
        https_config.enable_logging = true;
        https_config.log_level = 0;  // 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR
        https_config.log_file_path = "logs/mcp_filesystem_server_https_transport.log";
        
        // HTTPS/SSE服务端配置
        https_config.sse.host = host;
        https_config.sse.port = port;
        https_config.sse.protocol = netio::Protocol::HTTPS;
        https_config.sse.endpoint = endpoint;
        https_config.sse.timeout_seconds = 30;
        https_config.sse.max_retries = 3;
        https_config.sse.keep_alive = true;
        https_config.sse.auto_reconnect = false;  // 服务端不需要自动重连
        https_config.sse.user_agent = "MCP-FileSystem-Server-HTTPS/1.0";
        
        // HTTP响应头配置（CORS支持）
        https_config.sse.headers["Content-Type"] = "application/json; charset=utf-8";
        https_config.sse.headers["Access-Control-Allow-Origin"] = "*";
        https_config.sse.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS";
        https_config.sse.headers["Access-Control-Allow-Headers"] = "Content-Type, Authorization, mcp-protocol-version, X-Session-Id";
        https_config.sse.headers["Access-Control-Expose-Headers"] = "X-Session-Id";
        
        // 支持流
        https_config.sse.headers["Accept"] = "application/json, text/event-stream";
       
        // 服务端元数据配置
        https_config.server.server_name = "MCP FileSystem Server HTTPS";
        https_config.server.server_version = "1.0.0";
        https_config.server.description = "MCP FileSystem Server with HTTPS transport";
        
        // HTTPS证书配置
        https_config.https.cert_file = cert_file;
        https_config.https.key_file = key_file;
        if (!ca_file.empty()) {
            https_config.https.ca_file = ca_file;
        }
        https_config.https.verify_peer = verify_peer;
        https_config.https.verify_hostname = verify_hostname;
        
        logger->info("🚀 启动文件系统服务器 (HTTPS模式)...");
        logger->info("📋 HTTPS传输配置:");
        logger->info("  - 传输类型: HTTPS");
        logger->info("  - 模式: SERVER");
        logger->info("  - 主机: " + host);
        logger->info("  - 端口: " + std::to_string(port));
        logger->info("  - 端点: " + endpoint);
        logger->info("  - 协议: HTTPS (SSL/TLS加密)");
        logger->info("  - 超时: " + std::to_string(https_config.sse.timeout_seconds) + "秒");
        logger->info("  - 最大重试: " + std::to_string(https_config.sse.max_retries));
        logger->info("  - Keep-Alive: " + std::string(https_config.sse.keep_alive ? "启用" : "禁用"));
        logger->info("  - 服务器名称: " + https_config.server.server_name);
        logger->info("  - 服务器版本: " + https_config.server.server_version);
        logger->info("  - 证书文件: " + cert_file);
        logger->info("  - 私钥文件: " + key_file);
        if (!ca_file.empty()) {
            logger->info("  - CA证书文件: " + ca_file);
        }
        
        // 注意：由于MCPServer的封装，我们需要在startWithConfig之后设置HTTPS配置
        // 但startWithConfig会创建transport，我们需要在创建后访问并配置
        // 这里我们通过直接创建HttpsTransport并配置，然后手动设置到MCPServer
        // 但由于MCPServer的封装，我们无法直接访问transport
        
        // 临时解决方案：通过TransportFactory创建HttpsTransport并配置，然后传递给MCPServer
        // 但MCPServer的startWithConfig会重新创建transport，所以我们需要另一种方法
        
        // 目前，我们通过修改MCPServer来支持在startWithConfig之后设置HTTPS配置
        // 或者我们可以通过扩展TransportConfig来传递证书信息
        
        // 由于当前MCPServer不支持直接设置HTTPS配置，我们需要：
        // 1. 修改MCPServer添加setHttpsConfig方法（需要修改接口）
        // 2. 或者通过TransportConfig传递证书信息（需要扩展TransportConfig）
        // 3. 或者通过NetConfig传递（但需要修改TransportBase的configureNetIO）
        
        // 临时解决方案：我们假设证书信息可以通过NetConfig传递
        // 但当前TransportConfig中没有直接的HTTPS配置字段
        
        // 注意：这里我们需要在startWithConfig之前设置证书信息
        // 但由于TransportConfig的限制，我们暂时无法直接传递证书信息
        // 需要在MCPServer中添加支持，或者扩展TransportConfig
        
        // 目前先启动服务器，证书配置需要在MCPServer内部处理
        // 或者通过修改MCPServer来支持HTTPS配置参数
        
        if (server.startWithConfig(https_config)) {
            logger->info("✅ 文件系统服务器启动成功 (HTTPS模式)");
            logger->info("✅ 文件系统服务器启动成功，正在运行...");
            logger->info("🌐 HTTPS服务地址: https://" + host + ":" + std::to_string(port) + endpoint);
            logger->info("💡 测试命令示例:");
            logger->info("   curl -k -X POST https://" + host + ":" + std::to_string(port) + endpoint + " \\");
            logger->info("     -H 'Content-Type: application/json' \\");
            logger->info("     -d '{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"tools/list\"}'");
            logger->info("   注意: -k 选项用于跳过SSL证书验证（仅用于测试自签名证书）");
            printUsage();
            
            // 运行服务器统计信息
            runServerStatistics(server);
            
            // 停止服务器
            logger->info("🛑 正在停止文件系统服务器...");
            server.stop();
            logger->info("✅ 文件系统服务器已停止");
            
        } else {
            logger->error("❌ 文件系统服务器启动失败 (HTTPS模式)");
            logger->error("💡 请检查:");
            logger->error("  1. 端口是否被占用: " + std::to_string(port));
            logger->error("  2. 证书文件是否存在: " + cert_file);
            logger->error("  3. 私钥文件是否存在: " + key_file);
            logger->error("  4. 证书和私钥是否匹配");
            logger->error("  5. OpenSSL库是否正确安装");
            std::cerr << "❌ 文件系统服务器启动失败" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        logger->error("❌ 服务器错误: " + std::string(e.what()));
        std::cerr << "❌ 服务器错误: " << e.what() << std::endl;
        return 1;
    }
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}

