/**
 * @file mcp_client.cpp
 * @brief MCP Client Implementation
 * 
 * This file provides the implementation of the MCP client functionality.
 */

#include "mcp_client.h"
#include <sstream>
#include <chrono>
#include <random>
#include <regex>
#include <thread>

// 日志宏定义
#define MCP_LOG_DEBUG(msg) mcp_logger::Logger::getInstance().debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_INFO(msg) mcp_logger::Logger::getInstance().info(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_WARNING(msg) mcp_logger::Logger::getInstance().warning(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_ERROR(msg) mcp_logger::Logger::getInstance().error(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_FATAL(msg) mcp_logger::Logger::getInstance().fatal(msg, __FILE__, __LINE__, __FUNCTION__)

namespace mcp {

// MCP Client Implementation
class MCPClient::Impl {
public:
    Impl() : connected_(false), next_id_(1), transport_type_(mcp_transport::TransportType::AUTO), 
             current_endpoint_("/"), current_endpoint_index_(0) {
        MCP_LOG_DEBUG("MCPClient::Impl created");
        
        // 设置transport消息回调
        transport_.set_message_callback([this](const mcp_transport::TransportMessage& message) {
            handle_transport_message(message);
        });
        
        // 设置transport连接回调
        transport_.set_connection_callback([this](const mcp_transport::ConnectionInfo& info) {
            MCP_LOG_INFO("Transport connection status changed: " + std::to_string(static_cast<int>(info.status)));
            connected_ = (info.status == mcp_transport::TransportStatus::CONNECTED);
        });
        
        // 设置transport错误回调
        transport_.set_error_callback([this](const std::string& error) {
            MCP_LOG_ERROR("Transport error: " + error);
            connected_ = false;
        });
    }
    
    ~Impl() {
        disconnect();
        MCP_LOG_DEBUG("MCPClient::Impl destroyed");
    }
    
    bool connect(const std::string& endpoint) {
        MCP_LOG_INFO("Connecting to MCP server at: " + endpoint);
        
        // 解析endpoint并创建相应的transport配置
        mcp_transport::TransportConfig config = parse_endpoint(endpoint);
        return connect_with_config(config);
    }
    
    bool connect_with_config(const mcp_transport::TransportConfig& config) {
        MCP_LOG_INFO("Connecting with transport config");
        
        // 设置transport类型
        transport_type_ = config.type;
        
        // 启动transport
        if (!transport_.start(config)) {
            MCP_LOG_ERROR("Failed to start transport");
            connected_ = false;
            return false;
        }
        
        connected_ = true;
        MCP_LOG_INFO("Connected to MCP server successfully");
        return true;
    }
    
    void disconnect() {
        if (connected_) {
            MCP_LOG_INFO("Disconnecting from MCP server");
            transport_.stop();
            connected_ = false;
            MCP_LOG_INFO("Disconnected from MCP server");
        }
    }
    
    bool is_connected() const {
        return connected_ && transport_.is_running();
    }
    
    void set_transport_type(mcp_transport::TransportType type) {
        transport_type_ = type;
        MCP_LOG_DEBUG("Transport type set to: " + std::to_string(static_cast<int>(type)));
    }
    
    mcp_transport::TransportType get_transport_type() const {
        return transport_type_;
    }
    
    mcp_transport::TransportStatus get_transport_status() const {
        return transport_.get_status();
    }
    
    InitializeResult initialize(const InitializeParams& params) {
        MCP_LOG_INFO("Initializing MCP client");
        std::cout << "Initializing MCP client" << std::endl;
        // Validate protocol version
        if (!utils::is_valid_protocol_version(params.protocolVersion)) {
            throw mcp_exception(error_code::invalid_params, "Unsupported protocol version: " + params.protocolVersion);
        }
        std::cout << "Initializing MCP client 111" << std::endl;
        std::cout << "Initializing MCP client 2" << std::endl; 
        // Create initialize request
        auto req = request::create(methods::INITIALIZE, params.to_json());
        MCP_LOG_DEBUG("Sending initialize request: " + req.to_json().dump());
        std::cout << "Initializing MCP client 3" << std::endl;
        // Send request and get response
        MCP_LOG_DEBUG("Sending initialize request: " + req.to_json().dump());
        
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            std::string error_msg = "Initialize failed: " + response.error.dump();
            MCP_LOG_ERROR(error_msg);
            throw mcp_exception(error_code::internal_error, error_msg);
        }
        
        // Parse initialize result
        InitializeResult result = InitializeResult::from_json(response.result);
        server_info_ = result.serverInfo;
        server_capabilities_ = result.capabilities;
        
        MCP_LOG_INFO("MCP client initialized successfully");
        MCP_LOG_INFO("Server: " + server_info_.name + " v" + server_info_.version);
        
        return result;
    }
    
    void initialized() {
        MCP_LOG_INFO("Sending initialized notification");
        auto notification = request::create_notification(methods::NOTIFICATION_INITIALIZED);
        send_notification_internal(notification);
    }
    
    json ping() {
        MCP_LOG_DEBUG("Sending ping request");
        auto req = request::create(methods::PING);
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            throw mcp_exception(error_code::internal_error, "Ping failed: " + response.error.dump());
        }
        
        MCP_LOG_DEBUG("Ping successful");
        return response.result;
    }
    
    std::vector<Tool> list_tools() {
        MCP_LOG_DEBUG("Requesting tools list");
        auto req = request::create(methods::TOOLS_LIST);
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            throw mcp_exception(error_code::internal_error, "Failed to list tools: " + response.error.dump());
        }
        
        std::vector<Tool> tools;
        if (response.result.contains("tools") && response.result["tools"].is_array()) {
            for (const auto& tool_json : response.result["tools"]) {
                tools.push_back(Tool::from_json(tool_json));
            }
        }
        
        MCP_LOG_INFO("Retrieved " + std::to_string(tools.size()) + " tools");
        return tools;
    }
    
    ToolCallResult call_tool(const ToolCallParams& params) {
        MCP_LOG_INFO("Calling tool: " + params.name);
        auto req = request::create(methods::TOOLS_CALL, params.to_json());
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            ToolCallResult result;
            result.isError = true;
            result.errorMessage = response.error.dump();
            MCP_LOG_ERROR("Tool call failed: " + result.errorMessage);
            return result;
        }
        
        ToolCallResult result = ToolCallResult::from_json(response.result);
        MCP_LOG_INFO("Tool call completed successfully");
        return result;
    }
    
    ToolCallResult call_tool(const std::string& name, const json& arguments) {
        MCP_LOG_INFO("Calling tool: " + name);
        ToolCallParams params;
        params.name = name;
        params.arguments = arguments;
        return call_tool(params);
    }
    
    std::vector<Resource> list_resources() {
        MCP_LOG_DEBUG("Requesting resources list");
        auto req = request::create(methods::RESOURCES_LIST);
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            throw mcp_exception(error_code::internal_error, "Failed to list resources: " + response.error.dump());
        }
        
        std::vector<Resource> resources;
        if (response.result.contains("resources") && response.result["resources"].is_array()) {
            for (const auto& resource_json : response.result["resources"]) {
                resources.push_back(Resource::from_json(resource_json));
            }
        }
        
        MCP_LOG_INFO("Retrieved " + std::to_string(resources.size()) + " resources");
        return resources;
    }
    
    json read_resource(const std::string& uri) {
        MCP_LOG_INFO("Reading resource: " + uri);
        auto req = request::create(methods::RESOURCES_READ, {{"uri", uri}});
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            throw mcp_exception(error_code::internal_error, "Failed to read resource: " + response.error.dump());
        }
        
        MCP_LOG_INFO("Resource read successfully");
        return response.result;
    }
    
    std::vector<Prompt> list_prompts() {
        MCP_LOG_DEBUG("Requesting prompts list");
        auto req = request::create(methods::PROMPTS_LIST);
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            throw mcp_exception(error_code::internal_error, "Failed to list prompts: " + response.error.dump());
        }
        
        std::vector<Prompt> prompts;
        if (response.result.contains("prompts") && response.result["prompts"].is_array()) {
            for (const auto& prompt_json : response.result["prompts"]) {
                prompts.push_back(Prompt::from_json(prompt_json));
            }
        }
        
        MCP_LOG_INFO("Retrieved " + std::to_string(prompts.size()) + " prompts");
        return prompts;
    }
    
    json get_prompt(const std::string& name, const json& arguments) {
        MCP_LOG_INFO("Getting prompt: " + name);
        auto req = request::create(methods::PROMPTS_GET, {
            {"name", name},
            {"arguments", arguments}
        });
        auto response = send_request_internal(req);
        
        if (response.is_error()) {
            throw mcp_exception(error_code::internal_error, "Failed to get prompt: " + response.error.dump());
        }
        
        MCP_LOG_INFO("Prompt retrieved successfully");
        return response.result;
    }
    
    void send_notification(const std::string& method, const json& params) {
        MCP_LOG_DEBUG("Sending notification: " + method);
        auto notification = request::create_notification(method, params);
        send_notification_internal(notification);
    }
    
    void set_notification_handler(const std::string& method, NotificationHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        notification_handlers_[method] = handler;
        MCP_LOG_DEBUG("Set notification handler for: " + method);
    }
    
    void set_tool_call_handler(ToolCallHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        tool_call_handler_ = handler;
        MCP_LOG_DEBUG("Set tool call handler");
    }
    
    void set_resource_read_handler(ResourceReadHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        resource_read_handler_ = handler;
        MCP_LOG_DEBUG("Set resource read handler");
    }
    
    void set_prompt_get_handler(PromptGetHandler handler) {
        std::lock_guard<std::mutex> lock(handlers_mutex_);
        prompt_get_handler_ = handler;
        MCP_LOG_DEBUG("Set prompt get handler");
    }
    
    void set_client_info(const std::string& name, const std::string& version) {
        client_info_.name = name;
        client_info_.version = version;
        MCP_LOG_DEBUG("Set client info: " + name + " v" + version);
    }
    
    void set_capabilities(const ClientCapabilities& capabilities) {
        client_capabilities_ = capabilities;
        MCP_LOG_DEBUG("Set client capabilities");
    }
    
    ServerInfo get_server_info() const {
        return server_info_;
    }
    
    ServerCapabilities get_server_capabilities() const {
        return server_capabilities_;
    }
    
    // Endpoint management methods
    void set_endpoints(const std::vector<std::string>& endpoints) {
        endpoints_ = endpoints;
        if (!endpoints_.empty()) {
            current_endpoint_ = endpoints_[0];
            current_endpoint_index_ = 0;
            MCP_LOG_INFO("Set " + std::to_string(endpoints_.size()) + " endpoints, current: " + current_endpoint_);
        }
    }
    
    void set_current_endpoint(const std::string& endpoint) {
        current_endpoint_ = endpoint;
        MCP_LOG_INFO("Set current endpoint: " + current_endpoint_);
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
            MCP_LOG_INFO("Rotated to endpoint: " + current_endpoint_);
        }
    }
    
private:
    response send_request_internal(const request& req) {
        if (!is_connected()) {
            return response::create_error(req.id, error_code::internal_error, "Not connected to server");
        }
        
        try {
            // 将MCP请求转换为transport消息
            mcp_transport::TransportMessage transport_msg;
            // 将json类型的ID转换为字符串
            if (req.id.is_string()) {
                transport_msg.id = req.id.get<std::string>();
            } else if (req.id.is_number()) {
                transport_msg.id = std::to_string(req.id.get<int>());
            } else {
                transport_msg.id = req.id.dump();
            }
            transport_msg.content = req.to_json().dump();
            transport_msg.method = "POST";
            transport_msg.path = current_endpoint_;
            transport_msg.timestamp = std::chrono::system_clock::now();
            
            MCP_LOG_DEBUG("Sending request via transport: " + req.method + " (ID: " + req.id.dump() + ")");
            
            // 重试逻辑
            const int max_attempts = 5;
            int attempt = 0;
            mcp_transport::TransportMessage transport_response;
            
            while (attempt < max_attempts) {
                ++attempt;
                MCP_LOG_DEBUG("📤 尝试 " + std::to_string(attempt) + ": " + req.method);
                
                transport_response = transport_.send_request_sync(transport_msg);
                if (transport_response.status_code == 200) {
                    break;
                }
                
                MCP_LOG_WARNING("  重试(" + std::to_string(attempt) + "/" + std::to_string(max_attempts) + ")，状态: " + std::to_string(transport_response.status_code));
                if (attempt < max_attempts) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
                }
            }
            
            MCP_LOG_DEBUG("📥 收到响应:");
            MCP_LOG_DEBUG("  状态码: " + std::to_string(transport_response.status_code));
            
            if (transport_response.status_code == 200) {
                MCP_LOG_DEBUG("  ✅ 成功");
                
                // 解析响应内容
                json response_json;
                try {
                    response_json = json::parse(transport_response.content);
                    
                    if (response_json.contains("result")) {
                        MCP_LOG_DEBUG("  结果: " + response_json["result"].dump(2));
                        return response::create_success(req.id, response_json["result"]);
                    } else if (response_json.contains("error")) {
                        MCP_LOG_ERROR("  错误: " + response_json["error"].dump(2));
                        return response::create_error(req.id, error_code::internal_error, response_json["error"].dump());
                    } else {
                        MCP_LOG_DEBUG("  内容: " + transport_response.content);
                        return response::create_success(req.id, response_json);
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_DEBUG("  内容: " + transport_response.content);
                    MCP_LOG_ERROR("Failed to parse response JSON: " + std::string(e.what()));
                    return response::create_error(req.id, error_code::parse_error, "Failed to parse response");
                }
            } else {
                MCP_LOG_DEBUG("  ❌ 失败(重试已用尽)");
                MCP_LOG_ERROR("  错误: " + transport_response.error_message);
                MCP_LOG_DEBUG("  内容: " + transport_response.content);
                
                std::string error_msg = transport_response.error_message.empty() ? 
                    "Transport error" : transport_response.error_message;
                return response::create_error(req.id, error_code::internal_error, error_msg);
            }
        } catch (const std::exception& e) {
            MCP_LOG_ERROR("Error sending request: " + std::string(e.what()));
            return response::create_error(req.id, error_code::internal_error, "Send request error: " + std::string(e.what()));
        }
    }
    
    void send_notification_internal(const request& notification) {
        if (!is_connected()) {
            MCP_LOG_WARNING("Cannot send notification: not connected");
            return;
        }
        
        try {
            // 将MCP通知转换为transport消息
            mcp_transport::TransportMessage transport_msg;
            // 将json类型的ID转换为字符串
            if (notification.id.is_string()) {
                transport_msg.id = notification.id.get<std::string>();
            } else if (notification.id.is_number()) {
                transport_msg.id = std::to_string(notification.id.get<int>());
            } else {
                transport_msg.id = notification.id.dump();
            }
            transport_msg.content = notification.to_json().dump();
            transport_msg.method = "POST";
            transport_msg.path = "/";
            transport_msg.timestamp = std::chrono::system_clock::now();
            
            MCP_LOG_DEBUG("Sending notification via transport: " + notification.method);
            
            // 异步发送通知（不需要等待响应）
            auto future = transport_.send_request(transport_msg);
            // 注意：这里不等待响应，因为通知是单向的
            
        } catch (const std::exception& e) {
            MCP_LOG_ERROR("Error sending notification: " + std::string(e.what()));
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
            
            MCP_LOG_INFO("Parsed IPC endpoint: " + command);
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
            
            config.type = mcp_transport::TransportType::SSE;
            config.sse.host = host;
            config.sse.port = port_str.empty() ? (protocol == "https" ? 443 : 80) : std::stoi(port_str);
            config.sse.protocol = (protocol == "https") ? netio::Protocol::HTTPS : netio::Protocol::HTTP;
            config.sse.timeout_seconds = 30;
            config.sse.max_retries = 3;
            config.sse.user_agent = "MCP-Client/1.0";
            config.sse.keep_alive = true;
            config.sse.auto_reconnect = false;
            
            MCP_LOG_INFO("Parsed HTTP endpoint: " + protocol + "://" + host + ":" + std::to_string(config.sse.port));
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
        
        MCP_LOG_INFO("Using default IPC configuration for endpoint: " + endpoint);
        return config;
    }
    
    // 处理来自transport的消息
    void handle_transport_message(const mcp_transport::TransportMessage& message) {
        try {
            // 解析消息内容为JSON
            json message_json = json::parse(message.content);
            
            // 检查是否是通知
            if (!message_json.contains("id") || message_json["id"].is_null()) {
                // 这是一个通知
                if (message_json.contains("method") && message_json.contains("params")) {
                    std::string method = message_json["method"];
                    json params = message_json["params"];
                    
                    MCP_LOG_DEBUG("Received notification: " + method);
                    
                    // 查找对应的通知处理器
                    std::lock_guard<std::mutex> lock(handlers_mutex_);
                    auto it = notification_handlers_.find(method);
                    if (it != notification_handlers_.end()) {
                        it->second(method, params);
                    }
                }
            } else {
                // 这是一个响应，但我们在send_request_internal中已经处理了
                MCP_LOG_DEBUG("Received response message (already handled)");
            }
        } catch (const std::exception& e) {
            MCP_LOG_ERROR("Error handling transport message: " + std::string(e.what()));
        }
    }
    
    bool connected_;
    int next_id_;
    mcp_transport::TransportType transport_type_;
    mcp_transport::Transport transport_;
    
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
};

// MCPClient public interface
MCPClient::MCPClient() : impl_(std::make_unique<Impl>()) {
    MCP_LOG_DEBUG("MCPClient created");
}

MCPClient::~MCPClient() {
    MCP_LOG_DEBUG("MCPClient destroyed");
}

bool MCPClient::connect(const std::string& endpoint) {
    return impl_->connect(endpoint);
}

bool MCPClient::connect_with_config(const mcp_transport::TransportConfig& config) {
    return impl_->connect_with_config(config);
}

void MCPClient::disconnect() {
    impl_->disconnect();
}

bool MCPClient::is_connected() const {
    return impl_->is_connected();
}

void MCPClient::set_transport_type(mcp_transport::TransportType type) {
    impl_->set_transport_type(type);
}

mcp_transport::TransportType MCPClient::get_transport_type() const {
    return impl_->get_transport_type();
}

mcp_transport::TransportStatus MCPClient::get_transport_status() const {
    return impl_->get_transport_status();
}

InitializeResult MCPClient::initialize(const InitializeParams& params) {
    return impl_->initialize(params);
}

void MCPClient::initialized() {
    impl_->initialized();
}

json MCPClient::ping() {
    return impl_->ping();
}

std::vector<Tool> MCPClient::list_tools() {
    return impl_->list_tools();
}

ToolCallResult MCPClient::call_tool(const ToolCallParams& params) {
    return impl_->call_tool(params);
}

ToolCallResult MCPClient::call_tool(const std::string& name, const json& arguments) {
    return impl_->call_tool(name, arguments);
}

std::vector<Resource> MCPClient::list_resources() {
    return impl_->list_resources();
}

json MCPClient::read_resource(const std::string& uri) {
    return impl_->read_resource(uri);
}

std::vector<Prompt> MCPClient::list_prompts() {
    return impl_->list_prompts();
}

json MCPClient::get_prompt(const std::string& name, const json& arguments) {
    return impl_->get_prompt(name, arguments);
}

void MCPClient::send_notification(const std::string& method, const json& params) {
    impl_->send_notification(method, params);
}

void MCPClient::set_notification_handler(const std::string& method, NotificationHandler handler) {
    impl_->set_notification_handler(method, handler);
}

void MCPClient::set_tool_call_handler(ToolCallHandler handler) {
    impl_->set_tool_call_handler(handler);
}

void MCPClient::set_resource_read_handler(ResourceReadHandler handler) {
    impl_->set_resource_read_handler(handler);
}

void MCPClient::set_prompt_get_handler(PromptGetHandler handler) {
    impl_->set_prompt_get_handler(handler);
}

void MCPClient::set_client_info(const std::string& name, const std::string& version) {
    impl_->set_client_info(name, version);
}

void MCPClient::set_capabilities(const ClientCapabilities& capabilities) {
    impl_->set_capabilities(capabilities);
}

ServerInfo MCPClient::get_server_info() const {
    return impl_->get_server_info();
}

ServerCapabilities MCPClient::get_server_capabilities() const {
    return impl_->get_server_capabilities();
}

// Endpoint management methods
void MCPClient::set_endpoints(const std::vector<std::string>& endpoints) {
    impl_->set_endpoints(endpoints);
}

void MCPClient::set_current_endpoint(const std::string& endpoint) {
    impl_->set_current_endpoint(endpoint);
}

std::string MCPClient::get_current_endpoint() const {
    return impl_->get_current_endpoint();
}

std::vector<std::string> MCPClient::get_endpoints() const {
    return impl_->get_endpoints();
}

} // namespace mcp
