/**
 * @file netio_https_demo.cpp
 * @brief 使用NetIO发送HTTPS请求的demo
 * 
 * 这个文件演示了如何使用NetIO库发送HTTPS请求到MCP服务器。
 * 支持HTTPS协议的安全连接。
 */

#include "netio.h"
#include "../../common/json.hpp"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>

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

// 创建MCP工具列表请求
Message createMcpToolsListRequest() {
    // 创建JSON请求数据
    json request_data = {
        {"jsonrpc", "2.0"},
        {"method", "tools/list"},
        {"id", 1},
        {"params", {
            {"_meta", {
                {"progressToken", 1}
            }}
        }}
    };
    
    std::string json_string = request_data.dump();
    
    // 创建消息
    Message message;
    message.id = "mcp_tools_list_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    message.type = MessageType::REQUEST;
    message.method = "POST";
    message.path = "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ";
    message.body = json_string;
    message.headers = {
        {"Content-Type", "application/json; charset=utf-8"},
        {"Accept", "application/json"},
        {"User-Agent", "NetIO-MCP-Demo/1.0"}
    };
    message.timestamp = std::chrono::system_clock::now();
    
    return message;
}

int main() {
    std::cout << "🚀 NetIO MCP请求演示" << std::endl;
    std::cout << "===================" << std::endl;
    
    try {
        // 创建NetIO实例
        NetIO netio_client;
        
        // 配置网络连接
        NetConfig config;
        config.host = "mcp.didichuxing.com";
        config.port = 443;  // HTTPS端口
        config.protocol = Protocol::HTTPS;
        config.timeout_seconds = 30;
        config.max_retries = 3;
        config.keep_alive = true;
        config.auto_reconnect = false;
        config.user_agent = "NetIO-MCP-Demo/1.0";
        
        // 设置默认请求头
        config.headers = {
            {"Content-Type", "application/json; charset=utf-8"},
            {"Accept", "application/json"}
        };
        
        std::cout << "📤 连接配置:" << std::endl;
        std::cout << "主机: " << config.host << ":" << config.port << std::endl;
        std::cout << "协议: " << (config.protocol == Protocol::HTTPS ? "HTTPS" : "HTTP") << std::endl;
        std::cout << "超时: " << config.timeout_seconds << "秒" << std::endl;
        std::cout << std::endl;
        
        // 连接到服务器
        std::cout << "⏳ 连接到服务器..." << std::endl;
        if (!netio_client.connect(config)) {
            throw std::runtime_error("无法连接到服务器");
        }
        
        // 检查连接状态
        auto conn_info = netio_client.getConnectionInfo();
        std::cout << "✅ 连接成功" << std::endl;
        std::cout << "连接ID: " << conn_info.connection_id << std::endl;
        std::cout << "远程地址: " << conn_info.remote_address << ":" << conn_info.remote_port << std::endl;
        std::cout << "本地地址: " << conn_info.local_address << ":" << conn_info.local_port << std::endl;
        std::cout << std::endl;
        
        // 创建MCP请求
        Message request = createMcpToolsListRequest();
        
        std::cout << "📤 发送MCP请求:" << std::endl;
        std::cout << "请求ID: " << request.id << std::endl;
        std::cout << "方法: " << request.method << std::endl;
        std::cout << "路径: " << request.path << std::endl;
        std::cout << "请求头:" << std::endl;
        for (const auto& [key, value] : request.headers) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
        std::cout << "请求体:" << std::endl;
        std::cout << request.body << std::endl;
        std::cout << std::endl;
        
        // 发送请求
        std::cout << "⏳ 发送请求中..." << std::endl;
        Message response = netio_client.sendRequestSync(request);
        
        // 解析响应
        parseMcpResponse(response);
        
        // 显示统计信息
        std::cout << "\n📊 统计信息:" << std::endl;
        auto stats = netio_client.getStatistics();
        std::cout << stats.dump(2) << std::endl;
        
        // 断开连接
        std::cout << "\n⏳ 断开连接..." << std::endl;
        netio_client.disconnect();
        std::cout << "✅ 连接已断开" << std::endl;
        
        std::cout << "\n✅ 请求完成" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ 错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
