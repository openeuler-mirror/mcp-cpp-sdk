/**
 * @file mcp_protocol_messages.h
 * @brief Helper functions for creating MCP protocol messages
 * 
 * This file provides convenience functions for creating common MCP protocol
 * messages as JSON-RPC 2.0 requests.
 */

#ifndef MCP_PROTOCOL_MESSAGES_H
#define MCP_PROTOCOL_MESSAGES_H

#include "mcp_message.h"
#include "mcp_protocol_core.h"
#include <string>
#include <vector>
#include <utility>

namespace mcp {

/**
 * @brief Create an initialize message
 */
inline std::string createInitializeMessage(const std::string& client_name, const std::string& client_version, int id = 1) {
    InitializeParams params;
    params.clientInfo.name = client_name;
    params.clientInfo.version = client_version;
    params.capabilities = utils::createDefaultClientCapabilities();
    
    request req = request::createWithId(std::to_string(id), methods::INITIALIZE, params.toJson());
    return req.toJson().dump();
}

/**
 * @brief Create a ping message
 */
inline std::string createPingMessage(int id = 1) {
    request req = request::createWithId(std::to_string(id), methods::PING, json::object());
    return req.toJson().dump();
}

/**
 * @brief Create a tools/list message
 */
inline std::string createListToolsMessage(int id = 1) {
    request req = request::createWithId(std::to_string(id), methods::TOOLS_LIST, json::object());
    return req.toJson().dump();
}

/**
 * @brief Create a resources/list message
 */
inline std::string createListResourcesMessage(int id = 1) {
    request req = request::createWithId(std::to_string(id), methods::RESOURCES_LIST, json::object());
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for list_allowed_directories
 */
inline std::string createListAllowedDirectoriesMessage(int id = 1) {
    json params = {
        {"name", "list_allowed_directories"},
        {"arguments", json::object()}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for write_file
 */
inline std::string createWriteFileMessage(const std::string& path, const std::string& content, int id = 1) {
    json params = {
        {"name", "write_file"},
        {"arguments", {
            {"path", path},
            {"content", content}
        }}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for create_directory
 */
inline std::string createCreateDirectoryMessage(const std::string& path, int id = 1) {
    json params = {
        {"name", "create_directory"},
        {"arguments", {
            {"path", path}
        }}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for list_directory
 */
inline std::string createListDirectoryMessage(const std::string& path, int id = 1) {
    json params = {
        {"name", "list_directory"},
        {"arguments", {
            {"path", path}
        }}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for read_text_file
 */
inline std::string createReadTextFileMessage(const std::string& path, int start_line = -1, int end_line = -1, int id = 1) {
    json arguments = {
        {"path", path}
    };
    if (start_line >= 0) {
        arguments["startLine"] = start_line;
    }
    if (end_line >= 0) {
        arguments["endLine"] = end_line;
    }
    
    json params = {
        {"name", "read_text_file"},
        {"arguments", arguments}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for get_file_info
 */
inline std::string createGetFileInfoMessage(const std::string& path, int id = 1) {
    json params = {
        {"name", "get_file_info"},
        {"arguments", {
            {"path", path}
        }}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for edit_file
 */
inline std::string createEditFileMessage(const std::string& path, 
                                         const std::vector<std::pair<std::string, std::string>>& edits,
                                         bool create_if_not_exists = false,
                                         int id = 1) {
    json edits_array = json::array();
    for (const auto& edit : edits) {
        edits_array.push_back({
            {"oldText", edit.first},
            {"newText", edit.second}
        });
    }
    
    json params = {
        {"name", "edit_file"},
        {"arguments", {
            {"path", path},
            {"edits", edits_array},
            {"createIfNotExists", create_if_not_exists}
        }}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message for search_files
 */
inline std::string createSearchFilesMessage(const std::string& directory, 
                                            const std::string& pattern,
                                            const std::vector<std::string>& exclude_patterns = {},
                                            int id = 1) {
    json arguments = {
        {"directory", directory},
        {"pattern", pattern}
    };
    
    if (!exclude_patterns.empty()) {
        arguments["excludePatterns"] = exclude_patterns;
    }
    
    json params = {
        {"name", "search_files"},
        {"arguments", arguments}
    };
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params);
    return req.toJson().dump();
}

/**
 * @brief Create a resources/read message (backward compatible)
 */
inline std::string createReadFileMessage(const std::string& uri, int start_line = -1, int end_line = -1, int id = 1) {
    json arguments = {
        {"uri", uri}
    };
    if (start_line >= 0) {
        arguments["startLine"] = start_line;
    }
    if (end_line >= 0) {
        arguments["endLine"] = end_line;
    }
    
    json params = arguments;
    request req = request::createWithId(std::to_string(id), methods::RESOURCES_READ, params);
    return req.toJson().dump();
}

// ========== 2025-11-25 Extended Functions ==========

/**
 * @brief Create an initialize message with optional _meta (2025-11-25)
 */
inline std::string createInitializeMessage2025(const std::string& client_name, const std::string& client_version, 
                                                const json& progressToken = nullptr, int id = 1) {
    InitializeParams params;
    params.clientInfo.name = client_name;
    params.clientInfo.version = client_version;
    params.capabilities = utils::createDefaultClientCapabilities();
    if (!progressToken.is_null()) {
        params._meta.progressToken = progressToken;
    }
    request req = request::createWithId(std::to_string(id), methods::INITIALIZE, params.toJson());
    return req.toJson().dump();
}

/**
 * @brief Create a tools/call message with optional _meta and task (2025-11-25)
 */
inline std::string createToolCallMessage2025(const std::string& name, const json& arguments, 
                                              const ToolCallParams::TaskMetadataParams& task = ToolCallParams::TaskMetadataParams(),
                                              const json& progressToken = nullptr, int id = 1) {
    ToolCallParams params;
    params.name = name;
    params.arguments = arguments;
    params.task = task;
    if (!progressToken.is_null()) {
        params._meta.progressToken = progressToken;
    }
    request req = request::createWithId(std::to_string(id), methods::TOOLS_CALL, params.toJson());
    return req.toJson().dump();
}

/**
 * @brief Create a tasks/get message (new in 2025-11-25)
 */
inline std::string createGetTaskMessage(const std::string& taskId, int id = 1) {
    json params = {{"taskId", taskId}};
    request req = request::createWithId(std::to_string(id), methods::TASKS_GET, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tasks/result message (new in 2025-11-25)
 */
inline std::string createTaskResultMessage(const std::string& taskId, int id = 1) {
    json params = {{"taskId", taskId}};
    request req = request::createWithId(std::to_string(id), methods::TASKS_RESULT, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tasks/list message (new in 2025-11-25)
 */
inline std::string createListTasksMessage(const std::string& cursor = "", const json& progressToken = nullptr, int id = 1) {
    json params = json::object();
    if (!cursor.empty()) {
        params["cursor"] = cursor;
    }
    if (!progressToken.is_null()) {
        params["_meta"] = MetaData().fromJson({{"progressToken", progressToken}}).toJson();
    }
    request req = request::createWithId(std::to_string(id), methods::TASKS_LIST, params);
    return req.toJson().dump();
}

/**
 * @brief Create a tasks/cancel message (new in 2025-11-25)
 */
inline std::string createCancelTaskMessage(const std::string& taskId, int id = 1) {
    json params = {{"taskId", taskId}};
    request req = request::createWithId(std::to_string(id), methods::TASKS_CANCEL, params);
    return req.toJson().dump();
}

/**
 * @brief Create an elicitation/create message (new in 2025-11-25)
 */
inline std::string createElicitationMessage(const json& params_data, int id = 1) {
    // params_data should be either ElicitRequestURLParams or ElicitRequestFormParams as JSON
    request req = request::createWithId(std::to_string(id), methods::ELICITATION_CREATE, params_data);
    return req.toJson().dump();
}

/**
 * @brief Create a ping message with optional _meta (2025-11-25)
 */
inline std::string createPingMessage2025(const json& progressToken = nullptr, int id = 1) {
    json params = json::object();
    if (!progressToken.is_null()) {
        params["_meta"] = MetaData().fromJson({{"progressToken", progressToken}}).toJson();
    }
    request req = request::createWithId(std::to_string(id), methods::PING, params);
    return req.toJson().dump();
}

} // namespace mcp

#endif // MCP_PROTOCOL_MESSAGES_H

