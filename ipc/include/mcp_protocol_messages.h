#pragma once
#include <string>
#include <map>
#include <vector>
#include <sstream>

namespace mcp_protocol {

// Entity structure for create_entities tool
struct Entity {
    std::string name;
    std::string entityType;
    std::vector<std::string> observations;
    
    Entity(const std::string& n, const std::string& type, const std::vector<std::string>& obs)
        : name(n), entityType(type), observations(obs) {}
};

// MCP协议消息类型
enum class MCPMessageType {
    INITIALIZE,
    PING,
    PONG,
    NOTIFICATION,
    REQUEST,
    RESPONSE
};

// 生成MCP初始化消息
std::string create_initialize_message1(const std::string& client_name = "cpp-client", 
                                    const std::string& client_version = "1.0.0") {
    return R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {
                "logging": {},
                "prompts": {
                    "listChanged": true
                },
                "resources": {
                    "subscribe": true,
                }
    })";
}
std::string create_initialize_message(const std::string& client_name = "cpp-client", 
                                    const std::string& client_version = "1.0.0") {
    return R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "initialize",
        "params": {
            "protocolVersion": "2024-11-05",
            "capabilities": {
            },
            "clientInfo": {
                "name": "cpp-client",
                "version": "1.0.0"
            }
        }
    })";
}

std::string create_notifications_initialized() {
    return R"({
        "jsonrpc": "2.0",
        "method": "notifications/initialized"
    })";
}
// 生成MCP ping消息
std::string create_ping_message(int ping_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(ping_id) + R"(,
        "method": "ping"
    })";
}

// 生成MCP pong响应消息
std::string create_pong_message(int pong_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(pong_id) + R"(,
        "result": {}
    })";
}

// 生成MCP通知消息
std::string create_notification_message(const std::string& method, 
                                       const std::map<std::string, std::string>& params = {}) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "method": ")" + method + R"(")";
    
    if (!params.empty()) {
        json += R"(,
        "params": {)";
        bool first = true;
        for (const auto& param : params) {
            if (!first) json += ",";
            json += R"(")" + param.first + R"(": ")" + param.second + R"(")";
            first = false;
        }
        json += "}";
    }
    
    json += "}";
    return json;
}

// 生成MCP工具调用请求
std::string create_tool_call_message(const std::string& tool_name, 
                                   const std::map<std::string, std::string>& arguments = {},
                                   int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": ")" + tool_name + R"(")";
    
    if (!arguments.empty()) {
        json += R"(,
            "arguments": {)";
        bool first = true;
        for (const auto& arg : arguments) {
            if (!first) json += ",";
            json += R"(")" + arg.first + R"(": ")" + arg.second + R"(")";
            first = false;
        }
        json += "}";
    }
    
    json += R"(
        }
    })";
    return json;
}

// 生成list_allowed_directories工具调用请求
std::string create_list_allowed_directories_message(int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "list_allowed_directories",
            "arguments": {}
        }
    })";
}

// 辅助函数：转义JSON字符串
std::string escape_json_string(const std::string& str) {
    std::string result;
    result.reserve(str.length() + 10); // 预留一些空间
    
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b"; break;
            case '\f': result += "\\f"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:
                if (c >= 0 && c < 32) {
                    // 控制字符
                    result += "\\u";
                    result += "0000";
                    result[result.length() - 4] = '0' + (c >> 12);
                    result[result.length() - 3] = '0' + ((c >> 8) & 15);
                    result[result.length() - 2] = '0' + ((c >> 4) & 15);
                    result[result.length() - 1] = '0' + (c & 15);
                } else {
                    result += c;
                }
                break;
        }
    }
    return result;
}

// 生成write_file工具调用请求
std::string create_write_file_message(const std::string& file_path, 
                                     const std::string& content, 
                                     int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "write_file",
            "arguments": {
                "path": ")" + escape_json_string(file_path) + R"(",
                "content": ")" + escape_json_string(content) + R"("
            }
        }
    })";
    return json;
}

// 生成read_file工具调用请求
std::string create_read_file_message(const std::string& file_path, 
                                    int head_lines = -1, 
                                    int tail_lines = -1, 
                                    int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "read_file",
            "arguments": {
                "path": ")" + escape_json_string(file_path) + R"(")";
    
    if (head_lines > 0) {
        json += R"(,
                "head": )" + std::to_string(head_lines);
    }
    if (tail_lines > 0) {
        json += R"(,
                "tail": )" + std::to_string(tail_lines);
    }
    
    json += R"(
            }
        }
    })";
    return json;
}

// 生成read_text_file工具调用请求
std::string create_read_text_file_message(const std::string& file_path, 
                                         int head_lines = -1, 
                                         int tail_lines = -1, 
                                         int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "read_text_file",
            "arguments": {
                "path": ")" + escape_json_string(file_path) + R"(")";
    
    if (head_lines > 0) {
        json += R"(,
                "head": )" + std::to_string(head_lines);
    }
    if (tail_lines > 0) {
        json += R"(,
                "tail": )" + std::to_string(tail_lines);
    }
    
    json += R"(
            }
        }
    })";
    return json;
}

// 生成read_media_file工具调用请求
std::string create_read_media_file_message(const std::string& file_path, 
                                          int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "read_media_file",
            "arguments": {
                "path": ")" + escape_json_string(file_path) + R"("
            }
        }
    })";
}

// 生成read_multiple_files工具调用请求
std::string create_read_multiple_files_message(const std::vector<std::string>& file_paths, 
                                              int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "read_multiple_files",
            "arguments": {
                "paths": [)";
    
    for (size_t i = 0; i < file_paths.size(); ++i) {
        if (i > 0) json += ",";
        json += R"(")" + escape_json_string(file_paths[i]) + R"(")";
    }
    
    json += R"(
                ]
            }
        }
    })";
    return json;
}

// 生成edit_file工具调用请求
std::string create_edit_file_message(const std::string& file_path, 
                                    const std::vector<std::pair<std::string, std::string>>& edits,
                                    bool dry_run = false,
                                    int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "edit_file",
            "arguments": {
                "path": ")" + escape_json_string(file_path) + R"(",
                "edits": [)";
    
    for (size_t i = 0; i < edits.size(); ++i) {
        if (i > 0) json += ",";
        json += R"(
                    {
                        "oldText": ")" + escape_json_string(edits[i].first) + R"(",
                        "newText": ")" + escape_json_string(edits[i].second) + R"("
                    })";
    }
    
    json += R"(
                ])";
    
    if (dry_run) {
        json += R"(,
                "dryRun": true)";
    }
    
    json += R"(
            }
        }
    })";
    return json;
}

// 生成create_directory工具调用请求
std::string create_create_directory_message(const std::string& dir_path, 
                                           int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "create_directory",
            "arguments": {
                "path": ")" + escape_json_string(dir_path) + R"("
            }
        }
    })";
}

// 生成list_directory工具调用请求
std::string create_list_directory_message(const std::string& dir_path, 
                                         int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "list_directory",
            "arguments": {
                "path": ")" + escape_json_string(dir_path) + R"("
            }
        }
    })";
}

// 生成list_directory_with_sizes工具调用请求
std::string create_list_directory_with_sizes_message(const std::string& dir_path, 
                                                    const std::string& sort_by = "name",
                                                    int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "list_directory_with_sizes",
            "arguments": {
                "path": ")" + escape_json_string(dir_path) + R"(",
                "sortBy": ")" + sort_by + R"("
            }
        }
    })";
    return json;
}

// 生成directory_tree工具调用请求
std::string create_directory_tree_message(const std::string& dir_path, 
                                         int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "directory_tree",
            "arguments": {
                "path": ")" + escape_json_string(dir_path) + R"("
            }
        }
    })";
}

// 生成move_file工具调用请求
std::string create_move_file_message(const std::string& source_path, 
                                    const std::string& destination_path, 
                                    int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "move_file",
            "arguments": {
                "source": ")" + escape_json_string(source_path) + R"(",
                "destination": ")" + escape_json_string(destination_path) + R"("
            }
        }
    })";
}

// 生成search_files工具调用请求
std::string create_search_files_message(const std::string& search_path, 
                                       const std::string& pattern, 
                                       const std::vector<std::string>& exclude_patterns = {},
                                       int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "search_files",
            "arguments": {
                "path": ")" + escape_json_string(search_path) + R"(",
                "pattern": ")" + escape_json_string(pattern) + R"(")";
    
    if (!exclude_patterns.empty()) {
        json += R"(,
                "excludePatterns": [)";
        for (size_t i = 0; i < exclude_patterns.size(); ++i) {
            if (i > 0) json += ",";
            json += R"(")" + escape_json_string(exclude_patterns[i]) + R"(")";
        }
        json += "]";
    }
    
    json += R"(
            }
        }
    })";
    return json;
}

// 生成get_file_info工具调用请求
std::string create_get_file_info_message(const std::string& file_path, 
                                        int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/call",
        "params": {
            "name": "get_file_info",
            "arguments": {
                "path": ")" + escape_json_string(file_path) + R"("
            }
        }
    })";
}

// 生成MCP资源读取请求
std::string create_resource_read_message(const std::string& uri, int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "resources/read",
        "params": {
            "uri": ")" + uri + R"("
        }
    })";
}

// 生成MCP提示请求
std::string create_prompt_message(const std::string& prompt_name, 
                                const std::map<std::string, std::string>& arguments = {},
                                int request_id = 1) {
    std::string json = R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "prompts/get",
        "params": {
            "name": ")" + prompt_name + R"(")";
    
    if (!arguments.empty()) {
        json += R"(,
            "arguments": {)";
        bool first = true;
        for (const auto& arg : arguments) {
            if (!first) json += ",";
            json += R"(")" + arg.first + R"(": ")" + arg.second + R"(")";
            first = false;
        }
        json += "}";
    }
    
    json += R"(
        }
    })";
    return json;
}

// 生成MCP工具列表请求
std::string create_list_tools_message(int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "tools/list"
    })";
}

// 生成MCP资源列表请求
std::string create_list_resources_message(int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "resources/list"
    })";
}

// 生成MCP提示列表请求
std::string create_list_prompts_message(int request_id = 1) {
    return R"({
        "jsonrpc": "2.0",
        "id": )" + std::to_string(request_id) + R"(,
        "method": "prompts/list"
    })";
}

} // namespace mcp_protocol
