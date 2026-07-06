/**
 * @file mcp_server.h
 * @brief MCP Server Implementation
 * 
 * This file provides the server-side implementation of the Model Context Protocol (MCP).
 * Handles server startup, client connections, and request processing.
 */

#ifndef MCP_SERVER_H
#define MCP_SERVER_H

#include "../../mcp_protocol/include/mcp_protocol_core.h"
#include "../../transport/include/transport_base.h"
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
    bool startWithConfig(const mcp_transport::TransportConfig& config);
    void stop();
    bool isRunning() const;
    
    // Connection management
    bool isConnected() const { return isRunning(); }
    void disconnect() { stop(); }
    
    // Protocol implementation
    InitializeResult initialize(const InitializeParams& params);
    void initialized();
    json ping();
    
    // Tool methods
    std::vector<Tool> listTools();
    ToolCallResult callTool(const ToolCallParams& params);
    
    // Resource methods
    std::vector<Resource> listResources();
    json readResource(const std::string& uri);
    
    // Prompt methods
    std::vector<Prompt> listPrompts();
    json getPrompt(const std::string& name, const json& arguments = json::object());
    
    // Notification methods
    void sendNotification(const std::string& method, const json& params = json::object());
    void setNotificationHandler(const std::string& method, NotificationHandler handler);
    
    // Handler registration
    void setToolCallHandler(ToolCallHandler handler);
    void setResourceReadHandler(ResourceReadHandler handler);
    void setPromptGetHandler(PromptGetHandler handler);
    
    // Server configuration
    void setServerInfo(const std::string& name, const std::string& version);
    void setCapabilities(const ServerCapabilities& capabilities);
    void setDisableSessionValidation(bool disable);
    
    // Tool management
    void addTool(const Tool& tool);
    void removeTool(const std::string& name);
    void clearTools();
    
    // Resource management
    void addResource(const Resource& resource);
    void removeResource(const std::string& uri);
    void clearResources();
    
    // Prompt management
    void addPrompt(const Prompt& prompt);
    void removePrompt(const std::string& name);
    void clearPrompts();
    
    // Client management
    void notifyResourcesChanged();
    void notifyToolsChanged();
    void notifyPromptsChanged();
    
    // 虚拟方法，子类可以重写以自定义功能
    virtual void setupDefaultTools();
    virtual void setupDefaultResources();
    
public:
    class Impl;
protected:
    std::unique_ptr<Impl> impl_;
};

} // namespace mcp

#endif // MCP_SERVER_H
