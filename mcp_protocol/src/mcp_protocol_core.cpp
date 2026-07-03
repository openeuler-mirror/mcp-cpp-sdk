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

// 日志记录器将在需要时局部声明

namespace mcp {

// Utility functions implementation
namespace utils {

ClientCapabilities createDefaultClientCapabilities() {
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
    caps.sampling = {
        {"maxTokens", 4096},
        {"temperature", 0.7}
    };
    return caps;
}

ServerCapabilities createDefaultServerCapabilities() {
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
    caps.sampling = {
        {"maxTokens", 2048},
        {"temperature", 0.5}
    };
    return caps;
}

bool isValidProtocolVersion(const std::string& version) {
    return version == "2024-11-05" || version == "2025-03-26" || version == "2025-06-18" || version == "2025-11-25";
}

Tool createToolFromSchema(const std::string& name, const std::string& description, const json& schema) {
    Tool tool;
    tool.name = name;
    tool.description = description;
    tool.inputSchema = schema;
    return tool;
}

Resource createResourceFromUri(const std::string& uri, const std::string& name, const std::string& description) {
    Resource resource;
    resource.uri = uri;
    resource.name = name.empty() ? uri : name;
    resource.description = description;
    resource.mimeType = "text/plain";
    return resource;
}

} // namespace utils

} // namespace mcp
