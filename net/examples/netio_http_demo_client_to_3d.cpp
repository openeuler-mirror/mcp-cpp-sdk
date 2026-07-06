/**
 * @file netio_http_demo.cpp
 * @brief 使用NetIO发送HTTP请求的demo
 * 
 * 这个文件演示了如何使用NetIO库发送HTTP请求到本地MCP服务器。
 * 请求地址: http://localhost:10086/mcp
 */

#include "netio.h"
#include "../../common/json.hpp"
#include <iostream>
#include <string>
#include <chrono>

using namespace netio;
using json = nlohmann::json;

// 解析MCP响应的函数
void parseMcpResponse(const Message& response) {
    std::cout << "\n📥 收到响应:" << std::endl;
    std::cout << "状态码: " << response.status_code << std::endl;
    std::cout << "响应头:" << std::endl;
    for (const auto& [key, value] : response.headers) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    std::cout << "响应体:" << std::endl;
    
    // 尝试解析JSON响应
    try {
        auto json_response = json::parse(response.body);
        std::cout << json_response.dump(2) << std::endl;
        
        // 解析MCP响应
        if (json_response.contains("result")) {
            std::cout << "\n✅ 请求成功" << std::endl;
            auto mcp_result = json_response["result"];
            
            if (mcp_result.contains("tools") && mcp_result["tools"].is_array()) {
                std::cout << "🔧 可用工具 (" << mcp_result["tools"].size() << " 个):" << std::endl;
                for (size_t i = 0; i < mcp_result["tools"].size(); ++i) {
                    const auto& tool = mcp_result["tools"][i];
                    std::cout << "  " << (i + 1) << ". " << tool.value("name", "未知") << std::endl;
                    if (tool.contains("description")) {
                        std::cout << "     描述: " << tool["description"].get<std::string>() << std::endl;
                    }
                }
            } else if (mcp_result.contains("resources") && mcp_result["resources"].is_array()) {
                std::cout << "📚 可用资源 (" << mcp_result["resources"].size() << " 个):" << std::endl;
                for (size_t i = 0; i < mcp_result["resources"].size(); ++i) {
                    const auto& resource = mcp_result["resources"][i];
                    std::cout << "  " << (i + 1) << ". " << resource.value("uri", "未知") << std::endl;
                    if (resource.contains("name")) {
                        std::cout << "     名称: " << resource["name"].get<std::string>() << std::endl;
                    }
                }
            } else {
                std::cout << "📋 结果内容: " << mcp_result.dump(2) << std::endl;
            }
        } else if (json_response.contains("error")) {
            std::cout << "\n❌ 请求失败" << std::endl;
            auto error = json_response["error"];
            std::cout << "错误代码: " << error.value("code", -1) << std::endl;
            std::cout << "错误消息: " << error.value("message", "未知错误") << std::endl;
            if (error.contains("data")) {
                std::cout << "错误数据: " << error["data"].dump(2) << std::endl;
            }
        } else {
            std::cout << "\n⚠️  未知响应格式" << std::endl;
            std::cout << "响应内容: " << json_response.dump(2) << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cout << "⚠️  响应不是有效的JSON: " << e.what() << std::endl;
        std::cout << "原始响应: " << response.body << std::endl;
    }
}

// 创建MCP ping请求
json createMcpPingRequest() {
    return {
        {"jsonrpc", "2.0"},
        {"method", "ping"},
        {"id", 1},
        {"params", {}}
    };
}

// 创建MCP initialize请求
json createMcpInitializeRequest() {
    return {
        {"jsonrpc", "2.0"},
        {"method", "initialize"},
        {"id", 2},
        {"params", {
            {"protocolVersion", "2024-11-05"},
            {"capabilities", {
                {"roots", {
                    {"listChanged", true}
                }},
                {"sampling", {}}
            }},
            {"clientInfo", {
                {"name", "localhost-mcp-demo"},
                {"version", "1.0.0"}
            }}
        }}
    };
}

// 创建MCP工具列表请求
json createMcpToolsListRequest() {
    return {
        {"jsonrpc", "2.0"},
        {"method", "tools/list"},
        {"id", 3},
        {"params", {
            {"_meta", {
                {"progressToken", 1}
            }}
        }}
    };
}

// 创建MCP资源列表请求
json createMcpResourcesListRequest() {
    return {
        {"jsonrpc", "2.0"},
        {"method", "resources/list"},
        {"id", 4},
        {"params", {}}
    };
}

// 创建MCP提示列表请求
json createMcpPromptsListRequest() {
    return {
        {"jsonrpc", "2.0"},
        {"method", "prompts/list"},
        {"id", 5},
        {"params", {}}
    };
}

// 发送MCP请求的通用函数
bool sendMcpRequest(NetIO& netio, const std::map<std::string, std::string>& headers, const json& request, const std::string& test_name) {
    std::cout << "\n" << test_name << std::endl;
    std::cout << std::string(test_name.length(), '=') << std::endl;
    
    std::string request_json = request.dump();
    
    std::cout << "📤 发送请求:" << std::endl;
    std::cout << "URL: http://localhost:10086/mcp/" << std::endl;
    std::cout << "Method: POST" << std::endl;
    std::cout << "Headers:" << std::endl;
    for (const auto& [key, value] : headers) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    std::cout << "Body:" << std::endl;
    std::cout << request_json << std::endl;
    std::cout << std::endl;
    
    std::cout << "⏳ 发送请求中..." << std::endl;
    auto result = netio.sendRequestSync("POST", "/mcp/", request_json, headers);
    
    if (result.status_code >= 200 && result.status_code < 300) {
        parseMcpResponse(result);
        return true;
    } else {
        std::cout << "❌ 请求失败: " << result.error_message << std::endl;
        return false;
    }
}

// 服务器 
// cd /Users/apple/demo_mcp
// source /demo_env/bin/activate
// markitdown-mcp --http --host 0.0.0.0 --port 10086
//
int main() {
    std::cout << "🚀 本地MCP服务器请求演示" << std::endl;
    std::cout << "=========================" << std::endl;
    
    try {
        // 创建NetIO客户端
        NetIO netio;
        
        // 配置网络设置
        NetConfig config;
        config.host = "localhost";
        config.port = 10086;
        config.protocol = Protocol::HTTP;
        config.timeout_seconds = 30;
        config.user_agent = "Localhost-MCP-Demo/1.0";
        config.headers = {
            {"Content-Type", "application/json; charset=utf-8"},
            {"Accept", "application/json, text/event-stream"}
        };
        
        // 连接到服务器
        if (!netio.connect(config)) {
            std::cout << "❌ 无法连接到服务器 localhost:10086" << std::endl;
            return 1;
        }
        
        // 设置请求头
        std::map<std::string, std::string> headers = {
            {"Content-Type", "application/json; charset=utf-8"},
            {"Accept", "application/json, text/event-stream"},
            {"User-Agent", "Localhost-MCP-Demo/1.0"}
        };
        
        std::cout << "📤 连接配置:" << std::endl;
        std::cout << "主机: localhost:10086" << std::endl;
        std::cout << "协议: HTTP" << std::endl;
        std::cout << "超时: 30秒" << std::endl;
        std::cout << std::endl;
        
        // 测试1: Ping请求
        auto ping_request = createMcpPingRequest();
        sendMcpRequest(netio, headers, ping_request, "🏓 测试1: Ping请求");
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        
        // 测试2: Initialize请求
        auto init_request = createMcpInitializeRequest();
        sendMcpRequest(netio, headers, init_request, "🚀 测试2: Initialize请求");
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        
        // 测试3: 工具列表请求
        auto tools_request = createMcpToolsListRequest();
        sendMcpRequest(netio, headers, tools_request, "🔧 测试3: 工具列表请求");
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        
        // 测试4: 资源列表请求
        auto resources_request = createMcpResourcesListRequest();
        sendMcpRequest(netio, headers, resources_request, "📚 测试4: 资源列表请求");
        
        std::cout << "\n" << std::string(50, '=') << std::endl;
        
        // 测试5: 提示列表请求
        auto prompts_request = createMcpPromptsListRequest();
        sendMcpRequest(netio, headers, prompts_request, "💡 测试5: 提示列表请求");
        
        std::cout << "\n✅ 所有测试完成" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
