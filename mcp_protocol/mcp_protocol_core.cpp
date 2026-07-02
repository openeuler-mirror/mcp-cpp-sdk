/**
 * @file mcp_protocol_core.cpp
 * @brief MCP Protocol Core Implementation
 * 
 * This file provides the implementation of core protocol utilities
 * and data structure serialization/deserialization.
 */

#include "mcp_protocol_core.h"
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

// Utility functions implementation
namespace utils {

ClientCapabilities create_default_client_capabilities() {
    ClientCapabilities caps;
    caps.logging = json::object();
    caps.prompts = {
        {"listChanged", true}
    };
    caps.resources = {
        {"subscribe", true}
    };
    caps.tools = {
        {"listChanged", true}
    };
    return caps;
}

ServerCapabilities create_default_server_capabilities() {
    ServerCapabilities caps;
    caps.prompts = {
        {"listChanged", false}
    };
    caps.resources = {
        {"listChanged", false},
        {"subscribe", false}
    };
    caps.tools = {
        {"listChanged", false}
    };
    return caps;
}

bool is_valid_protocol_version(const std::string& version) {
    return version == "2024-11-05" || version == "2025-03-26" || version == "2025-06-18";
}

Tool create_tool_from_schema(const std::string& name, const std::string& description, const json& schema) {
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.inputSchema = schema;
    return tool;
}

Resource create_resource_from_uri(const std::string& uri, const std::string& name, const std::string& description) {
    Resource resource;
    resource.uri = uri;
    resource.name = name.empty() ? uri : name;
    resource.description = description;
    resource.mimeType = "text/plain";
    return resource;
}

} // namespace utils

} // namespace mcp
