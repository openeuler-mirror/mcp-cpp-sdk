/**
 * @file mcp_protocol_core.h
 * @brief MCP Protocol Core Definitions and Utilities
 * 
 * This file contains the core protocol definitions, data structures,
 * and utility functions for the Model Context Protocol (MCP).
 * Implements the 2024-11-05 protocol specification.
 */

#ifndef MCP_PROTOCOL_CORE_H
#define MCP_PROTOCOL_CORE_H

#include "mcp_message.h"
#include "../log/logger.h"
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>

namespace mcp {

// MCP Protocol Methods
namespace methods {
    // Lifecycle methods
    constexpr const char* INITIALIZE = "initialize";
    constexpr const char* PING = "ping";
    
    // Resource methods
    constexpr const char* RESOURCES_LIST = "resources/list";
    constexpr const char* RESOURCES_READ = "resources/read";
    constexpr const char* RESOURCES_TEMPLATES_LIST = "resources/templates/list";
    
    // Tool methods
    constexpr const char* TOOLS_LIST = "tools/list";
    constexpr const char* TOOLS_CALL = "tools/call";
    
    // Prompt methods
    constexpr const char* PROMPTS_LIST = "prompts/list";
    constexpr const char* PROMPTS_GET = "prompts/get";
    
    // Logging methods
    constexpr const char* LOGGING_SET_LEVEL = "logging/setLevel";
    
    // Notifications
    constexpr const char* NOTIFICATION_INITIALIZED = "notifications/initialized";
    constexpr const char* NOTIFICATION_RESOURCES_LIST_CHANGED = "notifications/resources/list_changed";
    constexpr const char* NOTIFICATION_RESOURCE_UPDATED = "notifications/resources/updated";
    constexpr const char* NOTIFICATION_PROMPTS_LIST_CHANGED = "notifications/prompts/list_changed";
    constexpr const char* NOTIFICATION_TOOLS_LIST_CHANGED = "notifications/tools/list_changed";
}

// MCP Protocol Version
constexpr const char* PROTOCOL_VERSION = "2024-11-05";

// Client Capabilities
struct ClientCapabilities {
    json logging = json::object();
    json prompts = json::object();
    json resources = json::object();
    json tools = json::object();
    json sampling = json::object();
    
    json to_json() const {
        json caps;
        if (!logging.empty()) caps["logging"] = logging;
        if (!prompts.empty()) caps["prompts"] = prompts;
        if (!resources.empty()) caps["resources"] = resources;
        if (!tools.empty()) caps["tools"] = tools;
        if (!sampling.empty()) caps["sampling"] = sampling;
        return caps;
    }
    
    static ClientCapabilities from_json(const json& j) {
        ClientCapabilities caps;
        if (j.contains("logging")) caps.logging = j["logging"];
        if (j.contains("prompts")) caps.prompts = j["prompts"];
        if (j.contains("resources")) caps.resources = j["resources"];
        if (j.contains("tools")) caps.tools = j["tools"];
        if (j.contains("sampling")) caps.sampling = j["sampling"];
        return caps;
    }
};

// Server Capabilities
struct ServerCapabilities {
    json experimental = json::object();
    json prompts = json::object();
    json resources = json::object();
    json tools = json::object();
    json logging = json::object();
    json sampling = json::object();
    
    json to_json() const {
        json caps;
        if (!experimental.empty()) caps["experimental"] = experimental;
        if (!prompts.empty()) caps["prompts"] = prompts;
        if (!resources.empty()) caps["resources"] = resources;
        if (!tools.empty()) caps["tools"] = tools;
        if (!logging.empty()) caps["logging"] = logging;
        if (!sampling.empty()) caps["sampling"] = sampling;
        return caps;
    }
    
    static ServerCapabilities from_json(const json& j) {
        ServerCapabilities caps;
        if (j.contains("experimental")) caps.experimental = j["experimental"];
        if (j.contains("prompts")) caps.prompts = j["prompts"];
        if (j.contains("resources")) caps.resources = j["resources"];
        if (j.contains("tools")) caps.tools = j["tools"];
        if (j.contains("logging")) caps.logging = j["logging"];
        if (j.contains("sampling")) caps.sampling = j["sampling"];
        return caps;
    }
};

// Client Info
struct ClientInfo {
    std::string name;
    std::string version;
    
    json to_json() const {
        return {
            {"name", name},
            {"version", version}
        };
    }
    
    static ClientInfo from_json(const json& j) {
        ClientInfo info;
        info.name = j.value("name", "");
        info.version = j.value("version", "");
        return info;
    }
};

// Server Info
struct ServerInfo {
    std::string name;
    std::string version;
    
    json to_json() const {
        return {
            {"name", name},
            {"version", version}
        };
    }
    
    static ServerInfo from_json(const json& j) {
        ServerInfo info;
        info.name = j.value("name", "");
        info.version = j.value("version", "");
        return info;
    }
};

// Initialize Request Parameters
struct InitializeParams {
    std::string protocolVersion = PROTOCOL_VERSION;
    ClientCapabilities capabilities;
    ClientInfo clientInfo;
    
    json to_json() const {
        return {
            {"protocolVersion", protocolVersion},
            {"capabilities", capabilities.to_json()},
            {"clientInfo", clientInfo.to_json()}
        };
    }
    
    static InitializeParams from_json(const json& j) {
        InitializeParams params;
        params.protocolVersion = j.value("protocolVersion", PROTOCOL_VERSION);
        if (j.contains("capabilities")) {
            params.capabilities = ClientCapabilities::from_json(j["capabilities"]);
        }
        if (j.contains("clientInfo")) {
            params.clientInfo = ClientInfo::from_json(j["clientInfo"]);
        }
        return params;
    }
};

// Initialize Response Result
struct InitializeResult {
    std::string protocolVersion = PROTOCOL_VERSION;
    ServerCapabilities capabilities;
    ServerInfo serverInfo;
    
    json to_json() const {
        return {
            {"protocolVersion", protocolVersion},
            {"capabilities", capabilities.to_json()},
            {"serverInfo", serverInfo.to_json()}
        };
    }
    
    static InitializeResult from_json(const json& j) {
        InitializeResult result;
        result.protocolVersion = j.value("protocolVersion", PROTOCOL_VERSION);
        if (j.contains("capabilities")) {
            result.capabilities = ServerCapabilities::from_json(j["capabilities"]);
        }
        if (j.contains("serverInfo")) {
            result.serverInfo = ServerInfo::from_json(j["serverInfo"]);
        }
        return result;
    }
};

// Tool Definition
struct Tool {
    std::string name;
    std::string description;
    json inputSchema;
    
    json to_json() const {
        return {
            {"name", name},
            {"description", description},
            {"inputSchema", inputSchema}
        };
    }
    
    static Tool from_json(const json& j) {
        Tool tool;
        tool.name = j.value("name", "");
        tool.description = j.value("description", "");
        tool.inputSchema = j.value("inputSchema", json::object());
        return tool;
    }
};

// Tool Call Parameters
struct ToolCallParams {
    std::string name;
    json arguments;
    
    json to_json() const {
        return {
            {"name", name},
            {"arguments", arguments}
        };
    }
    
    static ToolCallParams from_json(const json& j) {
        ToolCallParams params;
        params.name = j.value("name", "");
        params.arguments = j.value("arguments", json::object());
        return params;
    }
};

// Tool Call Result
struct ToolCallResult {
    json content;
    bool isError = false;
    std::string errorMessage;
    
    json to_json() const {
        if (isError) {
            return {
                {"error", errorMessage}
            };
        }
        return {
            {"content", content}
        };
    }
    
    static ToolCallResult from_json(const json& j) {
        ToolCallResult result;
        if (j.contains("error")) {
            result.isError = true;
            result.errorMessage = j.value("error", "");
        } else {
            result.content = j.value("content", json::object());
        }
        return result;
    }
};

// Resource Definition
struct Resource {
    std::string uri;
    std::string name;
    std::string description;
    std::string mimeType;
    
    json to_json() const {
        return {
            {"uri", uri},
            {"name", name},
            {"description", description},
            {"mimeType", mimeType}
        };
    }
    
    static Resource from_json(const json& j) {
        Resource resource;
        resource.uri = j.value("uri", "");
        resource.name = j.value("name", "");
        resource.description = j.value("description", "");
        resource.mimeType = j.value("mimeType", "");
        return resource;
    }
};

// Prompt Definition
struct Prompt {
    std::string name;
    std::string description;
    std::vector<std::string> arguments;
    
    json to_json() const {
        return {
            {"name", name},
            {"description", description},
            {"arguments", arguments}
        };
    }
    
    static Prompt from_json(const json& j) {
        Prompt prompt;
        prompt.name = j.value("name", "");
        prompt.description = j.value("description", "");
        if (j.contains("arguments") && j["arguments"].is_array()) {
            for (const auto& arg : j["arguments"]) {
                prompt.arguments.push_back(arg.get<std::string>());
            }
        }
        return prompt;
    }
};

// Notification Handlers
using NotificationHandler = std::function<void(const std::string& method, const json& params)>;
using ToolCallHandler = std::function<ToolCallResult(const ToolCallParams& params)>;
using ResourceReadHandler = std::function<json(const std::string& uri)>;
using PromptGetHandler = std::function<json(const std::string& name, const json& arguments)>;

// Utility functions
namespace utils {
    // Create default client capabilities
    ClientCapabilities create_default_client_capabilities();
    
    // Create default server capabilities
    ServerCapabilities create_default_server_capabilities();
    
    // Validate protocol version
    bool is_valid_protocol_version(const std::string& version);
    
    // Create tool from JSON schema
    Tool create_tool_from_schema(const std::string& name, const std::string& description, const json& schema);
    
    // Create resource from URI
    Resource create_resource_from_uri(const std::string& uri, const std::string& name = "", const std::string& description = "");
}

} // namespace mcp

#endif // MCP_PROTOCOL_CORE_H
