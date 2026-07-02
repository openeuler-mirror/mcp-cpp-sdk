/**
 * @file mcp_server.cpp
 * @brief MCP Server Implementation
 * 
 * This file provides the implementation of the MCP server functionality.
 */

#include "mcp_server.h"
#include <sstream>
#include <chrono>
#include <random>

// 日志宏定义
#define MCP_LOG_DEBUG(msg) mcp_logger::Logger::getInstance().debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_INFO(msg) mcp_logger::Logger::getInstance().info(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_WARNING(msg) mcp_logger::Logger::getInstance().warning(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_ERROR(msg) mcp_logger::Logger::getInstance().error(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_FATAL(msg) mcp_logger::Logger::getInstance().fatal(msg, __FILE__, __LINE__, __FUNCTION__)

namespace mcp {

// MCP Server Implementation
class MCPServer::Impl {
public:
    Impl() : running_(false), next_id_(1) {
        MCP_LOG_DEBUG("MCPServer::Impl created");
    }
    
    ~Impl() {
        MCP_LOG_DEBUG("MCPServer::Impl destroyed");
    }
    
    bool start(const std::string& endpoint) {
        MCP_LOG_INFO("Starting MCP server at: " + endpoint);
        // TODO: Implement actual server startup logic
        running_ = true;
        MCP_LOG_INFO("MCP server started successfully");
        return true;
    }
    
    void stop() {
        if (running_) {
            MCP_LOG_INFO("Stopping MCP server");
            running_ = false;
            MCP_LOG_INFO("MCP server stopped");
        }
    }
    
    bool is_running() const {
        return running_;
    }
    
    InitializeResult initialize(const InitializeParams& params) {
        MCP_LOG_INFO("Handling initialize request from client: " + params.clientInfo.name);
        
        // Validate protocol version
        if (!utils::is_valid_protocol_version(params.protocolVersion)) {
            throw mcp_exception(error_code::invalid_params, "Unsupported protocol version: " + params.protocolVersion);
        }
        
        // Store client info
        client_info_ = params.clientInfo;
        client_capabilities_ = params.capabilities;
        
        // Create initialize result
        InitializeResult result;
        result.protocolVersion = PROTOCOL_VERSION;
        result.capabilities = server_capabilities_;
        result.serverInfo = server_info_;
        
        MCP_LOG_INFO("Initialize completed successfully");
        return result;
    }
    
    void initialized() {
        MCP_LOG_INFO("Client initialized notification received");
        // TODO: Handle client initialized notification
    }
    
    json ping() {
        MCP_LOG_DEBUG("Handling ping request");
        return json::object();
    }
    
    std::vector<Tool> list_tools() {
        MCP_LOG_DEBUG("Listing tools");
        return tools_;
    }
    
    ToolCallResult call_tool(const ToolCallParams& params) {
        MCP_LOG_INFO("Tool call requested: " + params.name);
        
        if (tool_call_handler_) {
            return tool_call_handler_(params);
        } else {
            ToolCallResult result;
            result.isError = true;
            result.errorMessage = "No tool call handler registered";
            MCP_LOG_ERROR("Tool call failed: " + result.errorMessage);
            return result;
        }
    }
    
    std::vector<Resource> list_resources() {
        MCP_LOG_DEBUG("Listing resources");
        return resources_;
    }
    
    json read_resource(const std::string& uri) {
        MCP_LOG_INFO("Reading resource: " + uri);
        
        if (resource_read_handler_) {
            return resource_read_handler_(uri);
        } else {
            throw mcp_exception(error_code::internal_error, "No resource read handler registered");
        }
    }
    
    std::vector<Prompt> list_prompts() {
        MCP_LOG_DEBUG("Listing prompts");
        return prompts_;
    }
    
    json get_prompt(const std::string& name, const json& arguments) {
        MCP_LOG_INFO("Getting prompt: " + name);
        
        if (prompt_get_handler_) {
            return prompt_get_handler_(name, arguments);
        } else {
            throw mcp_exception(error_code::internal_error, "No prompt get handler registered");
        }
    }
    
    void send_notification(const std::string& method, const json& params) {
        (void)params; // Suppress unused parameter warning
        MCP_LOG_DEBUG("Sending notification: " + method);
        // TODO: Implement actual notification sending logic
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
    
    void set_server_info(const std::string& name, const std::string& version) {
        server_info_.name = name;
        server_info_.version = version;
        MCP_LOG_DEBUG("Set server info: " + name + " v" + version);
    }
    
    void set_capabilities(const ServerCapabilities& capabilities) {
        server_capabilities_ = capabilities;
        MCP_LOG_DEBUG("Set server capabilities");
    }
    
    void add_tool(const Tool& tool) {
        std::lock_guard<std::mutex> lock(tools_mutex_);
        tools_.push_back(tool);
        MCP_LOG_DEBUG("Added tool: " + tool.name);
    }
    
    void remove_tool(const std::string& name) {
        std::lock_guard<std::mutex> lock(tools_mutex_);
        tools_.erase(
            std::remove_if(tools_.begin(), tools_.end(),
                [&name](const Tool& tool) { return tool.name == name; }),
            tools_.end());
        MCP_LOG_DEBUG("Removed tool: " + name);
    }
    
    void clear_tools() {
        std::lock_guard<std::mutex> lock(tools_mutex_);
        tools_.clear();
        MCP_LOG_DEBUG("Cleared all tools");
    }
    
    void add_resource(const Resource& resource) {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        resources_.push_back(resource);
        MCP_LOG_DEBUG("Added resource: " + resource.uri);
    }
    
    void remove_resource(const std::string& uri) {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        resources_.erase(
            std::remove_if(resources_.begin(), resources_.end(),
                [&uri](const Resource& resource) { return resource.uri == uri; }),
            resources_.end());
        MCP_LOG_DEBUG("Removed resource: " + uri);
    }
    
    void clear_resources() {
        std::lock_guard<std::mutex> lock(resources_mutex_);
        resources_.clear();
        MCP_LOG_DEBUG("Cleared all resources");
    }
    
    void add_prompt(const Prompt& prompt) {
        std::lock_guard<std::mutex> lock(prompts_mutex_);
        prompts_.push_back(prompt);
        MCP_LOG_DEBUG("Added prompt: " + prompt.name);
    }
    
    void remove_prompt(const std::string& name) {
        std::lock_guard<std::mutex> lock(prompts_mutex_);
        prompts_.erase(
            std::remove_if(prompts_.begin(), prompts_.end(),
                [&name](const Prompt& prompt) { return prompt.name == name; }),
            prompts_.end());
        MCP_LOG_DEBUG("Removed prompt: " + name);
    }
    
    void clear_prompts() {
        std::lock_guard<std::mutex> lock(prompts_mutex_);
        prompts_.clear();
        MCP_LOG_DEBUG("Cleared all prompts");
    }
    
    void notify_resources_changed() {
        MCP_LOG_DEBUG("Notifying resources changed");
        send_notification(methods::NOTIFICATION_RESOURCES_LIST_CHANGED, json::object());
    }
    
    void notify_tools_changed() {
        MCP_LOG_DEBUG("Notifying tools changed");
        send_notification(methods::NOTIFICATION_TOOLS_LIST_CHANGED, json::object());
    }
    
    void notify_prompts_changed() {
        MCP_LOG_DEBUG("Notifying prompts changed");
        send_notification(methods::NOTIFICATION_PROMPTS_LIST_CHANGED, json::object());
    }
    
private:
    bool running_;
    int next_id_;
    ServerInfo server_info_;
    ServerCapabilities server_capabilities_;
    ClientInfo client_info_;
    ClientCapabilities client_capabilities_;
    
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

// MCPServer public interface
MCPServer::MCPServer() : impl_(std::make_unique<Impl>()) {
    MCP_LOG_DEBUG("MCPServer created");
}

MCPServer::~MCPServer() {
    MCP_LOG_DEBUG("MCPServer destroyed");
}

bool MCPServer::start(const std::string& endpoint) {
    return impl_->start(endpoint);
}

void MCPServer::stop() {
    impl_->stop();
}

bool MCPServer::is_running() const {
    return impl_->is_running();
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

std::vector<Tool> MCPServer::list_tools() {
    return impl_->list_tools();
}

ToolCallResult MCPServer::call_tool(const ToolCallParams& params) {
    return impl_->call_tool(params);
}

std::vector<Resource> MCPServer::list_resources() {
    return impl_->list_resources();
}

json MCPServer::read_resource(const std::string& uri) {
    return impl_->read_resource(uri);
}

std::vector<Prompt> MCPServer::list_prompts() {
    return impl_->list_prompts();
}

json MCPServer::get_prompt(const std::string& name, const json& arguments) {
    return impl_->get_prompt(name, arguments);
}

void MCPServer::send_notification(const std::string& method, const json& params) {
    impl_->send_notification(method, params);
}

void MCPServer::set_notification_handler(const std::string& method, NotificationHandler handler) {
    impl_->set_notification_handler(method, handler);
}

void MCPServer::set_tool_call_handler(ToolCallHandler handler) {
    impl_->set_tool_call_handler(handler);
}

void MCPServer::set_resource_read_handler(ResourceReadHandler handler) {
    impl_->set_resource_read_handler(handler);
}

void MCPServer::set_prompt_get_handler(PromptGetHandler handler) {
    impl_->set_prompt_get_handler(handler);
}

void MCPServer::set_server_info(const std::string& name, const std::string& version) {
    impl_->set_server_info(name, version);
}

void MCPServer::set_capabilities(const ServerCapabilities& capabilities) {
    impl_->set_capabilities(capabilities);
}

void MCPServer::add_tool(const Tool& tool) {
    impl_->add_tool(tool);
}

void MCPServer::remove_tool(const std::string& name) {
    impl_->remove_tool(name);
}

void MCPServer::clear_tools() {
    impl_->clear_tools();
}

void MCPServer::add_resource(const Resource& resource) {
    impl_->add_resource(resource);
}

void MCPServer::remove_resource(const std::string& uri) {
    impl_->remove_resource(uri);
}

void MCPServer::clear_resources() {
    impl_->clear_resources();
}

void MCPServer::add_prompt(const Prompt& prompt) {
    impl_->add_prompt(prompt);
}

void MCPServer::remove_prompt(const std::string& name) {
    impl_->remove_prompt(name);
}

void MCPServer::clear_prompts() {
    impl_->clear_prompts();
}

void MCPServer::notify_resources_changed() {
    impl_->notify_resources_changed();
}

void MCPServer::notify_tools_changed() {
    impl_->notify_tools_changed();
}

void MCPServer::notify_prompts_changed() {
    impl_->notify_prompts_changed();
}

} // namespace mcp
