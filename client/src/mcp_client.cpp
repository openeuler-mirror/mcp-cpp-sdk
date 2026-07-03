/**
 * @file mcp_client.cpp
 * @brief MCP Client Implementation
 * 
 * This file provides the implementation of the MCP client functionality.
 */

#include "mcp_client.h"
#include "../../transport/include/transport_factory.h"
#include "../../transport/include/https_transport.h"
#include "../../log/include/logger.h"
#include <sstream>
#include <chrono>
#include <random>
#include <regex>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cctype>
#include <map>

// 日志记录器将在需要时局部声明

namespace mcp {

// MCP Client Implementation
class MCPClient::Impl {
public:
    Impl() : connected_(false), transport_type_(mcp_transport::TransportType::AUTO), 
             transport_(nullptr), current_endpoint_("/"), current_endpoint_index_(0) {
        // 延迟初始化logger，避免在构造函数中调用，防止静态初始化顺序问题
        // mcp_logger::getModuleLogger("mcp_client")->debug("MCPClient::Impl created");
        // transport_ 将在 connect_with_config 中使用工厂模式创建
    }
    
    ~Impl() {
        disconnect();
        // 延迟获取logger，避免在析构过程中出现问题
        // mcp_logger::getModuleLogger("mcp_client")->debug("MCPClient::Impl destroyed");
    }
    
    bool connect(const std::string& endpoint) {
        mcp_logger::getModuleLogger("mcp_client")->info("Connecting to MCP server at: " + endpoint);
        
        // 验证端点格式
        if (endpoint.empty()) {
            mcp_logger::getModuleLogger("mcp_client")->error("Empty endpoint provided");
            return false;
        }
        
        // 解析endpoint并创建相应的transport配置
        mcp_transport::TransportConfig config = parse_endpoint(endpoint);
        
        // 对于无效端点，直接返回false
        if (config.type == mcp_transport::TransportType::IPC && 
            (config.ipc.command.empty() || config.ipc.command == "invalid://endpoint" || 
             config.ipc.command == "invalid-endpoint")) {
            mcp_logger::getModuleLogger("mcp_client")->error("Invalid endpoint: " + endpoint);
            return false;
        }
        
        return connect_with_config(config);
    }
    
    bool connect_with_config(const mcp_transport::TransportConfig& config) {
        mcp_logger::getModuleLogger("mcp_client")->info("Connecting with transport config");
        
        // 设置transport类型
        transport_type_ = config.type;
        
        // 使用工厂模式创建 transport
        transport_ = mcp_transport::TransportFactory::createFromConfig(config);
        if (!transport_) {
            mcp_logger::getModuleLogger("mcp_client")->error("Failed to create transport using factory");
            connected_ = false;
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
        
        // 设置transport消息回调
        transport_->setMessageCallback([this](const mcp_transport::TransportMessage& message) {
            handle_transport_message(message);
        });
        
        // 设置transport连接回调（内部默认回调，用户可以通过set_connection_callback覆盖）
        transport_->setConnectionCallback([this](const mcp_transport::ConnectionInfo& info) {
            // 注意：这个回调可能在logger初始化之前被调用，所以延迟logger获取
            try {
                auto logger = mcp_logger::getModuleLogger("mcp_client");
                logger->info("Transport connection status changed: " + std::to_string(static_cast<int>(info.status)));
            } catch (...) {
                // 如果logger尚未初始化，静默忽略
            }
            connected_ = (info.status == mcp_transport::TransportStatus::CONNECTED);
            
            // 从连接信息中提取 session_id 并设置为后续请求的 request_id
            if (!info.netio_info.session_id.empty()) {
                setSessionId(info.netio_info.session_id);
                try {
                    auto logger = mcp_logger::getModuleLogger("mcp_client");
                    logger->info("Extracted session id from connection info: " + info.netio_info.session_id);
                } catch (...) {
                    // 如果logger尚未初始化，静默忽略
                }
            }
            
            // 如果用户设置了回调，也调用它
            {
                std::lock_guard<std::mutex> lock(handlers_mutex_);
                if (connection_callback_) {
                    connection_callback_(info);
                }
            }
        });
        
        // 设置transport错误回调
        transport_->setErrorCallback([this](const std::string& error) {
            // 注意：这个回调可能在logger初始化之前被调用，所以延迟logger获取
            try {
                auto logger = mcp_logger::getModuleLogger("mcp_client");
                logger->error("Transport error: " + error);
            } catch (...) {
                // 如果logger尚未初始化，静默忽略
            }
            connected_ = false;
        });
        
        // 启动transport
        if (!transport_->start(config)) {
            mcp_logger::getModuleLogger("mcp_client")->error("Failed to start transport");
            connected_ = false;
            return false;
        }
        
        connected_ = true;
        mcp_logger::getModuleLogger("mcp_client")->info("Connected to MCP server successfully");
        return true;
    }
    
    void disconnect() {
        if (connected_ && transport_) {
            mcp_logger::getModuleLogger("mcp_client")->info("Disconnecting from MCP server");
            transport_->stop();
            transport_.reset(); // 释放 transport
            connected_ = false;
            mcp_logger::getModuleLogger("mcp_client")->info("Disconnected from MCP server");
        }
    }
    
    bool is_connected() const {
        return connected_ && transport_ && transport_->isRunning();
    }
    
    void set_transport_type(mcp_transport::TransportType type) {
        transport_type_ = type;
        mcp_logger::getModuleLogger("mcp_client")->debug("Transport type set to: " + std::to_string(static_cast<int>(type)));
    }
    
    mcp_transport::TransportType get_transport_type() const {
        return transport_type_;
    }
    
    mcp_transport::TransportStatus get_transport_status() const {
        return transport_ ? transport_->getStatus() : mcp_transport::TransportStatus::DISCONNECTED;
    }
    
    InitializeResult initialize(const InitializeParams& params) {
        mcp_logger::getModuleLogger("mcp_client")->info("Initializing MCP client");
        std::cout << "Initializing MCP client" << std::endl;

        // Validate protocol version
        if (!utils::isValidProtocolVersion(params.protocolVersion)) {
            throw mcp_exception(error_code::invalid_params, "Unsupported protocol version: " + params.protocolVersion);
        }
       
        // Create initialize request with session_id
        auto req = create_request_with_session_id(methods::INITIALIZE, params.toJson());
        mcp_logger::getModuleLogger("mcp_client")->debug("Sending initialize request: " + req.toJson().dump());
        
        // Send request and get response
        auto response = send_request_internal(req); 
        if (response.isError()) {
            std::string error_msg = "Initialize failed: " + response.error.dump();
            mcp_logger::getModuleLogger("mcp_client")->error(error_msg);
            throw mcp_exception(error_code::internal_error, error_msg);
        }
        
        // Parse initialize result
        InitializeResult result = InitializeResult::fromJson(response.result);
        server_info_ = result.serverInfo;
        server_capabilities_ = result.capabilities;
        
        mcp_logger::getModuleLogger("mcp_client")->info("MCP client initialized successfully. Server: " + server_info_.name + " v" + server_info_.version);
        
        return result;
    }
    
    void initialized() {
        mcp_logger::getModuleLogger("mcp_client")->info("Sending initialized notification");
        //auto notification = request::createNotification(methods::NOTIFICATION_INITIALIZED);
        //send_notification_internal(notification);
    }
    
    json ping() {
        mcp_logger::getModuleLogger("mcp_client")->debug("Sending ping request");
        auto req = create_request_with_session_id(methods::PING);
        auto response = send_request_internal(req);
        
        if (response.isError()) {
            throw mcp_exception(error_code::internal_error, "Ping failed: " + response.error.dump());
        }
        
        mcp_logger::getModuleLogger("mcp_client")->debug("Ping successful");
        return response.result;
    }
    
    std::vector<Tool> list_tools() {
        mcp_logger::getModuleLogger("mcp_client")->debug("Requesting tools list");
        auto req = create_request_with_session_id(methods::TOOLS_LIST);
        auto response = send_request_internal(req);
        
        if (response.isError()) {
            throw mcp_exception(error_code::internal_error, "Failed to list tools: " + response.error.dump());
        }
        
        std::vector<Tool> tools;
        if (response.result.contains("tools") && response.result["tools"].is_array()) {
            for (const auto& tool_json : response.result["tools"]) {
                tools.push_back(Tool::fromJson(tool_json));
            }
        }
        
        mcp_logger::getModuleLogger("mcp_client")->debug("Retrieved " + std::to_string(tools.size()) + " tools");
        return tools;
    }
    
    ToolCallResult call_tool(const ToolCallParams& params) {
        mcp_logger::getModuleLogger("mcp_client")->debug("Calling tool: " + params.name);
        
        // 检查连接状态
        if (!is_connected()) {
            throw mcp_exception(error_code::internal_error, "Not connected to server");
        }
        
        auto req = create_request_with_session_id(methods::TOOLS_CALL, params.toJson());
        auto response = send_request_internal(req);
        
        if (response.isError()) {
            ToolCallResult result;
            result.isError = true;
            result.errorMessage = response.error.dump();
            mcp_logger::getModuleLogger("mcp_client")->error("Tool call failed: " + result.errorMessage);
            return result;
        }
        
        ToolCallResult result = ToolCallResult::fromJson(response.result);
        mcp_logger::getModuleLogger("mcp_client")->debug("Tool call completed successfully");
        return result;
    }
    
    ToolCallResult call_tool(const std::string& name, const json& arguments) {
        mcp_logger::getModuleLogger("mcp_client")->debug("Calling tool: " + name);
        ToolCallParams params;
        params.name = name;
        params.arguments = arguments;
        return call_tool(params);
    }
    
    std::vector<Resource> list_resources() {
        mcp_logger::getModuleLogger("mcp_client")->debug("Requesting resources list");
        auto req = create_request_with_session_id(methods::RESOURCES_LIST);
        auto response = send_request_internal(req);
        
        if (response.isError()) {
            throw mcp_exception(error_code::internal_error, "Failed to list resources: " + response.error.dump());
        }
        
        std::vector<Resource> resources;
        if (response.result.contains("resources") && response.result["resources"].is_array()) {
            for (const auto& resource_json : response.result["resources"]) {
                resources.push_back(Resource::fromJson(resource_json));
            }
        }
        
        mcp_logger::getModuleLogger("mcp_client")->debug("Retrieved " + std::to_string(resources.size()) + " resources");
        return resources;
    }
    
    json read_resource(const std::string& uri) {
        mcp_logger::getModuleLogger("mcp_client")->debug("Reading resource: " + uri);
        auto req = create_request_with_session_id(methods::RESOURCES_READ, {{"uri", uri}});
        auto response = send_request_internal(req);
        
        if (response.isError()) {
            throw mcp_exception(error_code::internal_error, "Failed to read resource: " + response.error.dump());
        }
        
        mcp_logger::getModuleLogger("mcp_client")->debug("Resource read successfully");
        return response.result;
    }
    
    std::vector<Prompt> list_prompts() {
        mcp_logger::getModuleLogger("mcp_client")->debug("Requesting prompts list");
        auto req = create_request_with_session_id(methods::PROMPTS_LIST);
        auto response = send_request_internal(req);
        
        if (response.isError()) {
            throw mcp_exception(error_code::internal_error, "Failed to list prompts: " + response.error.dump());
        }
        
        std::vector<Prompt> prompts;
        if (response.result.contains("prompts") && response.result["prompts"].is_array()) {
            for (const auto& prompt_json : response.result["prompts"]) {
                prompts.push_back(Prompt::fromJson(prompt_json));
            }
        }
        
        mcp_logger::getModuleLogger("mcp_client")->debug("Retrieved " + std::to_string(prompts.size()) + " prompts");
        return prompts;
    }
    
    json get_prompt(const std::string& name, const json& arguments) {
        mcp_logger::getModuleLogger("mcp_client")->debug("Getting prompt: " + name);
        auto req = create_request_with_session_id(methods::PROMPTS_GET, {
            {"name", name},
            {"arguments", arguments}
        });
        auto response = send_request_internal(req);
        
        if (response.isError()) {
            throw mcp_exception(error_code::internal_error, "Failed to get prompt: " + response.error.dump());
        }
        
        mcp_logger::getModuleLogger("mcp_client")->debug("Prompt retrieved successfully");
        return response.result;
    }
    
    void send_notification(const std::string& method, const json& params) {
        mcp_logger::getModuleLogger("mcp_client")->debug("Sending notification: " + method);
        auto notification = request::createNotification(method, params);
        send_notification_internal(notification);
    }
    
    void set_notification_handler(const std::string& method, NotificationHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        notification_handlers_[method] = handler;
        mcp_logger::getModuleLogger("mcp_client")->debug("Set notification handler for: " + method);
    }
    
    void set_tool_call_handler(ToolCallHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        tool_call_handler_ = handler;
        mcp_logger::getModuleLogger("mcp_client")->debug("Set tool call handler");
    }
    
    void set_resource_read_handler(ResourceReadHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        resource_read_handler_ = handler;
        mcp_logger::getModuleLogger("mcp_client")->debug("Set resource read handler");
    }
    
    void set_prompt_get_handler(PromptGetHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        prompt_get_handler_ = handler;
        mcp_logger::getModuleLogger("mcp_client")->debug("Set prompt get handler");
    }
    
    void set_connection_callback(std::function<void(const mcp_transport::ConnectionInfo&)> callback) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        connection_callback_ = callback;
        mcp_logger::getModuleLogger("mcp_client")->debug("Set connection callback");
        // 注意：不需要重新设置 transport 的回调，因为构造函数中的回调会检查并调用 connection_callback_
    }
    
    void set_client_info(const std::string& name, const std::string& version) {
        client_info_.name = name;
        client_info_.version = version;
        mcp_logger::getModuleLogger("mcp_client")->debug("Set client info: " + name + " v" + version);
    }
    
    void set_capabilities(const ClientCapabilities& capabilities) {
        client_capabilities_ = capabilities;
        mcp_logger::getModuleLogger("mcp_client")->debug("Set client capabilities");
    }
    
    ServerInfo get_server_info() const {
        return server_info_;
    }
    
    ServerCapabilities get_server_capabilities() const {
        return server_capabilities_;
    }
    
    std::string get_session_id() const {
        return getSessionId();
    }
    
    // Endpoint management methods
    void set_endpoints(const std::vector<std::string>& endpoints) {
        endpoints_ = endpoints;
        if (!endpoints_.empty()) {
            current_endpoint_ = endpoints_[0];
            current_endpoint_index_ = 0;
            mcp_logger::getModuleLogger("mcp_client")->info("Set " + std::to_string(endpoints_.size()) + " endpoints, current: " + current_endpoint_);
        }
    }
    
    void set_current_endpoint(const std::string& endpoint) {
        current_endpoint_ = endpoint;
        mcp_logger::getModuleLogger("mcp_client")->info("Set current endpoint: " + current_endpoint_);
    }
    
    std::string get_current_endpoint() const {
        return current_endpoint_;
    }
    
    std::vector<std::string> get_endpoints() const {
        return endpoints_;
    }
    
    // 轮询到下一个endpoint
    void rotate_endpoint() {
        if (endpoints_.size() > 1) {
            current_endpoint_index_ = (current_endpoint_index_ + 1) % endpoints_.size();
            current_endpoint_ = endpoints_[current_endpoint_index_];
            mcp_logger::getModuleLogger("mcp_client")->info("Rotated to endpoint: " + current_endpoint_);
        }
    }
    
private:
    // 辅助函数：使用 session_id 创建请求
    request create_request_with_session_id(const std::string& method, const json& params = json::object()) {
        std::string session_id = getSessionId();
        if (!session_id.empty()) {
            return request::createWithId(session_id, method, params);
        } else {
            // 如果没有 session_id，使用默认的 create 方法
            return request::create(method, params);
        }
    }
    
    response send_request_internal(const request& req) {
        if (!is_connected()) {
            return response::createError(req.id, error_code::internal_error, "Not connected to server");
        }
        
        try {
            // 将MCP请求转换为transport消息
            mcp_transport::TransportMessage transport_msg;
            json request_json = req.toJson();

            // 获取 session_id 用于 header（请求的 id 已经在创建时使用 session_id 设置了）
            std::string session_id = getSessionId();

            // 设置 transport_msg.id（请求的 id 已经在创建时通过 createWithId 设置了）
            if (request_json.contains("id")) {
                transport_msg.id = jsonIdToString(request_json["id"]);
            } else if (!req.id.is_null()) {
                transport_msg.id = jsonIdToString(req.id);
            }

            transport_msg.content = request_json.dump();
            transport_msg.method = "POST";
            transport_msg.path = current_endpoint_;
            transport_msg.timestamp = std::chrono::system_clock::now();
            if (!session_id.empty()) {
                transport_msg.headers["X-Session-Id"] = session_id;
                mcp_logger::getModuleLogger("mcp_client")->debug("Attached session id header: " + session_id);
            }
            
            mcp_logger::getModuleLogger("mcp_client")->debug("Sending request via transport: " + req.method + " (ID: " + req.id.dump() + ")");
            
            // 重试逻辑
            const int max_attempts = 5;
            int attempt = 0;
            mcp_transport::TransportMessage transport_response;
            
            while (attempt < max_attempts) {
                ++attempt;
                mcp_logger::getModuleLogger("mcp_client")->debug("📤 尝试 " + std::to_string(attempt) + ": " + req.method);
                
                if (!transport_) {
                    mcp_logger::getModuleLogger("mcp_client")->error("Transport is not initialized");
                    throw std::runtime_error("Transport is not initialized");
                }
                transport_response = transport_->sendRequestSync(transport_msg);
                updateSessionIdFromHeaders(transport_response.headers);
                if (transport_response.status_code == 200) {
                    break;
                }
                
                mcp_logger::getModuleLogger("mcp_client")->warning("  重试(" + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ")，状态: " + std::to_string(transport_response.status_code));
                if (attempt < max_attempts) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
                }
            }
            
            mcp_logger::getModuleLogger("mcp_client")->debug("📥 收到响应:");
            mcp_logger::getModuleLogger("mcp_client")->debug("  状态码: " + std::to_string(transport_response.status_code));
            
            if (transport_response.status_code == 200) {
                mcp_logger::getModuleLogger("mcp_client")->debug("  ✅ 成功");
                
                // 解析响应内容
                json response_json;
                try {
                    mcp_logger::getModuleLogger("mcp_client")->debug("  响应内容: " + transport_response.content);
                    response_json = json::parse(transport_response.content);
                    
                    if (response_json.contains("result")) {
                        mcp_logger::getModuleLogger("mcp_client")->debug("  结果: " + response_json["result"].dump(2));
                        return response::createSuccess(req.id, response_json["result"]);
                    } else if (response_json.contains("error")) {
                        mcp_logger::getModuleLogger("mcp_client")->error("  错误: " + response_json["error"].dump(2));
                        return response::createError(req.id, error_code::internal_error, response_json["error"].dump());
                    } else {
                        mcp_logger::getModuleLogger("mcp_client")->debug("  内容: " + transport_response.content);
                        return response::createSuccess(req.id, response_json);
                    }
                } catch (const std::exception& e) {
                    mcp_logger::getModuleLogger("mcp_client")->debug("  内容: " + transport_response.content);
                    mcp_logger::getModuleLogger("mcp_client")->error("Failed to parse response JSON: " + std::string(e.what()));
                    return response::createError(req.id, error_code::parse_error, "Failed to parse response");
                }
            } else {
                mcp_logger::getModuleLogger("mcp_client")->debug("  ❌ 失败(重试已用尽)");
                mcp_logger::getModuleLogger("mcp_client")->error("  错误: " + transport_response.error_message);
                mcp_logger::getModuleLogger("mcp_client")->debug("  内容: " + transport_response.content);
                
                std::string error_msg = transport_response.error_message.empty() ? 
                    "Transport error" : transport_response.error_message;
                return response::createError(req.id, error_code::internal_error, error_msg);
            }
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("mcp_client")->error("Error sending request: " + std::string(e.what()));
            return response::createError(req.id, error_code::internal_error, "Send request error: " + std::string(e.what()));
        }
    }
    
    void send_notification_internal(const request& notification) {
        if (!is_connected()) {
            mcp_logger::getModuleLogger("mcp_client")->warning("Cannot send notification: not connected");
            return;
        }
        
        try {
            // 将MCP通知转换为transport消息
            mcp_transport::TransportMessage transport_msg;
            json notification_json = notification.toJson();

            if (notification_json.contains("id") && !notification.id.is_null()) {
                transport_msg.id = jsonIdToString(notification_json["id"]);
            } else if (!notification.id.is_null()) {
                transport_msg.id = jsonIdToString(notification.id);
            }

            transport_msg.content = notification_json.dump();
            transport_msg.method = "POST";
            transport_msg.path = "/";
            transport_msg.timestamp = std::chrono::system_clock::now();
            std::string session_id = getSessionId();
            if (!session_id.empty()) {
                transport_msg.headers["X-Session-Id"] = session_id;
                mcp_logger::getModuleLogger("mcp_client")->debug("Attached session id header to notification: " + session_id);
            }
            
            mcp_logger::getModuleLogger("mcp_client")->debug("Sending notification via transport: " + notification.method);
            
            if (!transport_) {
                mcp_logger::getModuleLogger("mcp_client")->error("Transport is not initialized");
                return;
            }
            
            // 异步发送通知（不需要等待响应）
            auto future = transport_->sendRequest(transport_msg);
            // 注意：这里不等待响应，因为通知是单向的
            
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("mcp_client")->error("Error sending notification: " + std::string(e.what()));
        }
    }
    
    // 解析endpoint字符串并创建相应的transport配置
    mcp_transport::TransportConfig parse_endpoint(const std::string& endpoint) {
        mcp_transport::TransportConfig config;
        
        // 检查是否是IPC命令格式 (例如: "cmd:python script.py")
        if (endpoint.find("cmd:") == 0) {
            std::string command = endpoint.substr(4); // 移除 "cmd:" 前缀
            
            config.type = mcp_transport::TransportType::IPC;
            config.ipc.command = command;
            config.ipc.args = {};
            config.ipc.buffer_size = 4096;
            config.ipc.read_timeout_ms = 1000;
            config.ipc.max_retry_count = 3;
            config.ipc.enable_logging = true;
            
            mcp_logger::getModuleLogger("mcp_client")->info("Parsed IPC endpoint: " + command);
            return config;
        }
        
        // 检查是否是HTTP/HTTPS URL格式
        std::regex http_regex(R"(^(https?):\/\/([^:\/]+)(?::(\d+))?(?:\/(.*))?$)");
        std::smatch matches;
        
        if (std::regex_match(endpoint, matches, http_regex)) {
            std::string protocol = matches[1].str();
            std::string host = matches[2].str();
            std::string port_str = matches[3].str();
            std::string path = matches[4].str();
            
            // 根据协议类型设置 transport 类型
            config.type = (protocol == "https") ? mcp_transport::TransportType::HTTPS : mcp_transport::TransportType::HTTP;
            config.sse.host = host;
            config.sse.port = port_str.empty() ? (protocol == "https" ? 443 : 80) : std::stoi(port_str);
            config.sse.protocol = (protocol == "https") ? netio::Protocol::HTTPS : netio::Protocol::HTTP;
            config.sse.timeout_seconds = 30;
            config.sse.max_retries = 3;
            config.sse.user_agent = "MCP-Client/1.0";
            config.sse.keep_alive = true;
            config.sse.auto_reconnect = false;
            
            mcp_logger::getModuleLogger("mcp_client")->info("Parsed HTTP endpoint: " + protocol + "://" + host + ":" + std::to_string(config.sse.port));
            return config;
        }
        
        // 检查是否是无效端点
        if (endpoint == "invalid://endpoint" || endpoint == "invalid-endpoint" || endpoint.empty()) {
            // 返回一个无效的配置，让连接失败
            config.type = mcp_transport::TransportType::IPC;
            config.ipc.command = endpoint; // 保持原始值用于验证
            config.ipc.args = {};
            config.ipc.buffer_size = 4096;
            config.ipc.read_timeout_ms = 1000;
            config.ipc.max_retry_count = 3;
            config.ipc.enable_logging = true;
            
            mcp_logger::getModuleLogger("mcp_client")->info("Using default IPC configuration for endpoint: " + endpoint);
            return config;
        }
        
        // 默认使用IPC，假设endpoint是一个命令
        config.type = mcp_transport::TransportType::IPC;
        config.ipc.command = endpoint;
        config.ipc.args = {};
        config.ipc.buffer_size = 4096;
        config.ipc.read_timeout_ms = 1000;
        config.ipc.max_retry_count = 3;
        config.ipc.enable_logging = true;
        
        mcp_logger::getModuleLogger("mcp_client")->info("Using default IPC configuration for endpoint: " + endpoint);
        return config;
    }
    
    // 处理来自transport的消息
    void handle_transport_message(const mcp_transport::TransportMessage& message) {
        // 检查是否是有效的 JSON MCP 消息格式
        // 服务器日志输出可能被误读，先检查是否是 JSON 格式
        std::string content = message.content;
        
        // 去除前后空白字符
        size_t start = content.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) {
            // 空消息，忽略
            return;
        }
        size_t end = content.find_last_not_of(" \t\r\n");
        content = content.substr(start, end - start + 1);
        
        // 检查是否看起来像 JSON（以 { 开头）
        if (content.empty() || content[0] != '{') {
            // 不是 JSON 格式，可能是日志输出，静默忽略
            return;
        }
        
        try {
            // 解析消息内容为JSON
            json message_json = json::parse(content);
            
            // 检查是否是 JSON-RPC 2.0 格式
            if (!message_json.contains("jsonrpc") || message_json["jsonrpc"] != "2.0") {
                // 不是 JSON-RPC 2.0 格式，可能是日志输出，静默忽略
                return;
            }
            
            updateSessionIdFromHeaders(message.headers);
            
            // 检查是否是通知
            if (!message_json.contains("id") || message_json["id"].is_null()) {
                // 这是一个通知
                if (message_json.contains("method") && message_json.contains("params")) {
                    std::string method = message_json["method"];
                    json params = message_json["params"];
                    
                    mcp_logger::getModuleLogger("mcp_client")->debug("Received notification: " + method);
                    
                    // 查找对应的通知处理器
                    std::lock_guard<std::mutex> lock(handlers_mutex_);
                    auto it = notification_handlers_.find(method);
                    if (it != notification_handlers_.end()) {
                        it->second(method, params);
                    }
                }
            } else {
                // 这是一个响应，但我们在send_request_internal中已经处理了
                mcp_logger::getModuleLogger("mcp_client")->debug("Received response message (already handled)");
            }
        } catch (const nlohmann::json::parse_error& e) {
            // JSON 解析错误，可能是日志输出或格式错误，静默忽略
            // 只在调试模式下记录
            mcp_logger::getModuleLogger("mcp_client")->debug("Failed to parse message as JSON (likely log output): " + content.substr(0, 100));
        } catch (const std::exception& e) {
            // 其他错误，记录但使用 debug 级别而不是 error
            mcp_logger::getModuleLogger("mcp_client")->debug("Error handling transport message: " + std::string(e.what()));
        }
    }

    static bool equalsIgnoreCase(const std::string& lhs, const std::string& rhs) {
        if (lhs.size() != rhs.size()) {
            return false;
        }
        for (std::size_t i = 0; i < lhs.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(lhs[i])) != std::tolower(static_cast<unsigned char>(rhs[i]))) {
                return false;
            }
        }
        return true;
    }

    static std::string jsonIdToString(const json& id) {
        if (id.is_string()) {
            return id.get<std::string>();
        }
        if (id.is_number_integer() || id.is_number_unsigned()) {
            return std::to_string(id.get<long long>());
        }
        if (id.is_number_float()) {
            return std::to_string(id.get<double>());
        }
        return id.dump();
    }

    void updateSessionIdFromHeaders(const std::map<std::string, std::string>& headers) {
        std::string new_session_id;
        for (const auto& header : headers) {
            if (equalsIgnoreCase(header.first, "X-Session-Id")) {
                new_session_id = header.second;
                break;
            }
        }

        if (!new_session_id.empty()) {
            setSessionId(new_session_id);
            mcp_logger::getModuleLogger("mcp_client")->info("Received session id from headers: " + new_session_id);
        }
    }

    void setSessionId(const std::string& new_session_id) {
        std::lock_guard<std::mutex> lock(session_mutex_);
        if (session_id_ == new_session_id) {
            return;
        }
        session_id_ = new_session_id;
        mcp_logger::getModuleLogger("mcp_client")->info("Updated session id: " + session_id_);
    }

    std::string getSessionId() const {
        std::lock_guard<std::mutex> lock(session_mutex_);
        return session_id_;
    }
    
    bool connected_;
    // int next_id_;  // 暂时注释掉，未使用
    mcp_transport::TransportType transport_type_;
    std::unique_ptr<mcp_transport::TransportBase> transport_;
    
    // Endpoint management
    std::vector<std::string> endpoints_;
    std::string current_endpoint_;
    size_t current_endpoint_index_;
    
    ClientInfo client_info_;
    ClientCapabilities client_capabilities_;
    ServerInfo server_info_;
    ServerCapabilities server_capabilities_;
    
    std::mutex handlers_mutex_;
    std::map<std::string, NotificationHandler> notification_handlers_;
    ToolCallHandler tool_call_handler_;
    ResourceReadHandler resource_read_handler_;
    PromptGetHandler prompt_get_handler_;
    std::function<void(const mcp_transport::ConnectionInfo&)> connection_callback_;
    mutable std::mutex session_mutex_;
    std::string session_id_;
};

// MCPClient public interface
MCPClient::MCPClient() : impl_(std::make_unique<Impl>()) {
    mcp_logger::getModuleLogger("mcp_client")->debug("MCPClient created");
}

MCPClient::~MCPClient() {
    mcp_logger::getModuleLogger("mcp_client")->debug("MCPClient destroyed");
}

bool MCPClient::connect(const std::string& endpoint) {
    return impl_->connect(endpoint);
}

bool MCPClient::connectWithConfig(const mcp_transport::TransportConfig& config) {
    return impl_->connect_with_config(config);
}

void MCPClient::disconnect() {
    impl_->disconnect();
}

bool MCPClient::isConnected() const {
    return impl_->is_connected();
}

void MCPClient::setTransportType(mcp_transport::TransportType type) {
    impl_->set_transport_type(type);
}

mcp_transport::TransportType MCPClient::getTransportType() const {
    return impl_->get_transport_type();
}

mcp_transport::TransportStatus MCPClient::getTransportStatus() const {
    return impl_->get_transport_status();
}

InitializeResult MCPClient::initialize(const InitializeParams& params) {
    return impl_->initialize(params);
}

void MCPClient::initialized() {
    // 忽略初始化完成通知
    //impl_->initialized();
}

json MCPClient::ping() {
    return impl_->ping();
}

std::vector<Tool> MCPClient::listTools() {
    return impl_->list_tools();
}

ToolCallResult MCPClient::callTool(const ToolCallParams& params) {
    return impl_->call_tool(params);
}

ToolCallResult MCPClient::callTool(const std::string& name, const json& arguments) {
    return impl_->call_tool(name, arguments);
}

std::vector<Resource> MCPClient::listResources() {
    return impl_->list_resources();
}

json MCPClient::readResource(const std::string& uri) {
    return impl_->read_resource(uri);
}

std::vector<Prompt> MCPClient::listPrompts() {
    return impl_->list_prompts();
}

json MCPClient::getPrompt(const std::string& name, const json& arguments) {
    return impl_->get_prompt(name, arguments);
}

void MCPClient::sendNotification(const std::string& method, const json& params) {
    impl_->send_notification(method, params);
}

void MCPClient::setNotificationHandler(const std::string& method, NotificationHandler handler) {
    impl_->set_notification_handler(method, handler);
}

void MCPClient::setToolCallHandler(ToolCallHandler handler) {
    impl_->set_tool_call_handler(handler);
}

void MCPClient::setResourceReadHandler(ResourceReadHandler handler) {
    impl_->set_resource_read_handler(handler);
}

void MCPClient::setPromptGetHandler(PromptGetHandler handler) {
    impl_->set_prompt_get_handler(handler);
}

void MCPClient::setConnectionCallback(ConnectionCallback callback) {
    impl_->set_connection_callback(callback);
}

void MCPClient::setClientInfo(const std::string& name, const std::string& version) {
    impl_->set_client_info(name, version);
}

void MCPClient::setCapabilities(const ClientCapabilities& capabilities) {
    impl_->set_capabilities(capabilities);
}

ServerInfo MCPClient::getServerInfo() const {
    return impl_->get_server_info();
}

ServerCapabilities MCPClient::getServerCapabilities() const {
    return impl_->get_server_capabilities();
}

std::string MCPClient::getSessionId() const {
    return impl_->get_session_id();
}

// Endpoint management methods
void MCPClient::setEndpoints(const std::vector<std::string>& endpoints) {
    impl_->set_endpoints(endpoints);
}

void MCPClient::setCurrentEndpoint(const std::string& endpoint) {
    impl_->set_current_endpoint(endpoint);
}

std::string MCPClient::getCurrentEndpoint() const {
    return impl_->get_current_endpoint();
}

std::vector<std::string> MCPClient::getEndpoints() const {
    return impl_->get_endpoints();
}

} // namespace mcp
