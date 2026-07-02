/**
 * @file mcp_client.h
 * @brief MCP Client Implementation
 * 
 * This file provides the client-side implementation of the Model Context Protocol (MCP).
 * Handles client connections, requests, and responses.
 */

#ifndef MCP_CLIENT_H
#define MCP_CLIENT_H

#include "mcp_protocol_core.h"
#include "../transport/transport.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <mutex>

namespace mcp {

// MCP Client Implementation
class MCPClient {
public:
    MCPClient();
    virtual ~MCPClient();
    
    // Connection management
    bool connect(const std::string& endpoint);
    bool connect_with_config(const mcp_transport::TransportConfig& config);
    void disconnect();
    bool is_connected() const;
    
    // Transport configuration
    void set_transport_type(mcp_transport::TransportType type);
    mcp_transport::TransportType get_transport_type() const;
    mcp_transport::TransportStatus get_transport_status() const;
    
    // Protocol implementation
    InitializeResult initialize(const InitializeParams& params);
    void initialized();
    json ping();
    
    // Tool methods
    std::vector<Tool> list_tools();
    ToolCallResult call_tool(const ToolCallParams& params);
    ToolCallResult call_tool(const std::string& name, const json& arguments = json::object());
    
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
    
    // Configuration
    void set_client_info(const std::string& name, const std::string& version);
    void set_capabilities(const ClientCapabilities& capabilities);
    
    // Endpoint management
    void set_endpoints(const std::vector<std::string>& endpoints);
    void set_current_endpoint(const std::string& endpoint);
    std::string get_current_endpoint() const;
    std::vector<std::string> get_endpoints() const;
    
    // Server info
    ServerInfo get_server_info() const;
    ServerCapabilities get_server_capabilities() const;
 


private:
    class Impl;
    //std::unique_ptr<Impl> impl_;
protected:
    std::unique_ptr<Impl> impl_;
};
} // namespace mcp

#endif // MCP_CLIENT_H
