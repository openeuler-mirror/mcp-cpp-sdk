/**
 * @file simple_agent_example.cpp
 * @brief 简单的 MCP Agent 使用示例
 * 
 * 这个文件展示如何快速使用 MCP Agent 进行基本的对话和工具调用。
 */
#include "mcp_agent.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace mcp;
using namespace mcp_logger;

int main() {
    std::cout << "🤖 MCP Agent 简单示例" << std::endl;
    std::cout << "=====================" << std::endl;
    
    try {
        // 1. 初始化日志系统
        std::cout << "📝 初始化日志系统..." << std::endl;
        LogConfig log_config;
        log_config.level = mcp_logger::LogLevel::INFO;
        log_config.target = mcp_logger::LogTarget::CONSOLE;
        log_config.enable_timestamp = true;
        log_config.format = "[{timestamp}] [{level}] - {message}";
        initializeLogger(log_config);
        
        // 2. 创建 Agent 实例
        std::cout << "🤖 创建 Agent 实例..." << std::endl;
        auto agent = std::make_unique<MCPAgent>();
        
        // 3. 配置 Agent
        std::cout << "⚙️ 配置 Agent..." << std::endl;
        AgentConfig config;
        
        // LLM 配置
        config.llm_config.api_endpoint = "https://api.openai.com/v1/chat/completions";
        config.llm_config.api_key = "your-api-key-here"; // 请替换为实际的API密钥
        config.llm_config.model_name = "gpt-3.5-turbo";
        config.llm_config.system_prompt = 
            "你是一个智能助手，可以调用各种工具来帮助用户。"
            "请简洁明了地回答用户的问题。";
        config.llm_config.max_tokens = 1024;
        config.llm_config.temperature = 0.7;
        config.llm_config.timeout_seconds = 30;
        
        // MCP 传输配置
        config.transport_config.type = mcp_transport::TransportType::HTTP;
        config.transport_config.enable_logging = false;
        config.transport_config.sse.host = "localhost";
        config.transport_config.sse.port = 10086;
        config.transport_config.sse.protocol = netio::Protocol::HTTP;
        config.transport_config.sse.timeout_seconds = 30;
        config.transport_config.sse.max_retries = 3;
        config.transport_config.sse.user_agent = "MCP-Agent-Simple/1.0";
        
        // Agent 基本信息
        config.agent_name = "SimpleAgent";
        config.agent_version = "1.0.0";
        config.enable_tool_calling = true;
        config.enable_auto_retry = true;
        config.max_conversation_turns = 5;
        config.tool_call_timeout = 30.0;
        
        // 4. 初始化 Agent
        std::cout << "🚀 初始化 Agent..." << std::endl;
        if (!agent->initialize(config)) {
            std::cerr << "❌ Agent 初始化失败" << std::endl;
            std::cerr << "💡 请检查:" << std::endl;
            std::cerr << "  1. MCP 服务器是否运行在 localhost:10086" << std::endl;
            std::cerr << "  2. 网络连接是否正常" << std::endl;
            std::cerr << "  3. API 密钥是否正确配置" << std::endl;
            return 1;
        }
        
        std::cout << "✅ Agent 初始化成功" << std::endl;
        std::cout << "Agent 名称: " << agent->getAgentName() << std::endl;
        std::cout << "Agent 版本: " << agent->getAgentVersion() << std::endl;
        
        // 5. 显示可用工具
        std::cout << "\n🔧 可用工具:" << std::endl;
        auto tools = agent->getAvailableTools();
        if (tools.empty()) {
            std::cout << "  暂无可用工具" << std::endl;
        } else {
            for (const auto& tool : tools) {
                std::cout << "  - " << tool.name << ": " << tool.description << std::endl;
            }
        }
        
        // 6. 基本对话测试
        std::cout << "\n💬 基本对话测试:" << std::endl;
        
        std::vector<std::string> test_messages = {
            "你好，请介绍一下你自己",
            "你能帮我做什么？",
            "请告诉我今天的天气如何",
            "谢谢你的帮助"
        };
        
        for (const auto& message : test_messages) {
            std::cout << "\n用户: " << message << std::endl;
            
            try {
                std::string response = agent->processMessage(message);
                std::cout << "助手: " << response << std::endl;
            } catch (const std::exception& e) {
                std::cout << "助手: 抱歉，处理您的消息时出现了错误: " << e.what() << std::endl;
            }
            
            // 添加延迟，避免请求过于频繁
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
        
        // 7. 工具调用测试（如果有工具）
        if (!tools.empty()) {
            std::cout << "\n🔧 工具调用测试:" << std::endl;
            std::string tool_message = "请帮我搜索一些信息";
            std::cout << "用户: " << tool_message << std::endl;
            
            try {
                std::string response = agent->processMessage(tool_message);
                std::cout << "助手: " << response << std::endl;
            } catch (const std::exception& e) {
                std::cout << "助手: 工具调用失败: " << e.what() << std::endl;
            }
        }
        
        // 8. 显示对话统计
        std::cout << "\n📊 对话统计:" << std::endl;
        auto context = agent->getConversationContext();
        std::cout << "  对话轮数: " << context.turn_count << std::endl;
        std::cout << "  连接状态: " << (agent->isConnected() ? "已连接" : "未连接") << std::endl;
        
        // 9. 清理资源
        std::cout << "\n🛑 清理资源..." << std::endl;
        agent->shutdown();
        std::cout << "✅ 清理完成" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "\n🎉 示例程序完成！" << std::endl;
    return 0;
}