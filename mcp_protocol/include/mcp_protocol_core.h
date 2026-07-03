/**
 * @file mcp_protocol_core.h
 * @brief MCP Protocol Core Definitions and Utilities
 * 
 * This file contains the core protocol definitions, data structures,
 * and utility functions for the Model Context Protocol (MCP).
 * Implements the 2024-11-05 and 2025-11-25 protocol specifications.
 * Supports backward compatibility between versions.
 */

#ifndef MCP_PROTOCOL_CORE_H
#define MCP_PROTOCOL_CORE_H

#include "mcp_message.h"
#include "../../log/include/logger.h"
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
    
    // Task methods (new in 2025-11-25)
    constexpr const char* TASKS_GET = "tasks/get";
    constexpr const char* TASKS_RESULT = "tasks/result";
    constexpr const char* TASKS_LIST = "tasks/list";
    constexpr const char* TASKS_CANCEL = "tasks/cancel";
    
    // Elicitation methods (new in 2025-11-25)
    constexpr const char* ELICITATION_CREATE = "elicitation/create";
    
    // Completion methods
    constexpr const char* COMPLETION_COMPLETE = "completion/complete";
    
    // Sampling methods
    constexpr const char* SAMPLING_CREATE_MESSAGE = "sampling/createMessage";
    
    // Resources methods
    constexpr const char* RESOURCES_SUBSCRIBE = "resources/subscribe";
    constexpr const char* RESOURCES_UNSUBSCRIBE = "resources/unsubscribe";
    
    // Roots methods
    constexpr const char* ROOTS_LIST = "roots/list";
    
    // Notifications
    constexpr const char* NOTIFICATION_INITIALIZED = "notifications/initialized";
    constexpr const char* NOTIFICATION_CANCELLED = "notifications/cancelled";
    constexpr const char* NOTIFICATION_RESOURCES_LIST_CHANGED = "notifications/resources/list_changed";
    constexpr const char* NOTIFICATION_RESOURCE_UPDATED = "notifications/resources/updated";
    constexpr const char* NOTIFICATION_PROMPTS_LIST_CHANGED = "notifications/prompts/list_changed";
    constexpr const char* NOTIFICATION_TOOLS_LIST_CHANGED = "notifications/tools/list_changed";
    constexpr const char* NOTIFICATION_ROOTS_LIST_CHANGED = "notifications/roots/list_changed";
    constexpr const char* NOTIFICATION_MESSAGE = "notifications/message";
    constexpr const char* NOTIFICATION_PROGRESS = "notifications/progress";
    
    // Task notifications (new in 2025-11-25)
    constexpr const char* NOTIFICATION_TASKS_STATUS = "notifications/tasks/status";
    
    // Elicitation notifications (new in 2025-11-25)
    constexpr const char* NOTIFICATION_ELICITATION_COMPLETE = "notifications/elicitation/complete";
}

// MCP Protocol Version Constants
constexpr const char* PROTOCOL_VERSION_2024_11_05 = "2024-11-05";
constexpr const char* PROTOCOL_VERSION_2025_11_25 = "2025-11-25";
constexpr const char* PROTOCOL_VERSION = PROTOCOL_VERSION_2024_11_05; // Default for backward compatibility

// Client Capabilities
struct ClientCapabilities {
    json logging = json::object();
    json prompts = json::object();
    json resources = json::object();
    json tools = json::object();
    json sampling = json::object();
    
    json toJson() const {
        json caps;
        if (!logging.empty()) caps["logging"] = logging;
        if (!prompts.empty()) caps["prompts"] = prompts;
        if (!resources.empty()) caps["resources"] = resources;
        if (!tools.empty()) caps["tools"] = tools;
        if (!sampling.empty()) caps["sampling"] = sampling;
        return caps;
    }
    
    static ClientCapabilities fromJson(const json& j) {
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
    
    json toJson() const {
        json caps;
        if (!experimental.empty()) caps["experimental"] = experimental;
        if (!prompts.empty()) caps["prompts"] = prompts;
        if (!resources.empty()) caps["resources"] = resources;
        if (!tools.empty()) caps["tools"] = tools;
        if (!logging.empty()) caps["logging"] = logging;
        if (!sampling.empty()) caps["sampling"] = sampling;
        return caps;
    }
    
    static ServerCapabilities fromJson(const json& j) {
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
    
    json toJson() const {
        return {
            {"name", name},
            {"version", version}
        };
    }
    
    static ClientInfo fromJson(const json& j) {
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
    
    json toJson() const {
        return {
            {"name", name},
            {"version", version}
        };
    }
    
    static ServerInfo fromJson(const json& j) {
        ServerInfo info;
        info.name = j.value("name", "");
        info.version = j.value("version", "");
        return info;
    }
};

// ========== 2025-11-25 Common Types (defined early for use in other structures) ==========

// Metadata structure for _meta field (2025-11-25)
struct MetaData {
    json progressToken = nullptr;
    
    json toJson() const {
        json j = json::object();
        if (!progressToken.is_null()) {
            j["progressToken"] = progressToken;
        }
        return j;
    }
    
    static MetaData fromJson(const json& j) {
        MetaData meta;
        if (j.contains("progressToken")) {
            meta.progressToken = j["progressToken"];
        }
        return meta;
    }
    
    bool empty() const {
        return progressToken.is_null();
    }
};

// Initialize Request Parameters (extended for 2025-11-25 with optional _meta)
struct InitializeParams {
    std::string protocolVersion = PROTOCOL_VERSION;
    ClientCapabilities capabilities;
    ClientInfo clientInfo;
    MetaData _meta; // Optional field for 2025-11-25
    
    json toJson() const {
        json j = {
            {"protocolVersion", protocolVersion},
            {"capabilities", capabilities.toJson()},
            {"clientInfo", clientInfo.toJson()}
        };
        // Only include _meta if not empty (2025-11-25)
        if (!_meta.empty()) {
            j["_meta"] = _meta.toJson();
        }
        return j;
    }
    
    static InitializeParams fromJson(const json& j) {
        InitializeParams params;
        params.protocolVersion = j.value("protocolVersion", PROTOCOL_VERSION);
        if (j.contains("capabilities")) {
            params.capabilities = ClientCapabilities::fromJson(j["capabilities"]);
        }
        if (j.contains("clientInfo")) {
            params.clientInfo = ClientInfo::fromJson(j["clientInfo"]);
        }
        // Parse _meta if present (2025-11-25)
        if (j.contains("_meta")) {
            params._meta = MetaData::fromJson(j["_meta"]);
        }
        return params;
    }
};

// Initialize Response Result (extended for 2025-11-25 with optional instructions)
struct InitializeResult {
    std::string protocolVersion = PROTOCOL_VERSION;
    ServerCapabilities capabilities;
    ServerInfo serverInfo;
    std::string instructions; // Optional field for 2025-11-25
    
    json toJson() const {
        json j = {
            {"protocolVersion", protocolVersion},
            {"capabilities", capabilities.toJson()},
            {"serverInfo", serverInfo.toJson()}
        };
        // Only include instructions if not empty (2025-11-25)
        if (!instructions.empty()) {
            j["instructions"] = instructions;
        }
        return j;
    }
    
    static InitializeResult fromJson(const json& j) {
        InitializeResult result;
        result.protocolVersion = j.value("protocolVersion", PROTOCOL_VERSION);
        if (j.contains("capabilities")) {
            result.capabilities = ServerCapabilities::fromJson(j["capabilities"]);
        }
        if (j.contains("serverInfo")) {
            result.serverInfo = ServerInfo::fromJson(j["serverInfo"]);
        }
        // Parse instructions if present (2025-11-25)
        if (j.contains("instructions")) {
            result.instructions = j.value("instructions", "");
        }
        return result;
    }
};

// Tool Definition
struct Tool {
    std::string name;
    std::string description;
    json inputSchema;
    
    json toJson() const {
        return {
            {"name", name},
            {"description", description},
            {"inputSchema", inputSchema}
        };
    }
    
    static Tool fromJson(const json& j) {
        Tool tool;
        tool.name = j.value("name", "");
        tool.description = j.value("description", "");
        tool.inputSchema = j.value("inputSchema", json::object());
        return tool;
    }
};

// Tool Call Parameters (extended for 2025-11-25 with optional _meta and task)
// Note: TaskMetadata is defined later in this file, so we use forward declaration
struct ToolCallParams {
    std::string name;
    json arguments;
    MetaData _meta; // Optional field for 2025-11-25
    
    // Forward declaration - TaskMetadata will be defined later
    struct TaskMetadataParams {
        int ttl = 0; // Time to live in seconds
        
        json toJson() const {
            json j = json::object();
            if (ttl > 0) {
                j["ttl"] = ttl;
            }
            return j;
        }
        
        static TaskMetadataParams fromJson(const json& j) {
            TaskMetadataParams metadata;
            metadata.ttl = j.value("ttl", 0);
            return metadata;
        }
        
        bool empty() const {
            return ttl == 0;
        }
    };
    TaskMetadataParams task; // Optional field for 2025-11-25
    
    json toJson() const {
        json j = {
            {"name", name},
            {"arguments", arguments}
        };
        // Only include _meta if not empty (2025-11-25)
        if (!_meta.empty()) {
            j["_meta"] = _meta.toJson();
        }
        // Only include task if not empty (2025-11-25)
        if (!task.empty()) {
            j["task"] = task.toJson();
        }
        return j;
    }
    
    static ToolCallParams fromJson(const json& j) {
        ToolCallParams params;
        params.name = j.value("name", "");
        params.arguments = j.value("arguments", json::object());
        // Parse _meta if present (2025-11-25)
        if (j.contains("_meta")) {
            params._meta = MetaData::fromJson(j["_meta"]);
        }
        // Parse task if present (2025-11-25)
        if (j.contains("task")) {
            params.task = ToolCallParams::TaskMetadataParams::fromJson(j["task"]);
        }
        return params;
    }
};

// Tool Call Result (extended for 2025-11-25 with structuredContent)
struct ToolCallResult {
    json content;
    bool isError = false;
    std::string errorMessage;
    json structuredContent; // Optional field for 2025-11-25
    
    json toJson() const {
        json j;
        if (isError) {
            j["isError"] = true;
            j["error"] = errorMessage;
            // MCP protocol requires content field even for errors
            // Include error message as text content item
            json content_item;
            content_item["type"] = "text";
            content_item["text"] = errorMessage;
            j["content"] = json::array({content_item});
        } else {
            j["isError"] = false;
            // Convert content to MCP protocol format (array of content items)
            if (content.is_array()) {
                // Already in correct format
                j["content"] = content;
            } else if (content.is_object() || content.is_string() || content.is_number() || content.is_boolean()) {
                // Convert to text content item
                json content_item;
                content_item["type"] = "text";
                if (content.is_string()) {
                    content_item["text"] = content.get<std::string>();
                } else {
                    // Serialize object/number/boolean to JSON string
                    content_item["text"] = content.dump();
                }
                j["content"] = json::array({content_item});
            } else {
                // Empty or null content - return empty array
                j["content"] = json::array();
            }
        }
        // Only include structuredContent if not empty (2025-11-25)
        if (!structuredContent.empty()) {
            j["structuredContent"] = structuredContent;
        }
        return j;
    }
    
    static ToolCallResult fromJson(const json& j) {
        ToolCallResult result;
        if (j.contains("isError")) {
            result.isError = j["isError"].get<bool>();
        } else if (j.contains("error")) {
            result.isError = true;
            result.errorMessage = j.value("error", "");
        }
        if (j.contains("content")) {
            result.content = j["content"];
        }
        // Parse structuredContent if present (2025-11-25)
        if (j.contains("structuredContent")) {
            result.structuredContent = j["structuredContent"];
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
    
    json toJson() const {
        return {
            {"uri", uri},
            {"name", name},
            {"description", description},
            {"mimeType", mimeType}
        };
    }
    
    static Resource fromJson(const json& j) {
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
    
    json toJson() const {
        return {
            {"name", name},
            {"description", description},
            {"arguments", arguments}
        };
    }
    
    static Prompt fromJson(const json& j) {
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

// ========== 2025-11-25 New Types and Extensions ==========

// Common Types (2025-11-25)
using RequestId = json; // string | integer
using ProgressToken = json; // string | integer
using Cursor = std::string;

// Role enum
enum class Role {
    assistant,
    user
};

inline std::string roleToString(Role role) {
    return role == Role::assistant ? "assistant" : "user";
}

inline Role roleFromString(const std::string& role) {
    return role == "assistant" ? Role::assistant : Role::user;
}

// Content Types (2025-11-25)
struct TextContent {
    std::string type = "text";
    std::string text;
    
    json toJson() const {
        return {{"type", type}, {"text", text}};
    }
    
    static TextContent fromJson(const json& j) {
        TextContent content;
        content.type = j.value("type", "text");
        content.text = j.value("text", "");
        return content;
    }
};

struct ImageContent {
    std::string type = "image";
    std::string data;
    std::string mimeType;
    
    json toJson() const {
        return {{"type", type}, {"data", data}, {"mimeType", mimeType}};
    }
    
    static ImageContent fromJson(const json& j) {
        ImageContent content;
        content.type = j.value("type", "image");
        content.data = j.value("data", "");
        content.mimeType = j.value("mimeType", "");
        return content;
    }
};

// AudioContent (new in 2025-11-25)
struct AudioContent {
    std::string type = "audio";
    std::string data;
    std::string mimeType;
    
    json toJson() const {
        return {{"type", type}, {"data", data}, {"mimeType", mimeType}};
    }
    
    static AudioContent fromJson(const json& j) {
        AudioContent content;
        content.type = j.value("type", "audio");
        content.data = j.value("data", "");
        content.mimeType = j.value("mimeType", "");
        return content;
    }
};

// ResourceLink (new in 2025-11-25)
struct ResourceLink {
    std::string type = "resource";
    std::string uri;
    std::string text; // Optional
    
    json toJson() const {
        json j = {{"type", type}, {"uri", uri}};
        if (!text.empty()) j["text"] = text;
        return j;
    }
    
    static ResourceLink fromJson(const json& j) {
        ResourceLink link;
        link.type = j.value("type", "resource");
        link.uri = j.value("uri", "");
        link.text = j.value("text", "");
        return link;
    }
};

// Task Types (new in 2025-11-25)
enum class TaskStatus {
    working,
    completed,
    failed,
    cancelled,
    input_required
};

inline std::string taskStatusToString(TaskStatus status) {
    switch (status) {
        case TaskStatus::working: return "working";
        case TaskStatus::completed: return "completed";
        case TaskStatus::failed: return "failed";
        case TaskStatus::cancelled: return "cancelled";
        case TaskStatus::input_required: return "input_required";
        default: return "working";
    }
}

inline TaskStatus taskStatusFromString(const std::string& status) {
    if (status == "working") return TaskStatus::working;
    if (status == "completed") return TaskStatus::completed;
    if (status == "failed") return TaskStatus::failed;
    if (status == "cancelled") return TaskStatus::cancelled;
    if (status == "input_required") return TaskStatus::input_required;
    return TaskStatus::working;
}

// TaskMetadata (also defined in ToolCallParams, but standalone version for general use)
struct TaskMetadata {
    int ttl = 0;
    
    json toJson() const {
        json j = json::object();
        if (ttl > 0) j["ttl"] = ttl;
        return j;
    }
    
    static TaskMetadata fromJson(const json& j) {
        TaskMetadata metadata;
        metadata.ttl = j.value("ttl", 0);
        return metadata;
    }
    
    bool empty() const { return ttl == 0; }
};

// Task (new in 2025-11-25)
struct Task {
    std::string taskId;
    TaskStatus status;
    std::string createdAt;
    std::string lastUpdatedAt;
    std::string statusMessage;
    int ttl = 0;
    int pollInterval = 0;
    
    json toJson() const {
        json j = {
            {"taskId", taskId},
            {"status", taskStatusToString(status)},
            {"createdAt", createdAt},
            {"lastUpdatedAt", lastUpdatedAt}
        };
        if (!statusMessage.empty()) j["statusMessage"] = statusMessage;
        if (ttl > 0) j["ttl"] = ttl;
        if (pollInterval > 0) j["pollInterval"] = pollInterval;
        return j;
    }
    
    static Task fromJson(const json& j) {
        Task task;
        task.taskId = j.value("taskId", "");
        task.status = taskStatusFromString(j.value("status", "working"));
        task.createdAt = j.value("createdAt", "");
        task.lastUpdatedAt = j.value("lastUpdatedAt", "");
        task.statusMessage = j.value("statusMessage", "");
        task.ttl = j.value("ttl", 0);
        task.pollInterval = j.value("pollInterval", 0);
        return task;
    }
};

// Elicitation Types (new in 2025-11-25)
struct ElicitRequestURLParams {
    std::string mode = "url";
    std::string url;
    std::string message;
    
    json toJson() const {
        return {{"mode", mode}, {"url", url}, {"message", message}};
    }
    
    static ElicitRequestURLParams fromJson(const json& j) {
        ElicitRequestURLParams params;
        params.mode = j.value("mode", "url");
        params.url = j.value("url", "");
        params.message = j.value("message", "");
        return params;
    }
};

struct ElicitRequestFormParams {
    std::string mode = "form";
    std::string message;
    json fields;
    
    json toJson() const {
        return {{"mode", mode}, {"message", message}, {"fields", fields}};
    }
    
    static ElicitRequestFormParams fromJson(const json& j) {
        ElicitRequestFormParams params;
        params.mode = j.value("mode", "form");
        params.message = j.value("message", "");
        params.fields = j.value("fields", json::object());
        return params;
    }
};

struct ElicitCreateResult {
    std::string action; // "accept" | "decline" | "cancel"
    json content; // Only present when action="accept" and mode="form"
    
    json toJson() const {
        json j = {{"action", action}};
        if (!content.empty() && action == "accept") {
            j["content"] = content;
        }
        return j;
    }
    
    static ElicitCreateResult fromJson(const json& j) {
        ElicitCreateResult result;
        result.action = j.value("action", "cancel");
        if (j.contains("content")) {
            result.content = j["content"];
        }
        return result;
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
    ClientCapabilities createDefaultClientCapabilities();
    
    // Create default server capabilities
    ServerCapabilities createDefaultServerCapabilities();
    
    // Validate protocol version
    bool isValidProtocolVersion(const std::string& version);
    
    // Create tool from JSON schema
    Tool createToolFromSchema(const std::string& name, const std::string& description, const json& schema);
    
    // Create resource from URI
    Resource createResourceFromUri(const std::string& uri, const std::string& name = "", const std::string& description = "");
}

} // namespace mcp

#endif // MCP_PROTOCOL_CORE_H
