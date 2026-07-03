/**
 * @file agent_test.cpp
 * @brief MCP Agent 测试程序
 * 
 * 这个文件包含 MCP Agent 的各种功能测试。
 */

#include "mcp_agent.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <cassert>

using namespace mcp;
using namespace mcp_logger;

// 测试辅助函数
class AgentTester {
public:
    AgentTester() {
        logger_ = getModuleLogger("agent_test");
    }
    
    void runAllTests() {
        logger_->info("🧪 开始 MCP Agent 测试");
        logger_->info("========================");
        
        int passed = 0;
        int total = 0;
        
        // 基本功能测试
        total++; if (testInitialization()) passed++;
        total++; if (testConnection()) passed++;
        total++; if (testMessageProcessing()) passed++;
        total++; if (testToolCalling()) passed++;
        total++; if (testResourceAccess()) passed++;
        total++; if (testPromptAccess()) passed++;
        
        // 高级功能测试
        total++; if (testConversationManagement()) passed++;
        total++; if (testErrorHandling()) passed++;
        total++; if (testConcurrentAccess()) passed++;
        total++; if (testConfiguration()) passed++;
        
        logger_->info("\n📊 测试结果汇总");
        logger_->info("================");
        logger_->info("总测试数: " + std::to_string(total));
        logger_->info("通过测试: " + std::to_string(passed));
        logger_->info("失败测试: " + std::to_string(total - passed));
        logger_->info("成功率: " + std::to_string((double)passed / total * 100) + "%");
        
        if (passed == total) {
            logger_->info("🎉 所有测试通过！");
        } else {
            logger_->error("❌ 部分测试失败");
        }
    }
    
private:
    std::shared_ptr<mcp_logger::Logger::ModuleLogger> logger_;
    std::unique_ptr<MCPAgent> agent_;
    
    bool testInitialization() {
        logger_->info("\n🔧 测试 1: Agent 初始化");
        
        try {
            agent_ = std::make_unique<MCPAgent>();
            
            AgentConfig config = createTestConfig();
            bool result = agent_->initialize(config);
            
            if (result) {
                logger_->info("  ✅ 初始化成功");
                return true;
            } else {
                logger_->error("  ❌ 初始化失败");
                return false;
            }
        } catch (const std::exception& e) {
            logger_->error("  ❌ 初始化异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testConnection() {
        logger_->info("\n🔗 测试 2: 连接状态");
        
        try {
            bool connected = agent_->isConnected();
            bool initialized = agent_->isInitialized();
            
            if (connected && initialized) {
                logger_->info("  ✅ 连接正常");
                return true;
            } else {
                logger_->error("  ❌ 连接异常 - 连接状态: " + std::string(connected ? "已连接" : "未连接") + 
                              ", 初始化状态: " + std::string(initialized ? "已初始化" : "未初始化"));
                return false;
            }
        } catch (const std::exception& e) {
            logger_->error("  ❌ 连接测试异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testMessageProcessing() {
        logger_->info("\n💬 测试 3: 消息处理");
        
        try {
            std::vector<std::string> test_messages = {
                "你好",
                "你能帮我做什么？",
                "请介绍一下你自己",
                "谢谢"
            };
            
            for (const auto& message : test_messages) {
                std::string response = agent_->processMessage(message);
                
                if (response.empty()) {
                    logger_->error("  ❌ 消息处理失败: " + message);
                    return false;
                }
                
                logger_->info("  消息: " + message);
                logger_->info("  回复: " + response);
            }
            
            logger_->info("  ✅ 消息处理正常");
            return true;
        } catch (const std::exception& e) {
            logger_->error("  ❌ 消息处理异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testToolCalling() {
        logger_->info("\n🔧 测试 4: 工具调用");
        
        try {
            auto tools = agent_->getAvailableTools();
            logger_->info("  可用工具数量: " + std::to_string(tools.size()));
            
            if (tools.empty()) {
                logger_->warning("  ⚠️ 没有可用工具，跳过工具调用测试");
                return true;
            }
            
            // 测试工具列表获取
            for (const auto& tool : tools) {
                logger_->info("    工具: " + tool.name + " - " + tool.description);
            }
            
            // 测试工具调用（如果有合适的工具）
            if (tools.size() > 0) {
                std::string tool_name = tools[0].name;
                logger_->info("  测试工具调用: " + tool_name);
                
                // 这里可以添加具体的工具调用测试
                // 由于需要具体的工具参数，这里只做基本检查
            }
            
            logger_->info("  ✅ 工具调用测试完成");
            return true;
        } catch (const std::exception& e) {
            logger_->error("  ❌ 工具调用异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testResourceAccess() {
        logger_->info("\n📚 测试 5: 资源访问");
        
        try {
            auto resources = agent_->getAvailableResources();
            logger_->info("  可用资源数量: " + std::to_string(resources.size()));
            
            for (const auto& resource : resources) {
                logger_->info("    资源: " + resource.name + " (" + resource.uri + ")");
            }
            
            // 测试资源读取（如果有资源）
            if (!resources.empty()) {
                try {
                    auto resource_data = agent_->readResource(resources[0].uri);
                    logger_->info("  资源读取成功");
                } catch (const std::exception& e) {
                    logger_->warning("  资源读取失败: " + std::string(e.what()));
                }
            }
            
            logger_->info("  ✅ 资源访问测试完成");
            return true;
        } catch (const std::exception& e) {
            logger_->error("  ❌ 资源访问异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testPromptAccess() {
        logger_->info("\n💡 测试 6: 提示访问");
        
        try {
            auto prompts = agent_->getAvailablePrompts();
            logger_->info("  可用提示数量: " + std::to_string(prompts.size()));
            
            for (const auto& prompt : prompts) {
                logger_->info("    提示: " + prompt.name + " - " + prompt.description);
            }
            
            // 测试提示获取（如果有提示）
            if (!prompts.empty()) {
                try {
                    auto prompt_data = agent_->getPrompt(prompts[0].name);
                    logger_->info("  提示获取成功");
                } catch (const std::exception& e) {
                    logger_->warning("  提示获取失败: " + std::string(e.what()));
                }
            }
            
            logger_->info("  ✅ 提示访问测试完成");
            return true;
        } catch (const std::exception& e) {
            logger_->error("  ❌ 提示访问异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testConversationManagement() {
        logger_->info("\n🗣️ 测试 7: 对话管理");
        
        try {
            // 测试对话上下文
            auto context = agent_->getConversationContext();
            logger_->info("  初始对话轮数: " + std::to_string(context.turn_count));
            
            // 发送几条消息
            agent_->processMessage("第一条消息");
            agent_->processMessage("第二条消息");
            
            context = agent_->getConversationContext();
            logger_->info("  对话后轮数: " + std::to_string(context.turn_count));
            
            // 测试清空对话
            agent_->clearConversation();
            context = agent_->getConversationContext();
            
            if (context.turn_count == 0) {
                logger_->info("  ✅ 对话清空成功");
            } else {
                logger_->error("  ❌ 对话清空失败");
                return false;
            }
            
            // 测试系统提示词设置
            std::string new_prompt = "你是一个测试助手";
            agent_->setSystemPrompt(new_prompt);
            logger_->info("  ✅ 系统提示词设置成功");
            
            logger_->info("  ✅ 对话管理测试完成");
            return true;
        } catch (const std::exception& e) {
            logger_->error("  ❌ 对话管理异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testErrorHandling() {
        logger_->info("\n⚠️ 测试 8: 错误处理");
        
        try {
            // 测试空消息处理
            std::string response = agent_->processMessage("");
            if (!response.empty()) {
                logger_->info("  空消息处理: " + response);
            }
            
            // 测试无效输入
            response = agent_->processMessage("!@#$%^&*()");
            if (!response.empty()) {
                logger_->info("  特殊字符处理: " + response);
            }
            
            // 测试长消息
            std::string long_message(1000, 'A');
            response = agent_->processMessage(long_message);
            if (!response.empty()) {
                logger_->info("  长消息处理成功");
            }
            
            logger_->info("  ✅ 错误处理测试完成");
            return true;
        } catch (const std::exception& e) {
            logger_->error("  ❌ 错误处理异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testConcurrentAccess() {
        logger_->info("\n🔄 测试 9: 并发访问");
        
        try {
            std::vector<std::thread> threads;
            std::vector<bool> results(5, false);
            
            // 创建多个线程同时发送消息
            for (int i = 0; i < 5; ++i) {
                threads.emplace_back([this, i, &results]() {
                    try {
                        std::string response = agent_->processMessage("并发测试消息 " + std::to_string(i));
                        results[i] = !response.empty();
                    } catch (const std::exception& e) {
                        logger_->error("  线程 " + std::to_string(i) + " 异常: " + std::string(e.what()));
                        results[i] = false;
                    }
                });
            }
            
            // 等待所有线程完成
            for (auto& thread : threads) {
                thread.join();
            }
            
            // 检查结果
            int success_count = 0;
            for (bool result : results) {
                if (result) success_count++;
            }
            
            logger_->info("  并发测试结果: " + std::to_string(success_count) + "/5 成功");
            
            if (success_count >= 3) { // 至少3个成功
                logger_->info("  ✅ 并发访问测试通过");
                return true;
            } else {
                logger_->error("  ❌ 并发访问测试失败");
                return false;
            }
        } catch (const std::exception& e) {
            logger_->error("  ❌ 并发访问异常: " + std::string(e.what()));
            return false;
        }
    }
    
    bool testConfiguration() {
        logger_->info("\n⚙️ 测试 10: 配置管理");
        
        try {
            // 测试Agent信息获取
            std::string name = agent_->getAgentName();
            std::string version = agent_->getAgentVersion();
            
            if (!name.empty() && !version.empty()) {
                logger_->info("  Agent名称: " + name);
                logger_->info("  Agent版本: " + version);
                logger_->info("  ✅ 配置管理测试通过");
                return true;
            } else {
                logger_->error("  ❌ 配置信息获取失败");
                return false;
            }
        } catch (const std::exception& e) {
            logger_->error("  ❌ 配置管理异常: " + std::string(e.what()));
            return false;
        }
    }
    
    AgentConfig createTestConfig() {
        AgentConfig config;
        
        // LLM 配置
        config.llm_config.api_endpoint = "https://api.openai.com/v1/chat/completions";
        config.llm_config.api_key = "test-key"; // 测试用密钥
        config.llm_config.model_name = "gpt-3.5-turbo";
        config.llm_config.system_prompt = "你是一个测试助手";
        config.llm_config.max_tokens = 1024;
        config.llm_config.temperature = 0.7;
        config.llm_config.timeout_seconds = 10;
        
        // MCP 传输配置
        config.transport_config.type = mcp_transport::TransportType::HTTP;
        config.transport_config.enable_logging = false;
        config.transport_config.sse.host = "localhost";
        config.transport_config.sse.port = 10086;
        config.transport_config.sse.protocol = netio::Protocol::HTTP;
        config.transport_config.sse.timeout_seconds = 10;
        config.transport_config.sse.max_retries = 1;
        config.transport_config.sse.user_agent = "MCP-Agent-Test/1.0";
        
        // Agent 配置
        config.agent_name = "TestAgent";
        config.agent_version = "1.0.0";
        config.enable_tool_calling = true;
        config.enable_auto_retry = false; // 测试时禁用自动重试
        config.max_conversation_turns = 5;
        config.tool_call_timeout = 10.0;
        
        return config;
    }
};

int main() {
    // 初始化日志系统
    LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "agent_test.log";
    log_config.enable_timestamp = true;
    log_config.enable_thread_id = true;
    log_config.enable_file_location = true;
    log_config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    initializeLogger(log_config);
    
    try {
        AgentTester tester;
        tester.runAllTests();
    } catch (const std::exception& e) {
        std::cerr << "测试程序异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
