/**
 * @file mcp_message.h
 * @brief Core definitions for the Model Context Protocol (MCP) framework
 * 
 * This file contains the core structures and definitions for the MCP protocol.
 * Implements the 2024-11-05 and 2025-11-25 protocol specifications.
 * Supports backward compatibility between versions.
 */

#ifndef MCP_MESSAGE_H
#define MCP_MESSAGE_H

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <stdexcept>

// Include the JSON library for parsing and generating JSON
#include "../../common/json.hpp"
#include "../../log/include/logger.h"

namespace mcp {

// Use the nlohmann json library
using json = nlohmann::ordered_json;

// MCP version constants
constexpr const char* MCP_VERSION_2024_11_05 = "2024-11-05";
constexpr const char* MCP_VERSION_2025_11_25 = "2025-11-25";
constexpr const char* MCP_VERSION = MCP_VERSION_2025_11_25; // Default version for backward compatibility

// MCP error codes (JSON-RPC 2.0 standard codes)
enum class error_code {
    parse_error = -32700,           // Invalid JSON
    invalid_request = -32600,       // Invalid Request object
    method_not_found = -32601,      // Method not found
    invalid_params = -32602,        // Invalid method parameters
    internal_error = -32603,        // Internal JSON-RPC error
    server_error_start = -32000,    // Server error start
    server_error_end = -32099       // Server error end
};

// MCP exception class
class mcp_exception : public std::runtime_error {
public:
    mcp_exception(error_code code, const std::string& message)
        : std::runtime_error(message), code_(code) {}

    error_code code() const { return code_; }

private:
    error_code code_;
};

// JSON-RPC 2.0 Request
struct request {
    std::string jsonrpc = "2.0";
    json id;
    std::string method;
    json params;
    
    // Create a request
    static request create(const std::string& method, const json& params = json::object()) {
        request req;
        req.jsonrpc = "2.0";
        req.id = generateId();
        req.method = method;
        req.params = params;
        return req;
    }
    
    // Create a request with a specific ID
    static request createWithId(const std::string& id, const std::string& method, const json& params = json::object()) {
        request req;
        req.jsonrpc = "2.0";
        try {
            auto logger = mcp_logger::getModuleLogger("mcp_protocol");
            logger->debug("Creating request with id: " + id);
        } catch (...) {
            // 如果logger尚未初始化，静默忽略
        }
        req.id = json::string_t(id);
        try {
            auto logger = mcp_logger::getModuleLogger("mcp_protocol");
            logger->debug("Request id set to: " + req.id.dump());
        } catch (...) {
            // 如果logger尚未初始化，静默忽略
        }
        req.method = method;
        req.params = params;
        return req;
    }
    
    // Create a notification (no response expected)
    static request createNotification(const std::string& method, const json& params = json::object()) {
        request req;
        req.jsonrpc = "2.0";
        req.id = nullptr;
        //req.method = "notifications/" + method;
        req.method = method;
        req.params = params;
        return req;
    }
    
    // Check if this is a notification
    bool isNotification() const {
        return id.is_null();
    }
    
    // Convert to JSON
    json toJson() const {
        json j = {
            {"jsonrpc", jsonrpc},
            {"method", method}
        };
        
        if (!params.empty()) {
            j["params"] = params;
        }
        
        if (!isNotification()) {
            j["id"] = id;
        }
        
        return j;
    }

    static request fromJson(const json& j) {
        request req;
        req.jsonrpc = j["jsonrpc"].get<std::string>();
        req.id = j["id"];
        req.method = j["method"].get<std::string>();
        
        if (j.contains("params")) {
            req.params = j["params"];
        }
        
        return req;
    }
    
private:
    // Generate a unique ID
    static json generateId() {
        static int next_id = 1;
        return next_id++;
    }
};

// JSON-RPC 2.0 Response
struct response {
    std::string jsonrpc = "2.0";
    json id;
    json result;
    json error;
    
    // Create a success response
    static response createSuccess(const json& req_id, const json& result_data = json::object()) {
        response res;
        res.jsonrpc = "2.0";
        res.id = req_id;
        res.result = result_data;
        return res;
    }
    
    // Create an error response (backward compatible with 2024-11-05)
    static response createError(const json& req_id, error_code code, const std::string& message, const json& data = json::object()) {
        response res;
        res.jsonrpc = "2.0";
        res.id = req_id; // Always include id for backward compatibility
        res.error = {
            {"code", static_cast<int>(code)},
            {"message", message}
        };
        
        if (!data.empty()) {
            res.error["data"] = data;
        }
        
        return res;
    }
    
    // Create an error response for 2025-11-25 (id is optional)
    static response createError2025(const json& req_id, error_code code, const std::string& message, const json& data = json::object()) {
        response res;
        res.jsonrpc = "2.0";
        res.id = req_id; // Can be null for notifications
        res.error = {
            {"code", static_cast<int>(code)},
            {"message", message}
        };
        
        if (!data.empty()) {
            res.error["data"] = data;
        }
        
        return res;
    }
    
    // Create an error response without id (for notifications in 2025-11-25)
    static response createErrorWithoutId(error_code code, const std::string& message, const json& data = json::object()) {
        response res;
        res.jsonrpc = "2.0";
        res.id = nullptr; // id is optional in error responses per 2025-11-25 spec
        res.error = {
            {"code", static_cast<int>(code)},
            {"message", message}
        };
        
        if (!data.empty()) {
            res.error["data"] = data;
        }
        
        return res;
    }
    
    // Check if this is an error response
    bool isError() const {
        return !error.empty();
    }
    
    // Convert to JSON (supports both 2024-11-05 and 2025-11-25)
    json toJson(const std::string& protocol_version = MCP_VERSION_2024_11_05) const {
        json j = {
            {"jsonrpc", jsonrpc}
        };
        
        if (isError()) {
            j["error"] = error;
            // In 2025-11-25, id is optional in error responses
            // But we always include it for backward compatibility unless it's null
            if (protocol_version == MCP_VERSION_2025_11_25 && id.is_null()) {
                // Don't include id for 2025-11-25 if it's null (for notifications)
            } else {
                j["id"] = id;
            }
        } else {
            j["id"] = id;
            j["result"] = result;
        }
        
        return j;
    }

    static response fromJson(const json& j) {
        response res;
        res.jsonrpc = j["jsonrpc"].get<std::string>();
        
        if (j.contains("result")) {
            res.id = j["id"];
            res.result = j["result"];
        } else if (j.contains("error")) {
            // In 2025-11-25, id is optional in error responses
            if (j.contains("id")) {
                res.id = j["id"];
            } else {
                res.id = nullptr; // id is optional in error responses
            }
            res.error = j["error"];
        }
        
        return res;
    }
};

} // namespace mcp

#endif // MCP_MESSAGE_H