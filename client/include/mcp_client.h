/**
 * @file mcp_client.h
 * @brief MCP Client Implementation
 * 
 * This file provides the client-side implementation of the Model Context Protocol (MCP).
 * Handles client connections, requests, and responses.
 */

#ifndef MCP_CLIENT_H
#define MCP_CLIENT_H

#include "../../mcp_protocol/include/mcp_protocol_core.h"
#include "../../transport/include/transport_base.h"
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
    bool connectWithConfig(const mcp_transport::TransportConfig& config);
    void disconnect();
    bool isConnected() const;
    
    // Transport configuration
    void setTransportType(mcp_transport::TransportType type);
    mcp_transport::TransportType getTransportType() const;
    mcp_transport::TransportStatus getTransportStatus() const;
    
    // Protocol implementation
    InitializeResult initialize(const InitializeParams& params);
    void initialized();
    json ping();
    
    // Tool methods
    std::vector<Tool> listTools();
    ToolCallResult callTool(const ToolCallParams& params);
    ToolCallResult callTool(const std::string& name, const json& arguments = json::object());
    
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
    
    // Connection callback
    using ConnectionCallback = std::function<void(const mcp_transport::ConnectionInfo& info)>;
    void setConnectionCallback(ConnectionCallback callback);
    
    // Configuration
    void setClientInfo(const std::string& name, const std::string& version);
    void setCapabilities(const ClientCapabilities& capabilities);
    
    // Endpoint management
    void setEndpoints(const std::vector<std::string>& endpoints);
    void setCurrentEndpoint(const std::string& endpoint);
    std::string getCurrentEndpoint() const;
    std::vector<std::string> getEndpoints() const;
    
    // Server info
    ServerInfo getServerInfo() const;
    ServerCapabilities getServerCapabilities() const;
    
    // Session management
    std::string getSessionId() const;


private:
    class Impl;
    //std::unique_ptr<Impl> impl_;
protected:
    std::unique_ptr<Impl> impl_;
};
} // namespace mcp

#endif // MCP_CLIENT_H
