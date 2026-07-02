/**
 * @file mcp_server.h
 * @brief MCP Server Implementation
 * 
 * This file provides the server-side implementation of the Model Context Protocol (MCP).
 * Handles server startup, client connections, and request processing.
 */

#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include "mcp_protocol_core.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>

namespace mcp {

// MCP Server Implementation
class MCPServer {
public:
    MCPServer();
    virtual ~MCPServer();
    
    // Server management
    bool start(const std::string& endpoint = "");
    void stop();
    bool is_running() const;
    
    // Connection management
    bool is_connected() const { return is_running(); }
    void disconnect() { stop(); }
    
    // Protocol implementation
    InitializeResult initialize(const InitializeParams& params);
    void initialized();
    json ping();
    
    // Tool methods
    std::vector<Tool> list_tools();
    ToolCallResult call_tool(const ToolCallParams& params);
    
    // Resource methods
    std::vector<Resource> list_resources();
    json read_resource(const std::string& uri);
    
    // Prompt methods
    std::vector<Prompt> list_prompts();
    json get_prompt(const std::string& name, const json& arguments = json::object());
    
    // Notification methods
    void send_notification(const std::string& method, const json& params = json::object());
    void set_notification_handler(const std::string& method, NotificationHandler handler);
    
    // Handler registration
    void set_tool_call_handler(ToolCallHandler handler);
    void set_resource_read_handler(ResourceReadHandler handler);
    void set_prompt_get_handler(PromptGetHandler handler);
    
    // Server configuration
    void set_server_info(const std::string& name, const std::string& version);
    void set_capabilities(const ServerCapabilities& capabilities);
    
    // Tool management
    void add_tool(const Tool& tool);
    void remove_tool(const std::string& name);
    void clear_tools();
    
    // Resource management
    void add_resource(const Resource& resource);
    void remove_resource(const std::string& uri);
    void clear_resources();
    
    // Prompt management
    void add_prompt(const Prompt& prompt);
    void remove_prompt(const std::string& name);
    void clear_prompts();
    
    // Client management
    void notify_resources_changed();
    void notify_tools_changed();
    void notify_prompts_changed();
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace mcp

#endif // MCP_SERVER_H
