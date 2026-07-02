/**
 * @file mcp_client_http_demo.cpp
 * @brief MCP Client HTTP/SSE 通信测试示例
 * 
 * 这个文件专门演示如何使用 mcp_client 进行 HTTP/SSE 通信，
 * 包括 HTTPS 连接、API 调用和错误处理测试。
 */

#include "mcp_client.h"
#include "../log/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>

using namespace mcp;
using namespace mcp_logger;

// 初始化日志系统
void initialize_logger() {
    Logger& logger = Logger::getInstance();
    LogConfig config;
    config.level = mcp_logger::LogLevel::DEBUG_LEVEL;
    config.target = mcp_logger::LogTarget::BOTH;
    config.file_path = "mcp_client_http_demo.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    logger.initialize(config);
}

int main() {
    // 初始化日志系统
    initialize_logger();
    MCP_LOG_INFO("🌐 MCP Client HTTP/SSE 通信测试");
    MCP_LOG_INFO("=============================");
    
    try {
        // 创建 MCP 客户端实例
        auto client = std::make_unique<MCPClient>();
        
        // 创建 HTTP/SSE 传输配置
        mcp_transport::TransportConfig config;
        config.type = mcp_transport::TransportType::SSE;
        config.enable_logging = true;
        config.log_level = 1;
        config.log_file_path = "mcp_client_http_test.log";
        
        // HTTP/SSE 配置 - 连接到远程 MCP 服务器
        config.sse.host = "mcp.didichuxing.com";
        config.sse.port = 443;
        config.sse.protocol = netio::Protocol::HTTPS;
        config.sse.timeout_seconds = 30;
        config.sse.max_retries = 3;
        config.sse.user_agent = "MCP-Client-HTTP/1.0";
        config.sse.keep_alive = true;
        config.sse.auto_reconnect = true;
        config.sse.reconnect_interval = 5;
        config.sse.headers["Content-Type"] = "application/json; charset=utf-8";
        config.sse.headers["Accept"] = "application/json, text/event-stream";
        config.sse.headers["User-Agent"] = "MCP-Client-HTTP/1.0";
        
        // 测试端点列表
        std::vector<std::string> test_endpoints = {
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ"
        };
        
        // 设置测试端点
        client->set_endpoints(test_endpoints);
        MCP_LOG_INFO("📋 已设置 " + std::to_string(test_endpoints.size()) + " 个测试端点");
        for (size_t i = 0; i < test_endpoints.size(); ++i) {
            MCP_LOG_INFO("  端点 " + std::to_string(i + 1) + ": " + test_endpoints[i]);
        }
        MCP_LOG_INFO("  当前端点: " + client->get_current_endpoint());
        
        // 启动连接
        MCP_LOG_INFO("🚀 启动 MCP 客户端 HTTP/SSE 连接...");
        MCP_LOG_INFO("📋 配置信息:");
        MCP_LOG_INFO("  传输类型: HTTP/SSE");
        MCP_LOG_INFO("  主机: " + config.sse.host);
        MCP_LOG_INFO("  端口: " + std::to_string(config.sse.port));
        MCP_LOG_INFO("  协议: " + std::string(config.sse.protocol == netio::Protocol::HTTPS ? "HTTPS" : "HTTP"));
        MCP_LOG_INFO("  超时: " + std::to_string(config.sse.timeout_seconds) + " 秒");
        MCP_LOG_INFO("  最大重试: " + std::to_string(config.sse.max_retries));
        MCP_LOG_INFO("  用户代理: " + config.sse.user_agent);
        
        if (!client->connect_with_config(config)) {
            MCP_LOG_ERROR("❌ 启动 MCP 客户端连接失败");
            MCP_LOG_INFO("💡 提示: 请检查网络连接和服务器地址");
            MCP_LOG_INFO("💡 提示: 确保服务器 " + config.sse.host + " 可访问");
            return 1;
        }
        
        MCP_LOG_INFO("✅ MCP 客户端连接成功");
        
        // 等待连接建立
        MCP_LOG_INFO("⏳ 等待连接建立与服务初始化...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // 检查连接状态
        MCP_LOG_INFO("🔍 连接状态检查:");
        MCP_LOG_INFO("  连接状态: " + std::string(client->is_connected() ? "已连接" : "未连接"));
        MCP_LOG_INFO("  传输类型: " + std::to_string(static_cast<int>(client->get_transport_type())));
        MCP_LOG_INFO("  传输状态: " + std::to_string(static_cast<int>(client->get_transport_status())));
        
        if (!client->is_connected()) {
            MCP_LOG_ERROR("❌ 错误: 客户端未连接");
            MCP_LOG_INFO("💡 提示: 请检查网络连接和服务器状态");
            return 1;
        }
        
        // 设置客户端信息
        client->set_client_info("mcp-client-http-demo", "1.0.0");
        
        // 创建初始化参数
        InitializeParams init_params;
        init_params.protocolVersion = "2024-11-05";
        init_params.clientInfo.name = "mcp-client-http-demo";
        init_params.clientInfo.version = "1.0.0";
        init_params.capabilities = utils::create_default_client_capabilities();
        
        // 测试用例列表
        std::vector<std::function<void()>> test_cases = {
            [&]() {
                MCP_LOG_INFO("\n📤 测试 1: 初始化 MCP 协议");
                try {
                    auto result = client->initialize(init_params);
                    MCP_LOG_INFO("  ✅ 初始化成功");
                    MCP_LOG_INFO("  服务器: " + result.serverInfo.name + " v" + result.serverInfo.version);
                    MCP_LOG_INFO("  协议版本: " + result.protocolVersion);
                    MCP_LOG_INFO("  服务器能力: " + result.capabilities.to_json().dump(2));
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 初始化失败: " + std::string(e.what()));
                    MCP_LOG_INFO("  💡 建议: 检查服务器是否支持 MCP 协议");
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 2: 发送初始化完成通知");
                try {
                    client->initialized();
                    MCP_LOG_INFO("  ✅ 初始化完成通知发送成功");
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 初始化完成通知发送失败: " + std::string(e.what()));
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 3: 获取可用工具列表");
                try {
                    auto tools = client->list_tools();
                    MCP_LOG_INFO("  ✅ 获取工具列表成功");
                    MCP_LOG_INFO("  工具数量: " + std::to_string(tools.size()));
                    for (const auto& tool : tools) {
                        MCP_LOG_INFO("    - " + tool.name + ": " + tool.description);
                        MCP_LOG_INFO("      输入模式: " + tool.inputSchema.dump(2));
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 获取工具列表失败: " + std::string(e.what()));
                    MCP_LOG_INFO("  💡 建议: 检查服务器是否提供工具服务");
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 4: 获取可用资源列表");
                try {
                    auto resources = client->list_resources();
                    MCP_LOG_INFO("  ✅ 获取资源列表成功");
                    MCP_LOG_INFO("  资源数量: " + std::to_string(resources.size()));
                    for (const auto& resource : resources) {
                        MCP_LOG_INFO("    - " + resource.name + " (" + resource.uri + ")");
                        MCP_LOG_INFO("      描述: " + resource.description);
                        MCP_LOG_INFO("      MIME 类型: " + resource.mimeType);
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 获取资源列表失败: " + std::string(e.what()));
                    MCP_LOG_INFO("  💡 建议: 检查服务器是否提供资源服务");
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 5: 获取提示列表");
                try {
                    auto prompts = client->list_prompts();
                    MCP_LOG_INFO("  ✅ 获取提示列表成功");
                    MCP_LOG_INFO("  提示数量: " + std::to_string(prompts.size()));
                    for (const auto& prompt : prompts) {
                        MCP_LOG_INFO("    - " + prompt.name + ": " + prompt.description);
                        MCP_LOG_INFO("      参数: " + std::to_string(prompt.arguments.size()) + " 个");
                        for (const auto& arg : prompt.arguments) {
                            MCP_LOG_INFO("        - " + arg);
                        }
                    }
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ 获取提示列表失败: " + std::string(e.what()));
                    MCP_LOG_INFO("  💡 建议: 检查服务器是否提供提示服务");
                }
            },
            
            [&]() {
                MCP_LOG_INFO("\n📤 测试 6: 发送 ping 测试");
                try {
                    auto result = client->ping();
                    MCP_LOG_INFO("  ✅ Ping 成功");
                    MCP_LOG_INFO("  响应: " + result.dump());
                } catch (const std::exception& e) {
                    MCP_LOG_ERROR("  ❌ Ping 失败: " + std::string(e.what()));
                    MCP_LOG_INFO("  💡 建议: 检查网络连接和服务器响应");
                }
            },
            
        // 新增：测试 tool_call 接口
        [&]() {
            MCP_LOG_INFO("\n📤 测试 7: tool_call 接口调用 (maps_textsearch)");
            try {
                // 构造 maps_textsearch 工具调用参数
                nlohmann::json params = {
                    {"keywords", "美食"},
                    {"city", "上海"},
                    {"location", "121.4737,31.2304"}
                };
                auto result = client->call_tool("maps_textsearch", params);
                MCP_LOG_INFO("  ✅ maps_textsearch 工具调用成功");
                MCP_LOG_INFO("  响应: " + result.to_json().dump(2));
            } catch (const std::exception& e) {
                MCP_LOG_ERROR("  ❌ maps_textsearch 工具调用失败: " + std::string(e.what()));
                MCP_LOG_INFO("  💡 建议: 检查服务器是否支持 tool_call 方法及 maps_textsearch 工具");
            }

            MCP_LOG_INFO("\n📤 测试 8: tool_call 接口调用 (taxi_estimate)");
            try {
                // 构造 taxi_estimate 工具调用参数
                nlohmann::json params = {
                    {"from_lat", "31.2304"},
                    {"from_lng", "121.4737"},
                    {"from_name", "上海市人民广场"},
                    {"to_lat", "31.2200"},
                    {"to_lng", "121.4450"},
                    {"to_name", "上海火车站"}
                };
                auto result = client->call_tool("taxi_estimate", params);
                MCP_LOG_INFO("  ✅ taxi_estimate 工具调用成功");
                MCP_LOG_INFO("  响应: " + result.to_json().dump(2));
            } catch (const std::exception& e) {
                MCP_LOG_ERROR("  ❌ taxi_estimate 工具调用失败: " + std::string(e.what()));
                MCP_LOG_INFO("  💡 建议: 检查服务器是否支持 tool_call 方法及 taxi_estimate 工具");
            }

            MCP_LOG_INFO("\n📤 测试 9: tool_call 接口调用 (taxi_generate_ride_app_link)");
            try {
                // 构造 taxi_generate_ride_app_link 工具调用参数
                nlohmann::json params = {
                    {"from_lat", "31.2304"},
                    {"from_lng", "121.4737"},
                    {"to_lat", "31.2200"},
                    {"to_lng", "121.4450"},
                    {"product_category", "快车,专车"}
                };
                auto result = client->call_tool("taxi_generate_ride_app_link", params);
                MCP_LOG_INFO("  ✅ taxi_generate_ride_app_link 工具调用成功");
                MCP_LOG_INFO("  响应: " + result.to_json().dump(2));
            } catch (const std::exception& e) {
                MCP_LOG_ERROR("  ❌ taxi_generate_ride_app_link 工具调用失败: " + std::string(e.what()));
                MCP_LOG_INFO("  💡 建议: 检查服务器是否支持 tool_call 方法及 taxi_generate_ride_app_link 工具");
            }
        },
            
          
        };
        
        // 执行测试用例
        for (size_t i = 0; i < test_cases.size(); ++i) {
            MCP_LOG_INFO("\n🔄 当前使用端点: " + client->get_current_endpoint());
            test_cases[i]();
            
            // 轮询到下一个endpoint（除了最后一个测试用例）
            if (i < test_cases.size() - 1) {
                // 获取当前endpoint列表
                auto endpoints = client->get_endpoints();
                if (endpoints.size() > 1) {
                    // 找到当前endpoint的索引
                    auto current_endpoint = client->get_current_endpoint();
                    auto it = std::find(endpoints.begin(), endpoints.end(), current_endpoint);
                    if (it != endpoints.end()) {
                        size_t current_index = std::distance(endpoints.begin(), it);
                        size_t next_index = (current_index + 1) % endpoints.size();
                        client->set_current_endpoint(endpoints[next_index]);
                        MCP_LOG_INFO("🔄 切换到下一个端点: " + endpoints[next_index]);
                    }
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        }
        
        // 显示服务器信息
        MCP_LOG_INFO("\n📊 服务器信息:");
        try {
            auto server_info = client->get_server_info();
            auto server_caps = client->get_server_capabilities();
            MCP_LOG_INFO("  服务器名称: " + server_info.name);
            MCP_LOG_INFO("  服务器版本: " + server_info.version);
            MCP_LOG_INFO("  服务器能力: " + server_caps.to_json().dump(2));
        } catch (const std::exception& e) {
            MCP_LOG_ERROR("  获取服务器信息失败: " + std::string(e.what()));
        }
        
        // 测试连接稳定性
        MCP_LOG_INFO("\n🔧 测试连接稳定性:");
        for (int i = 0; i < 3; ++i) {
            MCP_LOG_INFO("  稳定性测试 " + std::to_string(i + 1) + "/3");
            try {
                auto result = client->ping();
                MCP_LOG_INFO("    ✅ Ping " + std::to_string(i + 1) + " 成功");
            } catch (const std::exception& e) {
                MCP_LOG_ERROR("    ❌ Ping " + std::to_string(i + 1) + " 失败: " + std::string(e.what()));
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // 断开连接
        MCP_LOG_INFO("\n🛑 断开 MCP 客户端连接...");
        client->disconnect();
        MCP_LOG_INFO("✅ MCP 客户端连接已断开");
        
    } catch (const std::exception& e) {
        MCP_LOG_ERROR("❌ 错误: " + std::string(e.what()));
        return 1;
    }
    
    MCP_LOG_INFO("\n✅ MCP Client HTTP/SSE 通信测试完成");
    return 0;
}