/**
 * @file mcp_server.cpp
 * @brief MCP Server Implementation
 * 
 * This file provides the implementation of the MCP server functionality.
 */

#include "mcp_server.h"
#include "../../transport/include/transport_factory.h"
#include "../../transport/include/transport_base.h"
#include "../../transport/include/https_transport.h"
#include "../../session/include/session_manager.h"
#include <sstream>
#include <chrono>
#include <random>
#include <filesystem>
#include <fstream>
#include <optional>
#include <unordered_map>
#include <cstdlib>

// 日志记录器将在需要时局部声明

namespace mcp {

// Forward declaration
class MCPServerConnectionHandler;

// Session ID 验证函数
// 验证 session ID 是否符合格式要求（非空、长度合理、字符集正确）
static bool validateSessionId(const std::string& session_id) {
    // 检查是否为空
    if (session_id.empty()) {
        return false;
    }
    
    // Session ID 长度应该是 32 字符（根据 SessionManager 的实现）
    constexpr std::size_t kExpectedLength = 32;
    if (session_id.length() != kExpectedLength) {
        return false;
    }
    
    // 验证字符集：只允许字母和数字（根据 SessionManager 的实现）
    // 使用更高效的字符检查方法
    for (char c : session_id) {
        if (!((c >= '0' && c <= '9') || 
              (c >= 'A' && c <= 'Z') || 
              (c >= 'a' && c <= 'z'))) {
            return false;
        }
    }
    
    return true;
}

// MCP Server Request Handler for Transport
class MCPServerRequestHandler : public mcp_transport::TransportServerRequestHandler {
public:
    explicit MCPServerRequestHandler(MCPServer::Impl* server_impl) : server_impl_(server_impl) {}
    
    std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) override;
    
    void handleNotification(const std::string& method, const std::string& params) override;
    
private:
    MCPServer::Impl* server_impl_;
};

// MCP Server Implementation
class MCPServer::Impl {
public:
    Impl() : running_(false) {
        mcp_logger::getModuleLogger("mcp_server")->info("MCPServer::Impl created");
        // 使用工厂模式创建 transport，初始时使用默认配置
        mcp_transport::TransportConfig default_config;
        default_config.type = mcp_transport::TransportType::AUTO;
        default_config.mode = mcp_transport::TransportMode::SERVER;
        transport_ = mcp_transport::TransportFactory::createFromConfig(default_config);
        if (!transport_) {
            mcp_logger::getModuleLogger("mcp_server")->error("Failed to create transport using factory");
        }
        setupDefaultTools();
        setupDefaultResources();
    }
    
    virtual ~Impl() {
        if (running_) {
            stop();
        }
        mcp_logger::getModuleLogger("mcp_server")->info("MCPServer::Impl destroyed");
    }
    
    bool start(const std::string& endpoint) {
        mcp_logger::getModuleLogger("mcp_server")->info("Starting MCP server at: " + endpoint);
        
        // 创建transport配置
        mcp_transport::TransportConfig config;
        config.type = mcp_transport::TransportType::AUTO;
        config.mode = mcp_transport::TransportMode::SERVER;
        config.enable_logging = true;
        config.log_level = 1;
        
        return startWithConfig(config);
    }
    
    bool startWithConfig(const mcp_transport::TransportConfig& config) {
        mcp_logger::getModuleLogger("mcp_server")->info("Starting MCP server with custom config");
        
        // 使用工厂模式根据配置创建 transport
        transport_ = mcp_transport::TransportFactory::createFromConfig(config);
        if (!transport_) {
            mcp_logger::getModuleLogger("mcp_server")->error("Failed to create transport using factory with config");
            return false;
        }
        
        // 如果是HTTPS传输，设置HTTPS配置（证书等）
        // 证书信息现在可以通过TransportConfig.https传递
        if (config.type == mcp_transport::TransportType::HTTPS) {
            auto* https_transport = dynamic_cast<mcp_transport::HttpsTransport*>(transport_.get());
            if (https_transport) {
                // 从TransportConfig中提取HTTPS配置
                mcp_transport::HttpsTransport::HttpsConfig https_config;
                https_config.cert_file = config.https.cert_file;
                https_config.key_file = config.https.key_file;
                https_config.ca_file = config.https.ca_file;
                https_config.verify_peer = config.https.verify_peer;
                https_config.verify_hostname = config.https.verify_hostname;
                https_transport->setHttpsConfig(https_config);
            }
        }
        
        // 设置服务端请求处理器
        auto handler = std::make_shared<MCPServerRequestHandler>(this);
        transport_->setServerRequestHandler(std::static_pointer_cast<mcp_transport::TransportServerRequestHandler>(handler));
        
        // 检查是否为 HTTP 或 HTTPS 类型（用于 SSE 连接）
        // 注意：IPC 传输不使用 HTTP，因此不需要 CORS 预检（preflight）处理和 SSE 连接处理器
        if(config.type == mcp_transport::TransportType::HTTP || config.type == mcp_transport::TransportType::HTTPS) {
            // 为HTTP/HTTPS传输创建SseServerRequestHandler适配器
            class MCPServerRequestHandlerAdapter : public mcp_transport::SseServerRequestHandler {
            public:
                explicit MCPServerRequestHandlerAdapter(std::shared_ptr<MCPServerRequestHandler> handler, MCPServer::Impl* server_impl)
                    : handler_(handler), server_impl_(server_impl) {}
                
                bool handleRequest(const netio::Message& request, netio::Message& response) override {
                    if (!handler_) {
                        return false;
                    }
                    
                    auto logger = mcp_logger::getModuleLogger("mcp_server");
                    
                    // 初始化响应
                    response.type = netio::MessageType::RESPONSE;
                    response.id = request.id;
                    response.timestamp = std::chrono::system_clock::now();
                    response.status_code = 200;
                    response.headers["Content-Type"] = "application/json";
                    
                    try {
                        // 解析JSON-RPC请求
                        json request_json;
                        if (!request.body.empty()) {
                            request_json = json::parse(request.body);
                        } else {
                            logger->warning("Empty request body");
                            response.status_code = 400;
                            response.body = R"({"jsonrpc":"2.0","error":{"code":-32600,"message":"Invalid request: empty body"}})";
                            return true;
                        }
                        
                        // 打印收到的请求
                        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        logger->info("📥 收到请求:");
                        logger->info("  Method: " + request.method);
                        logger->info("  Path: " + request.path);
                        logger->info("  ID: " + request.id);
                        logger->info("  完整请求JSON:");
                        logger->info(request_json.dump(2));
                        logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        
                        // 提取method、params、id
                        std::string method = request_json.value("method", "");
                        std::string params = request_json.contains("params") ? request_json["params"].dump() : "{}";
                        std::string request_id = request_json.contains("id") 
                            ? (request_json["id"].is_string() ? request_json["id"].get<std::string>() : request_json["id"].dump())
                            : "";
                        
                        // 如果是 initialize 请求，生成 session_id 并在响应头中返回
                        if (method == methods::INITIALIZE && server_impl_) {
                            // 生成 session_id
                            std::unordered_map<std::string, std::string> session_attributes;
                            session_attributes["client_info"] = params; // 存储客户端信息
                            session_attributes["remote_address"] = request.headers.count("X-Forwarded-For") 
                                ? request.headers.at("X-Forwarded-For") 
                                : (request.headers.count("Remote-Addr") ? request.headers.at("Remote-Addr") : "");
                            
                            // 使用 SessionManager 创建 session
                            std::string session_id = server_impl_->createSession(session_attributes);
                            
                            // 将 session_id 添加到响应头
                            response.headers["X-Session-Id"] = session_id;
                            logger->info("✅ Generated session_id for initialize request: " + session_id);
                        }
                        
                        // 调用TransportServerRequestHandler处理请求
                        std::string result = handler_->handleRequest(method, params, request_id);
                        
                        // 设置响应体
                        response.body = result;
                        
                        // 打印发送的响应
                        try {
                            json response_json = json::parse(result);
                            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                            logger->info("📤 发送响应:");
                            logger->info("  Status Code: " + std::to_string(response.status_code));
                            logger->info("  ID: " + response.id);
                            logger->info("  完整响应JSON:");
                            logger->info(response_json.dump(2));
                            if (!response.headers.empty()) {
                                logger->info("  响应头:");
                                for (const auto& [key, value] : response.headers) {
                                    logger->info("    " + key + ": " + value);
                                }
                            }
                            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        } catch (const json::exception&) {
                            // 如果响应不是有效的JSON，直接打印原始内容
                            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                            logger->info("📤 发送响应:");
                            logger->info("  Status Code: " + std::to_string(response.status_code));
                            logger->info("  ID: " + response.id);
                            logger->info("  响应内容: " + result);
                            logger->info("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        }
                        
                        return true;
                        
                    } catch (const json::exception& e) {
                        logger->error("JSON parse error: " + std::string(e.what()));
                        response.status_code = 400;
                        response.body = R"({"jsonrpc":"2.0","error":{"code":-32700,"message":"Parse error"}})";
                        
                        // 打印错误响应
                        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        logger->error("📤 发送错误响应:");
                        logger->error("  Status Code: " + std::to_string(response.status_code));
                        logger->error("  错误内容: " + response.body);
                        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        
                        return true;
                    } catch (const std::exception& e) {
                        logger->error("Request handling error: " + std::string(e.what()));
                        response.status_code = 500;
                        json error_json;
                        error_json["jsonrpc"] = "2.0";
                        error_json["error"] = {
                            {"code", -32603},
                            {"message", std::string("Internal error: ") + e.what()}
                        };
                        response.body = error_json.dump();
                        
                        // 打印错误响应
                        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        logger->error("📤 发送错误响应:");
                        logger->error("  Status Code: " + std::to_string(response.status_code));
                        logger->error("  完整错误响应JSON:");
                        logger->error(error_json.dump(2));
                        logger->error("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
                        
                        return true;
                    }
                }
            private:
                std::shared_ptr<MCPServerRequestHandler> handler_;
                MCPServer::Impl* server_impl_;
            };
            
            auto sse_handler = std::make_shared<MCPServerRequestHandlerAdapter>(handler, this);
            transport_->setSseServerRequestHandler(sse_handler);
        }
        
        // 设置connection请求处理器
        auto connection_handler = std::make_shared<MCPServerConnectionHandler>(this);
        transport_->setConnectionRequestHandler(std::static_pointer_cast<mcp_transport::SseServerConnectHandler>(connection_handler));
        
        // 设置一下回调处理器
        transport_->setMessageCallback([](const mcp_transport::TransportMessage& message) {
            mcp_logger::getModuleLogger("mcp_server")->debug("Received message: " + message.content);
        });
        
        // 检查是否为 HTTP 或 HTTPS 类型（用于 SSE 连接）
        // 注意：IPC 传输不使用 HTTP，因此不需要 CORS 预检（preflight）处理和 SSE 连接回调
        if(config.type == mcp_transport::TransportType::HTTP || config.type == mcp_transport::TransportType::HTTPS) {
            transport_->setConnectionCallback([this](const mcp_transport::ConnectionInfo& info) {
                auto logger = mcp_logger::getModuleLogger("mcp_server");
                const std::string& session_id = info.netio_info.session_id;
                
                // 验证 session ID
                bool is_valid = validateSessionId(session_id);
                
                if (is_valid) {
                    logger->info("✅ Connection established with valid session ID: " + session_id);
                    logger->info("   Connection ID: " + info.netio_info.connection_id);
                    logger->info("   Remote: " + info.netio_info.remote_address + ":" + 
                                std::to_string(info.netio_info.remote_port));
                    
                    // 将 ConnectionInfo 存储到 SessionManager
                    try {
                        // 将 ConnectionInfo 序列化为 JSON 字符串
                        nlohmann::json connection_info_json = info.toJson();
                        std::string connection_info_str = connection_info_json.dump();
                        
                        // 创建 attributes，将 ConnectionInfo 作为 JSON 字符串存储
                        std::unordered_map<std::string, std::string> attributes;
                        attributes["connection_info"] = connection_info_str;
                        attributes["connection_id"] = info.netio_info.connection_id;
                        attributes["remote_address"] = info.netio_info.remote_address;
                        attributes["remote_port"] = std::to_string(info.netio_info.remote_port);
                        attributes["local_address"] = info.netio_info.local_address;
                        attributes["local_port"] = std::to_string(info.netio_info.local_port);
                        attributes["status"] = std::to_string(static_cast<int>(info.status));
                        attributes["type"] = std::to_string(static_cast<int>(info.type));
                        
                        // 创建 Session 对象
                        Session session;
                        session.id = session_id;
                        session.created_at = std::chrono::steady_clock::now();
                        session.expires_at = session.created_at + session_manager_.GetSessionTtl();
                        session.attributes = attributes;
                        
                        // 添加到 SessionManager
                        if (session_manager_.AddSession(session_id, session)) {
                            logger->info("📦 Session stored in SessionManager: " + session_id);
                        } else {
                            logger->warning("⚠️ Failed to store session in SessionManager: " + session_id);
                        }
                    } catch (const std::exception& e) {
                        logger->error("❌ Error storing session in SessionManager: " + std::string(e.what()));
                    }
                } else {
                    logger->warning("⚠️ Connection established with invalid or empty session ID");
                    logger->warning("   Connection ID: " + info.netio_info.connection_id);
                }
            });
        }
        
        // 启动transport
        if (!transport_->start(config)) {
            mcp_logger::getModuleLogger("mcp_server")->error("Failed to start transport");
            return false;
        }
        
        running_ = true;
        mcp_logger::getModuleLogger("mcp_server")->info("MCP server started successfully");
        return true;
    }
    
    void stop() {
        if (running_) {
            mcp_logger::getModuleLogger("mcp_server")->info("Stopping MCP server");
            running_ = false;
            transport_->stop();
            mcp_logger::getModuleLogger("mcp_server")->info("MCP server stopped");
        }
    }
    
    bool isRunning() const {
        return running_;
    }
    
    InitializeResult initialize(const InitializeParams& params) {
        mcp_logger::getModuleLogger("mcp_server")->info("Handling initialize request from client: " + params.clientInfo.name);
        
        // Validate protocol version
        if (!utils::isValidProtocolVersion(params.protocolVersion)) {
            throw mcp_exception(error_code::invalid_params, "Unsupported protocol version: " + params.protocolVersion);
        }
        
        // Store client info
        client_info_ = params.clientInfo;
        client_capabilities_ = params.capabilities;
        
        // Create initialize result
        // Return the protocol version requested by the client if it's supported
        InitializeResult result;
        result.protocolVersion = params.protocolVersion; // Return client's requested version (validated above)
        result.capabilities = server_capabilities_;
        result.serverInfo = server_info_;
        
        mcp_logger::getModuleLogger("mcp_server")->info("Initialize completed successfully");
        return result;
    }
    
    void initialized() {
        mcp_logger::getModuleLogger("mcp_server")->info("Client initialized notification received");
        // TODO: Handle client initialized notification
    }
    
    json ping() {
        mcp_logger::getModuleLogger("mcp_server")->debug("Handling ping request");
        return json::object();
    }
    
    std::vector<Tool> listTools() {
        mcp_logger::getModuleLogger("mcp_server")->debug("Listing tools");
        return tools_;
    }
    
    ToolCallResult callTool(const ToolCallParams& params) {
        mcp_logger::getModuleLogger("mcp_server")->debug("Tool call requested: " + params.name);
        
        ToolCallResult result;
        
        try {
            // 如果设置了自定义工具调用处理器，优先使用它
            if (tool_call_handler_) {
                return tool_call_handler_(params);
            }
            
            // 否则使用默认实现
            if (params.name == "read_file") {
                std::string path = params.arguments.value("path", "");
                if (path.empty()) {
                    result.isError = true;
                    result.errorMessage = "Path parameter is required";
                    return result;
                }
                
                std::ifstream file(path);
                if (!file.is_open()) {
                    result.isError = true;
                    result.errorMessage = "Cannot open file: " + path;
                    return result;
                }
                
                std::string content((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
                
                result.content = json{{"content", content}, {"path", path}};
                return result;
            }
            else if (params.name == "write_file") {
                std::string path = params.arguments.value("path", "");
                std::string content = params.arguments.value("content", "");
                
                if (path.empty()) {
                    result.isError = true;
                    result.errorMessage = "Path parameter is required";
                    return result;
                }
                
                std::ofstream file(path);
                if (!file.is_open()) {
                    result.isError = true;
                    result.errorMessage = "Cannot create file: " + path;
                    return result;
                }
                
                file << content;
                file.close();
                
                result.content = json{{"message", "File written successfully"}, {"path", path}};
                return result;
            }
            else if (params.name == "list_directory") {
                std::string path = params.arguments.value("path", "");
                if (path.empty()) {
                    path = ".";
                }
                
                std::filesystem::path dir_path(path);
                if (!std::filesystem::exists(dir_path)) {
                    result.isError = true;
                    result.errorMessage = "Directory does not exist: " + path;
                    return result;
                }
                
                if (!std::filesystem::is_directory(dir_path)) {
                    result.isError = true;
                    result.errorMessage = "Path is not a directory: " + path;
                    return result;
                }
                
                json files = json::array();
                for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
                    json file_info;
                    file_info["name"] = entry.path().filename().string();
                    file_info["path"] = entry.path().string();
                    file_info["is_directory"] = entry.is_directory();
                    file_info["size"] = entry.is_regular_file() ? entry.file_size() : 0;
                    files.push_back(file_info);
                }
                
                result.content = json{{"files", files}, {"path", path}};
                return result;
            }
            else if (params.name == "create_directory") {
                std::string path = params.arguments.value("path", "");
                if (path.empty()) {
                    result.isError = true;
                    result.errorMessage = "Path parameter is required";
                    return result;
                }
                
                std::filesystem::path dir_path(path);
                if (std::filesystem::exists(dir_path)) {
                    result.isError = true;
                    result.errorMessage = "Directory already exists: " + path;
                    return result;
                }
                
                if (!std::filesystem::create_directories(dir_path)) {
                    result.isError = true;
                    result.errorMessage = "Failed to create directory: " + path;
                    return result;
                }
                
                result.content = json{{"message", "Directory created successfully"}, {"path", path}};
                return result;
            }
            else {
                result.isError = true;
                result.errorMessage = "Unknown tool: " + params.name;
                mcp_logger::getModuleLogger("mcp_server")->error("Tool call failed: " + result.errorMessage);
                return result;
            }
        } catch (const std::exception& e) {
            result.isError = true;
            result.errorMessage = "Tool execution error: " + std::string(e.what());
            mcp_logger::getModuleLogger("mcp_server")->error("Tool call failed: " + result.errorMessage);
            return result;
        }
    }
    
    std::vector<Resource> listResources() {
        mcp_logger::getModuleLogger("mcp_server")->debug("Listing resources");
        return resources_;
    }
    
    json readResource(const std::string& uri) {
        mcp_logger::getModuleLogger("mcp_server")->debug("Reading resource: " + uri);
        
        try {
            if (uri.substr(0, 7) == "file://") {
                std::string path = uri.substr(7); // Remove "file://" prefix
                if (path.empty() || path == ".") {
                    path = ".";
                }
                
                std::filesystem::path fs_path(path);
                if (!std::filesystem::exists(fs_path)) {
                    throw mcp_exception(error_code::internal_error, "Resource not found: " + uri);
                }
                
                if (std::filesystem::is_directory(fs_path)) {
                    // Return directory listing as JSON
                    json result;
                    result["type"] = "directory";
                    result["path"] = path;
                    result["uri"] = uri;
                    
                    json files = json::array();
                    for (const auto& entry : std::filesystem::directory_iterator(fs_path)) {
                        json file_info;
                        file_info["name"] = entry.path().filename().string();
                        file_info["path"] = entry.path().string();
                        file_info["is_directory"] = entry.is_directory();
                        file_info["size"] = entry.is_regular_file() ? entry.file_size() : 0;
                        files.push_back(file_info);
                    }
                    result["files"] = files;
                    return result;
                } else if (std::filesystem::is_regular_file(fs_path)) {
                    // Return file content
                    std::ifstream file(path);
                    if (!file.is_open()) {
                        throw mcp_exception(error_code::internal_error, "Cannot open file: " + path);
                    }
                    
                    std::string content((std::istreambuf_iterator<char>(file)),
                                       std::istreambuf_iterator<char>());
                    
                    json result;
                    result["type"] = "file";
                    result["path"] = path;
                    result["uri"] = uri;
                    result["content"] = content;
                    result["size"] = content.size();
                    return result;
                } else {
                    throw mcp_exception(error_code::internal_error, "Unsupported resource type: " + uri);
                }
            } else if (resource_read_handler_) {
                return resource_read_handler_(uri);
            } else {
                throw mcp_exception(error_code::internal_error, "No resource read handler registered");
            }
        } catch (const mcp_exception&) {
            throw; // Re-throw MCP exceptions
        } catch (const std::exception& e) {
            throw mcp_exception(error_code::internal_error, "Resource read error: " + std::string(e.what()));
        }
    }
    
    std::vector<Prompt> listPrompts() {
        mcp_logger::getModuleLogger("mcp_server")->debug("Listing prompts");
        return prompts_;
    }
    
    json getPrompt(const std::string& name, const json& arguments) {
        mcp_logger::getModuleLogger("mcp_server")->debug("Getting prompt: " + name);
        
        if (prompt_get_handler_) {
            return prompt_get_handler_(name, arguments);
        } else {
            throw mcp_exception(error_code::internal_error, "No prompt get handler registered");
        }
    }
    
    void sendNotification(const std::string& method, const json& params) {
        (void)params; // Suppress unused parameter warning
        mcp_logger::getModuleLogger("mcp_server")->debug("Sending notification: " + method);
        // TODO: Implement actual notification sending logic
    }
    
    void setNotificationHandler(const std::string& method, NotificationHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        notification_handlers_[method] = handler;
        mcp_logger::getModuleLogger("mcp_server")->debug("Set notification handler for: " + method);
    }
    
    void setToolCallHandler(ToolCallHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        tool_call_handler_ = handler;
        mcp_logger::getModuleLogger("mcp_server")->debug("Set tool call handler");
    }
    
    void setResourceReadHandler(ResourceReadHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        resource_read_handler_ = handler;
        mcp_logger::getModuleLogger("mcp_server")->debug("Set resource read handler");
    }
    
    void setPromptGetHandler(PromptGetHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        prompt_get_handler_ = handler;
        mcp_logger::getModuleLogger("mcp_server")->debug("Set prompt get handler");
    }
    
    void setServerInfo(const std::string& name, const std::string& version) {
        server_info_.name = name;
        server_info_.version = version;
        mcp_logger::getModuleLogger("mcp_server")->debug("Set server info: " + name + " v" + version);
    }
    
    void setCapabilities(const ServerCapabilities& capabilities) {
        server_capabilities_ = capabilities;
        mcp_logger::getModuleLogger("mcp_server")->debug("Set server capabilities");
    }
    
    void addTool(const Tool& tool) {
        std::lock_guard<std::mutex> lock(tools_mutex_);
        tools_.push_back(tool);
        mcp_logger::getModuleLogger("mcp_server")->debug("Added tool: " + tool.name);
    }
    
    void removeTool(const std::string& name) {
        std::lock_guard<std::mutex> lock(tools_mutex_);
        tools_.erase(
            std::remove_if(tools_.begin(), tools_.end(),
                [&name](const Tool& tool) { return tool.name == name; }),
            tools_.end());
        mcp_logger::getModuleLogger("mcp_server")->debug("Removed tool: " + name);
    }
    
    void clearTools() {
        std::lock_guard<std::mutex> lock(tools_mutex_);
        tools_.clear();
        mcp_logger::getModuleLogger("mcp_server")->debug("Cleared all tools");
    }
    
    void addResource(const Resource& resource) {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        resources_.push_back(resource);
        mcp_logger::getModuleLogger("mcp_server")->debug("Added resource: " + resource.uri);
    }
    
    void removeResource(const std::string& uri) {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        resources_.erase(
            std::remove_if(resources_.begin(), resources_.end(),
                [&uri](const Resource& resource) { return resource.uri == uri; }),
            resources_.end());
        mcp_logger::getModuleLogger("mcp_server")->debug("Removed resource: " + uri);
    }
    
    void clearResources() {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        resources_.clear();
        mcp_logger::getModuleLogger("mcp_server")->debug("Cleared all resources");
    }
    
    void addPrompt(const Prompt& prompt) {
        std::lock_guard<std::mutex> lock(prompts_mutex_);
        prompts_.push_back(prompt);
        mcp_logger::getModuleLogger("mcp_server")->debug("Added prompt: " + prompt.name);
    }
    
    void removePrompt(const std::string& name) {
        std::lock_guard<std::mutex> lock(prompts_mutex_);
        prompts_.erase(
            std::remove_if(prompts_.begin(), prompts_.end(),
                [&name](const Prompt& prompt) { return prompt.name == name; }),
            prompts_.end());
        mcp_logger::getModuleLogger("mcp_server")->debug("Removed prompt: " + name);
    }
    
    void clearPrompts() {
        std::lock_guard<std::mutex> lock(prompts_mutex_);
        prompts_.clear();
        mcp_logger::getModuleLogger("mcp_server")->debug("Cleared all prompts");
    }
    
    void notifyResourcesChanged() {
        mcp_logger::getModuleLogger("mcp_server")->debug("Notifying resources changed");
        sendNotification(methods::NOTIFICATION_RESOURCES_LIST_CHANGED, json::object());
    }
    
    void notifyToolsChanged() {
        mcp_logger::getModuleLogger("mcp_server")->debug("Notifying tools changed");
        sendNotification(methods::NOTIFICATION_TOOLS_LIST_CHANGED, json::object());
    }
    
    void notifyPromptsChanged() {
        mcp_logger::getModuleLogger("mcp_server")->debug("Notifying prompts changed");
        sendNotification(methods::NOTIFICATION_PROMPTS_LIST_CHANGED, json::object());
    }
    
    // 设置默认工具 - 子类可以重写此方法
    virtual void setupDefaultTools() {
        // 基类不提供默认工具，由子类实现
    }
    
    // 设置默认资源 - 子类可以重写此方法
    virtual void setupDefaultResources() {
        // 基类不提供默认资源，由子类实现
    }
    
    // 根据 session_id 获取 ConnectionInfo
    std::optional<mcp_transport::ConnectionInfo> getConnectionInfoBySessionId(const std::string& session_id) {
        try {
            auto attributes_opt = session_manager_.GetSessionAttributes(session_id);
            if (!attributes_opt.has_value()) {
                return std::nullopt;
            }
            
            const auto& attributes = attributes_opt.value();
            auto it = attributes.find("connection_info");
            if (it == attributes.end()) {
                return std::nullopt;
            }
            
            // 从 JSON 字符串反序列化 ConnectionInfo
            nlohmann::json connection_info_json = nlohmann::json::parse(it->second);
            // 注意：这里需要根据实际的 ConnectionInfo 结构来实现反序列化
            // 由于 ConnectionInfo 可能没有 fromJson 方法，我们可以手动构建
            mcp_transport::ConnectionInfo info;
            if (connection_info_json.contains("netio_info")) {
                auto& netio_info_json = connection_info_json["netio_info"];
                if (netio_info_json.contains("connection_id")) {
                    info.netio_info.connection_id = netio_info_json["connection_id"].get<std::string>();
                }
                if (netio_info_json.contains("remote_address")) {
                    info.netio_info.remote_address = netio_info_json["remote_address"].get<std::string>();
                }
                if (netio_info_json.contains("remote_port")) {
                    info.netio_info.remote_port = netio_info_json["remote_port"].get<int>();
                }
                if (netio_info_json.contains("local_address")) {
                    info.netio_info.local_address = netio_info_json["local_address"].get<std::string>();
                }
                if (netio_info_json.contains("local_port")) {
                    info.netio_info.local_port = netio_info_json["local_port"].get<int>();
                }
                if (netio_info_json.contains("session_id")) {
                    info.netio_info.session_id = netio_info_json["session_id"].get<std::string>();
                }
            }
            if (connection_info_json.contains("status")) {
                info.status = static_cast<mcp_transport::TransportStatus>(connection_info_json["status"].get<int>());
            }
            if (connection_info_json.contains("type")) {
                info.type = static_cast<mcp_transport::TransportType>(connection_info_json["type"].get<int>());
            }
            
            return std::make_optional(info);
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("mcp_server")->error("Error retrieving ConnectionInfo from SessionManager: " + std::string(e.what()));
            return std::nullopt;
        }
    }
    
    // 创建 session
    std::string createSession(const std::unordered_map<std::string, std::string>& attributes = {}) {
        return session_manager_.CreateSession(attributes);
    }
    
    // 验证 session_id 是否有效
    bool validateSession(const std::string& session_id) {
        return session_manager_.ValidateSession(session_id);
    }
    
    // 移除 session
    bool removeSession(const std::string& session_id) {
        return session_manager_.RemoveSession(session_id);
    }
    
    // Getter for server_info_
    const ServerInfo& getServerInfo() const {
        return server_info_;
    }
    
    // Getter for server_capabilities_
    const ServerCapabilities& getServerCapabilities() const {
        return server_capabilities_;
    }
    
    // 设置是否禁用 session 验证
    void setDisableSessionValidation(bool disable) {
        disable_session_validation_ = disable;
    }
    
    // 检查是否禁用 session 验证
    bool isSessionValidationDisabled() const {
        return disable_session_validation_;
    }
    
    // 获取 transport 实际运行类型（如果已启动）或配置类型
    mcp_transport::TransportType getTransportType() const {
        if (transport_) {
            // 优先使用 ConnectionInfo 中的实际类型（AUTO 已被解析）
            auto conn_info = transport_->getConnectionInfo();
            if (conn_info.type != mcp_transport::TransportType::AUTO) {
                return conn_info.type;
            }
            // 如果 ConnectionInfo 类型是 AUTO，使用配置类型
            // 如果配置类型也是 AUTO，检查 IPC 配置来判断
            auto config = transport_->getConfig();
            if (config.type == mcp_transport::TransportType::AUTO) {
                // AUTO 类型：如果有 IPC 配置，优先使用 IPC
                if (!config.ipc.command.empty()) {
                    return mcp_transport::TransportType::IPC;
                } else if (!config.sse.host.empty()) {
                    // 根据协议类型返回 HTTP 或 HTTPS
                    return (config.sse.protocol == netio::Protocol::HTTPS) 
                           ? mcp_transport::TransportType::HTTPS 
                           : mcp_transport::TransportType::HTTP;
                }
            }
            return config.type;
        }
        return mcp_transport::TransportType::AUTO;
    }

private:
    bool running_;
    ServerInfo server_info_;
    ServerCapabilities server_capabilities_;
    ClientInfo client_info_;
    ClientCapabilities client_capabilities_;
    
    std::unique_ptr<mcp_transport::TransportBase> transport_;
    
    // Session 管理
    SessionManager session_manager_;
    bool disable_session_validation_ = false;  // 是否禁用 session 验证
    
    std::mutex tools_mutex_;
    std::vector<Tool> tools_;
    
    std::mutex resources_mutex_;
    std::vector<Resource> resources_;
    
    std::mutex prompts_mutex_;
    std::vector<Prompt> prompts_;
    
    std::mutex handlers_mutex_;
    std::map<std::string, NotificationHandler> notification_handlers_;
    ToolCallHandler tool_call_handler_;
    ResourceReadHandler resource_read_handler_;
    PromptGetHandler prompt_get_handler_;
};

// MCP Server Request Handler implementation
std::string MCPServerRequestHandler::handleRequest(const std::string& method, const std::string& params, const std::string& request_id) {
    auto logger = mcp_logger::getModuleLogger("mcp_server");
    logger->debug("Handling request: " + method + " (ID: " + request_id + ")");
    
    // Session 验证：从 params 或 request_id 中提取 session_id
    std::string session_id;
    try {
        // 尝试从 params 中提取 session_id
        json params_json = json::parse(params);
        if (params_json.contains("session_id")) {
            session_id = params_json["session_id"].get<std::string>();
        } else if (validateSessionId(request_id)) {
            // 如果 request_id 本身是有效的 session_id，使用它
            session_id = request_id;
        }
    } catch (const std::exception&) {
        // 如果解析失败，尝试使用 request_id
        if (validateSessionId(request_id)) {
            session_id = request_id;
        }
    }
    
    // IPC 传输不需要 session 验证和 CORS 预检，因为它是本地进程间通信
    // 如果禁用了 session 验证，也跳过验证
    bool is_ipc = (server_impl_->getTransportType() == mcp_transport::TransportType::IPC);
    bool session_validation_disabled = server_impl_->isSessionValidationDisabled();
    if (!is_ipc && !session_validation_disabled) {
        // 验证 session（除了 initialize 方法，其他方法都需要验证）
        // 注意：只有 HTTP/HTTPS 传输才需要 session 验证和 CORS 处理
        if (method != methods::INITIALIZE && !session_id.empty()) {
            if (!server_impl_->validateSession(session_id)) {
                logger->warning("⚠️ Invalid or expired session ID: " + session_id);
                return json{{"jsonrpc", "2.0"}, 
                           {"id", request_id},
                           {"error", {{"code", -32001}, {"message", "Invalid or expired session"}}}}.dump();
            }
            logger->debug("✅ Session validated: " + session_id);
        } else if (method != methods::INITIALIZE && session_id.empty()) {
            logger->warning("⚠️ Missing session ID for method: " + method);
            return json{{"jsonrpc", "2.0"}, 
                       {"id", request_id},
                       {"error", {{"code", -32001}, {"message", "Session ID required"}}}}.dump();
        }
    } else if (session_validation_disabled) {
        logger->debug("⚠️ Session validation is disabled, skipping validation");
    }
    
    try {
        if (method == methods::INITIALIZE) {
            auto init_params = InitializeParams::fromJson(json::parse(params));
            auto result = server_impl_->initialize(init_params);
            if (is_ipc) {
                return result.toJson().dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            // Parse request_id - preserve original type (number, string, or null)
            // Since request_id was extracted as string, try to parse back to number if numeric
            if (request_id.empty()) {
                // null id is valid in JSON-RPC 2.0 for notifications, but requests should have id
                response["id"] = nullptr;
            } else {
                // Try to parse as integer if it's numeric, otherwise keep as string
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        // Successfully parsed as integer
                        response["id"] = static_cast<int>(num);
                    } else {
                        // Not a pure integer, keep as string
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result.toJson();
            return response.dump();
        }
        else if (method == methods::PING) {
            auto result = server_impl_->ping();
            if (is_ipc) {
                return result.dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        response["id"] = static_cast<int>(num);
                    } else {
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result;
            return response.dump();
        }
        else if (method == methods::TOOLS_LIST) {
            auto tools = server_impl_->listTools();
            json result;
            result["tools"] = json::array();
            for (const auto& tool : tools) {
                result["tools"].push_back(tool.toJson());
            }
            if (is_ipc) {
                return result.dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        response["id"] = static_cast<int>(num);
                    } else {
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result;
            return response.dump();
        }
        else if (method == methods::TOOLS_CALL) {
            auto tool_params = ToolCallParams::fromJson(json::parse(params));
            auto result = server_impl_->callTool(tool_params);
            if (is_ipc) {
                return result.toJson().dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        response["id"] = static_cast<int>(num);
                    } else {
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result.toJson();
            return response.dump();
        }
        else if (method == methods::RESOURCES_LIST) {
            auto resources = server_impl_->listResources();
            json result;
            result["resources"] = json::array();
            for (const auto& resource : resources) {
                result["resources"].push_back(resource.toJson());
            }
            if (is_ipc) {
                return result.dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        response["id"] = static_cast<int>(num);
                    } else {
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result;
            return response.dump();
        }
        else if (method == methods::RESOURCES_READ) {
            auto resource_params = json::parse(params);
            std::string uri = resource_params.value("uri", "");
            auto result = server_impl_->readResource(uri);
            if (is_ipc) {
                return result.dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        response["id"] = static_cast<int>(num);
                    } else {
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result;
            return response.dump();
        }
        else if (method == methods::PROMPTS_LIST) {
            auto prompts = server_impl_->listPrompts();
            json result;
            result["prompts"] = json::array();
            for (const auto& prompt : prompts) {
                result["prompts"].push_back(prompt.toJson());
            }
            if (is_ipc) {
                return result.dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        response["id"] = static_cast<int>(num);
                    } else {
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result;
            return response.dump();
        }
        else if (method == methods::PROMPTS_GET) {
            auto prompt_params = json::parse(params);
            std::string name = prompt_params.value("name", "");
            json arguments = prompt_params.value("arguments", json::object());
            auto result = server_impl_->getPrompt(name, arguments);
            if (is_ipc) {
                return result.dump();
            }
            // Wrap in JSON-RPC 2.0 response format
            json response;
            response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        response["id"] = static_cast<int>(num);
                    } else {
                        response["id"] = request_id;
                    }
                } catch (...) {
                    response["id"] = request_id;
                }
            }
            response["result"] = result;
            return response.dump();
        }
        else {
            // Unknown method - return JSON-RPC 2.0 error response
            json error_response;
            error_response["jsonrpc"] = "2.0";
            if (request_id.empty()) {
                error_response["id"] = nullptr;
            } else {
                try {
                    char* end;
                    long num = std::strtol(request_id.c_str(), &end, 10);
                    if (*end == '\0' && request_id[0] != '\0') {
                        error_response["id"] = static_cast<int>(num);
                    } else {
                        error_response["id"] = request_id;
                    }
                } catch (...) {
                    error_response["id"] = request_id;
                }
            }
            error_response["error"] = {
                {"code", static_cast<int>(error_code::method_not_found)},
                {"message", "Unknown method: " + method}
            };
            return error_response.dump();
        }
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("mcp_server")->error("Request handling error: " + std::string(e.what()));
        // Return JSON-RPC 2.0 error response
        json error_response;
        error_response["jsonrpc"] = "2.0";
        if (request_id.empty()) {
            error_response["id"] = nullptr;
        } else {
            try {
                char* end;
                long num = std::strtol(request_id.c_str(), &end, 10);
                if (*end == '\0' && request_id[0] != '\0') {
                    error_response["id"] = static_cast<int>(num);
                } else {
                    error_response["id"] = request_id;
                }
            } catch (...) {
                error_response["id"] = request_id;
            }
        }
        error_response["error"] = {
            {"code", static_cast<int>(error_code::internal_error)},
            {"message", "Internal error: " + std::string(e.what())}
        };
        return error_response.dump();
    }
}

void MCPServerRequestHandler::handleNotification(const std::string& method, const std::string& params) {
    auto logger = mcp_logger::getModuleLogger("mcp_server");
    logger->info("Handling notification: " + method);
    
    // Session 验证：从 params 中提取 session_id
    std::string session_id;
    try {
        json params_json = json::parse(params);
        if (params_json.contains("session_id")) {
            session_id = params_json["session_id"].get<std::string>();
        }
    } catch (const std::exception&) {
        // 解析失败，继续处理（某些通知可能不需要 session_id）
    }
    
    // 验证 session（除了 initialized 通知，其他通知都需要验证）
    // 如果配置类型是 IPC，跳过 session 验证（IPC 传输不需要 session 和 CORS 预检）
    // 如果禁用了 session 验证，也跳过验证
    bool is_ipc = (server_impl_->getTransportType() == mcp_transport::TransportType::IPC);
    bool session_validation_disabled = server_impl_->isSessionValidationDisabled();
    if (!is_ipc && !session_validation_disabled) {
        if (method != methods::NOTIFICATION_INITIALIZED && !session_id.empty()) {
            if (!server_impl_->validateSession(session_id)) {
                logger->warning("⚠️ Invalid or expired session ID for notification: " + session_id);
                return; // 通知失败时不返回错误，只记录日志
            }
            logger->debug("✅ Session validated for notification: " + session_id);
        } else if (method != methods::NOTIFICATION_INITIALIZED && session_id.empty()) {
            logger->warning("⚠️ Missing session ID for notification: " + method);
            return; // 通知失败时不返回错误，只记录日志
        }
    } else if (session_validation_disabled) {
        logger->debug("⚠️ Session validation is disabled, skipping validation for notification");
    }
    
    try {
        if (method == methods::NOTIFICATION_INITIALIZED) {
            server_impl_->initialized();
        }
        else {
            // 处理其他通知
            json params_json = json::parse(params);
            // 这里可以添加更多通知处理逻辑
        }
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("mcp_server")->error("Notification handling error: " + std::string(e.what()));
    }
}

// MCP Server Connection Handler for Transport (SSE connections)
class MCPServerConnectionHandler : public mcp_transport::SseServerConnectHandler {
public:
    explicit MCPServerConnectionHandler(MCPServer::Impl* server_impl) : server_impl_(server_impl) {}
    
    bool handleRequest(const netio::Message& request, netio::Message& response) override {
        auto logger = mcp_logger::getModuleLogger("mcp_server");
        
        try {
            logger->debug("🔌 Handling SSE connection request: method=" + request.method + ", path=" + request.path);
            
            // 初始化响应
            response = netio::Message{};
            response.type = netio::MessageType::RESPONSE;
            response.id = request.id;
            response.timestamp = std::chrono::system_clock::now();
            response.status_code = 200;
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.headers["Cache-Control"] = "no-cache";
            response.headers["Connection"] = "keep-alive";
            
            // 记录请求信息
            if (!request.body.empty()) {
                const std::size_t preview_len = std::min<std::size_t>(request.body.size(), 256);
                logger->debug("📦 Connection request body preview: " + 
                             request.body.substr(0, preview_len) +
                             (request.body.size() > preview_len ? "...(truncated)" : ""));
            }
            
            // 检查服务器是否正在运行
            if (!server_impl_ || !server_impl_->isRunning()) {
                logger->warning("⚠️ Server is not running, rejecting connection");
                response.status_code = 503;
                nlohmann::json error_json;
                error_json["jsonrpc"] = "2.0";
                error_json["error"] = {
                    {"code", -32000},
                    {"message", "Server not available"}
                };
                response.body = error_json.dump();
                return true;
            }
            
            // 验证请求路径（如果需要）
            // 这里可以根据需要添加路径验证逻辑
            if (!request.path.empty()) {
                logger->debug("📍 Connection path: " + request.path);
            }
            
            // 检查请求头中是否包含 session ID（客户端可能已经有一个 session ID）
            std::string client_session_id;
            for (const auto& header : request.headers) {
                if (header.first == "X-Session-Id" || header.first == "x-session-id") {
                    client_session_id = header.second;
                    break;
                }
            }
            
            // 如果客户端提供了 session ID，验证它
            if (!client_session_id.empty()) {
                if (validateSessionId(client_session_id)) {
                    logger->info("📋 Client provided valid session ID: " + client_session_id);
                } else {
                    logger->warning("⚠️ Client provided invalid session ID format, will generate new one");
                    client_session_id.clear(); // 清空无效的 session ID，让 transport 层生成新的
                }
            }
            
            // 创建成功响应 - 使用符合MCP协议规范的InitializeResult格式
            // 获取服务器的能力和信息
            ServerInfo server_info = server_impl_->getServerInfo();
            ServerCapabilities server_capabilities = server_impl_->getServerCapabilities();
            
            // 创建符合MCP协议规范的响应
            InitializeResult init_result;
            init_result.protocolVersion = PROTOCOL_VERSION_2024_11_05; // 使用 2024-11-05 协议版本
            init_result.capabilities = server_capabilities;
            init_result.serverInfo = server_info;
            
            // 转换为JSON格式
            nlohmann::json result = init_result.toJson();
            
            // 注意：session_id 会在 transport 层生成后通过响应头 X-Session-Id 返回
            // 如果客户端提供了有效的 session_id，这里可以记录它
            if (!client_session_id.empty()) {
                result["session_id"] = client_session_id; // 返回客户端提供的 session ID
            }
            // 如果客户端没有提供或提供的是无效的，transport 层会生成新的并在响应头中返回
            
            // 使用JSON-RPC格式包装响应，符合MCP协议规范
            nlohmann::json response_json;
            response_json["jsonrpc"] = "2.0";
            response_json["result"] = result;
            // 连接请求通常不需要id字段，但如果请求中有id，应该返回
            if (!request.id.empty()) {
                response_json["id"] = request.id;
            }
            
            // 确保响应体是有效的JSON字符串
            // 注意：不要手动设置Content-Length，httplib的set_content会自动设置
            try {
                response.body = response_json.dump();
            } catch (const std::exception& e) {
                logger->error("❌ Failed to serialize response JSON: " + std::string(e.what()));
                response.status_code = 500;
                nlohmann::json error_json;
                error_json["jsonrpc"] = "2.0";
                error_json["error"] = {
                    {"code", -32603},
                    {"message", "Internal error: Failed to serialize response"}
                };
                response.body = error_json.dump();
                return true;
            }
            
            logger->info("✅ SSE connection accepted for path: " + request.path);
            return true;
            
        } catch (const std::exception& e) {
            logger->error("❌ Exception in handleRequest: " + std::string(e.what()));
            // 确保即使发生异常也返回有效的响应
            response = netio::Message{};
            response.type = netio::MessageType::RESPONSE;
            response.id = request.id;
            response.timestamp = std::chrono::system_clock::now();
            response.status_code = 500;
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.headers["Connection"] = "close";
            
            nlohmann::json error_json;
            error_json["jsonrpc"] = "2.0";
            error_json["error"] = {
                {"code", -32603},
                {"message", std::string("Internal error: ") + e.what()}
            };
            response.body = error_json.dump();
            // 注意：不要手动设置Content-Length，httplib会自动设置
            return true;
        } catch (...) {
            logger->error("❌ Unknown exception in handleRequest");
            // 确保即使发生未知异常也返回有效的响应
            response = netio::Message{};
            response.type = netio::MessageType::RESPONSE;
            response.id = request.id;
            response.timestamp = std::chrono::system_clock::now();
            response.status_code = 500;
            response.headers["Content-Type"] = "application/json; charset=utf-8";
            response.headers["Connection"] = "close";
            
            nlohmann::json error_json;
            error_json["jsonrpc"] = "2.0";
            error_json["error"] = {
                {"code", -32603},
                {"message", "Internal error: Unknown exception"}
            };
            response.body = error_json.dump();
            // 注意：不要手动设置Content-Length，httplib会自动设置
            return true;
        }
    }
    
private:
    MCPServer::Impl* server_impl_;
};

// MCPServer public interface
MCPServer::MCPServer() : impl_(std::make_unique<Impl>()) {
    mcp_logger::getModuleLogger("mcp_server")->debug("MCPServer created");
}

MCPServer::~MCPServer() {
    mcp_logger::getModuleLogger("mcp_server")->debug("MCPServer destroyed");
}

bool MCPServer::start(const std::string& endpoint) {
    return impl_->start(endpoint);
}

bool MCPServer::startWithConfig(const mcp_transport::TransportConfig& config) {
    return impl_->startWithConfig(config);
}

void MCPServer::stop() {
    impl_->stop();
}

bool MCPServer::isRunning() const {
    return impl_->isRunning();
}

InitializeResult MCPServer::initialize(const InitializeParams& params) {
    return impl_->initialize(params);
}

void MCPServer::initialized() {
    impl_->initialized();
}

json MCPServer::ping() {
    return impl_->ping();
}

std::vector<Tool> MCPServer::listTools() {
    return impl_->listTools();
}

ToolCallResult MCPServer::callTool(const ToolCallParams& params) {
    return impl_->callTool(params);
}

std::vector<Resource> MCPServer::listResources() {
    return impl_->listResources();
}

json MCPServer::readResource(const std::string& uri) {
    return impl_->readResource(uri);
}

std::vector<Prompt> MCPServer::listPrompts() {
    return impl_->listPrompts();
}

json MCPServer::getPrompt(const std::string& name, const json& arguments) {
    return impl_->getPrompt(name, arguments);
}

void MCPServer::sendNotification(const std::string& method, const json& params) {
    impl_->sendNotification(method, params);
}

void MCPServer::setNotificationHandler(const std::string& method, NotificationHandler handler) {
    impl_->setNotificationHandler(method, handler);
}

void MCPServer::setToolCallHandler(ToolCallHandler handler) {
    impl_->setToolCallHandler(handler);
}

void MCPServer::setResourceReadHandler(ResourceReadHandler handler) {
    impl_->setResourceReadHandler(handler);
}

void MCPServer::setPromptGetHandler(PromptGetHandler handler) {
    impl_->setPromptGetHandler(handler);
}

void MCPServer::setServerInfo(const std::string& name, const std::string& version) {
    impl_->setServerInfo(name, version);
}

void MCPServer::setCapabilities(const ServerCapabilities& capabilities) {
    impl_->setCapabilities(capabilities);
}

void MCPServer::setDisableSessionValidation(bool disable) {
    impl_->setDisableSessionValidation(disable);
}

void MCPServer::addTool(const Tool& tool) {
    impl_->addTool(tool);
}

void MCPServer::removeTool(const std::string& name) {
    impl_->removeTool(name);
}

void MCPServer::clearTools() {
    impl_->clearTools();
}

void MCPServer::addResource(const Resource& resource) {
    impl_->addResource(resource);
}

void MCPServer::removeResource(const std::string& uri) {
    impl_->removeResource(uri);
}

void MCPServer::clearResources() {
    impl_->clearResources();
}

void MCPServer::addPrompt(const Prompt& prompt) {
    impl_->addPrompt(prompt);
}

void MCPServer::removePrompt(const std::string& name) {
    impl_->removePrompt(name);
}

void MCPServer::clearPrompts() {
    impl_->clearPrompts();
}

void MCPServer::notifyResourcesChanged() {
    impl_->notifyResourcesChanged();
}

void MCPServer::notifyToolsChanged() {
    impl_->notifyToolsChanged();
}

void MCPServer::notifyPromptsChanged() {
    impl_->notifyPromptsChanged();
}

void MCPServer::setupDefaultTools() {
    impl_->setupDefaultTools();
}

void MCPServer::setupDefaultResources() {
    impl_->setupDefaultResources();
}

} // namespace mcp
