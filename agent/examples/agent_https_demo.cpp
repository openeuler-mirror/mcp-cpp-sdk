/**
 * @file agent_https_demo.cpp
 * @brief MCP Agent HTTPS 演示程序
 * 
 * 这个文件演示如何使用 MCP Agent 通过 HTTPS/SSE 传输进行智能对话和工具调用。
 * 与 agent_ipc_demo.cpp 的主要区别是使用 HTTPS 传输而不是 IPC 传输。
 */

 #include "../include/mcp_agent.h"
 #include <iostream>
 #include <string>
 #include <thread>
 #include <chrono>
 #include <cstdlib>
 #include <cstring>
 
 using namespace mcp;
 using namespace mcp_logger;
 
 int main() {
     // 初始化日志系统
     LogConfig log_config;
     log_config.level = mcp_logger::LogLevel::DEBUG;
     log_config.target = mcp_logger::LogTarget::BOTH;
     log_config.file_path = "agent_https_demo.log";
     log_config.enable_timestamp = true;
     log_config.enable_thread_id = true;
     log_config.enable_file_location = true;
         log_config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    initializeLogger(log_config);
    
    auto logger = getModuleLogger("agent_https_demo");
    logger->info("🤖 MCP Agent HTTPS 演示程序启动");
    logger->info("================================");
     
     try {
         // 创建 Agent 实例
         auto agent = std::make_unique<MCPAgent>();
         
         // 配置 Agent
         AgentConfig agent_config;
         
         // LLM 配置
         agent_config.llm_config.api_endpoint = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions";
         // 从环境变量读取 API 密钥（安全方式）
        //const char* api_key_env = std::getenv("MCP_API_KEY");
        const char* api_key_env = "sk-11bd3e1feed241e6aedab31b7ce07ce6";
        if (!api_key_env || strlen(api_key_env) == 0) {
            logger->error("❌ MCP_API_KEY environment variable not set");
            logger->info("💡 Please set the API key: export MCP_API_KEY=your_api_key");
            return 1;
        }
        agent_config.llm_config.api_key = std::string(api_key_env);
         agent_config.llm_config.model_name = "qwen-plus";
         agent_config.llm_config.system_prompt = 
             "你是一个智能助手，可以调用各种工具来帮助用户完成任务。"
             "你可以使用文件系统操作、搜索等工具。"
             "请根据用户的需求，智能地选择和调用合适的工具。";
         agent_config.llm_config.max_tokens = 2048;
         agent_config.llm_config.temperature = 0.7;
         agent_config.llm_config.timeout_seconds = 30;
         
        // MCP HTTPS 传输配置
        agent_config.transport_config.type = mcp_transport::TransportType::HTTPS;
        agent_config.transport_config.enable_logging = false;
        
        // SSE 配置 - 连接到远程 MCP 服务器
        agent_config.transport_config.sse.host = "mcp.didichuxing.com";
        agent_config.transport_config.sse.port = 443;
        agent_config.transport_config.sse.protocol = netio::Protocol::HTTPS;
        agent_config.transport_config.sse.timeout_seconds = 30;
        agent_config.transport_config.sse.max_retries = 3;
        agent_config.transport_config.sse.user_agent = "MCP-Transport-SSE/1.0";
        agent_config.transport_config.sse.keep_alive = true;
        agent_config.transport_config.sse.auto_reconnect = true;
        agent_config.transport_config.sse.reconnect_interval = 5;
        agent_config.transport_config.sse.headers["Content-Type"] = "application/json; charset=utf-8";
        agent_config.transport_config.sse.headers["Accept"] = "application/json, text/event-stream";
        agent_config.transport_config.sse.headers["User-Agent"] = "MCP-Transport-SSE/1.0";
        
        // 测试端点列表
        std::vector<std::string> test_endpoints = {
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ",
            "/mcp-servers?key=EDPjGgm1axpZxdYn1Z3xxNyRB4nw07KQ"
        };
        
        // 设置测试端点
        agent_config.endpoints = test_endpoints;
        
        // Agent 基本信息
        agent_config.agent_name = "MCP智能助手(HTTPS)";
        agent_config.agent_version = "1.0.0";
        agent_config.enable_tool_calling = true;
        agent_config.enable_auto_retry = true;
        agent_config.max_conversation_turns = 10;
        agent_config.tool_call_timeout = 30.0;
         
        // 初始化 Agent
        logger->info("🚀 初始化 MCP Agent (HTTPS模式)...");
        logger->info("📋 HTTPS 配置信息:");
        logger->info("  传输类型: SSE (HTTPS)");
        logger->info("  主机: " + agent_config.transport_config.sse.host);
        logger->info("  端口: " + std::to_string(agent_config.transport_config.sse.port));
        logger->info("  协议: HTTPS");
        logger->info("  超时时间: " + std::to_string(agent_config.transport_config.sse.timeout_seconds) + "秒");
        logger->info("  最大重试次数: " + std::to_string(agent_config.transport_config.sse.max_retries));
        logger->info("  自动重连: " + std::string(agent_config.transport_config.sse.auto_reconnect ? "启用" : "禁用"));
        
        if (!agent->initialize(agent_config)) {
            logger->error("❌ Agent 初始化失败");
            logger->info("💡 请检查:");
            logger->info("  1. HTTPS 服务器是否正在运行");
            logger->info("  2. 服务器地址和端口是否正确: " + agent_config.transport_config.sse.host + ":" + std::to_string(agent_config.transport_config.sse.port));
            logger->info("  3. 网络连接是否正常");
            logger->info("  4. SSL证书是否有效");
            logger->info("  5. API 密钥是否正确配置");
            return 1;
        }
        
        logger->info("✅ Agent 初始化成功");
        logger->info("Agent 名称: " + agent->getAgentName());
        logger->info("Agent 版本: " + agent->getAgentVersion());
         
        // 等待 HTTPS 连接建立
        logger->info("⏳ 等待 HTTPS 连接建立...");
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // 检查连接状态
        logger->info("🔍 连接状态检查:");
        logger->info("  连接状态: " + std::string(agent->isConnected() ? "已连接" : "未连接"));
        
        if (!agent->isConnected()) {
            logger->error("❌ 错误: Agent 未连接到 HTTPS 服务器");
            logger->info("💡 请确保:");
            logger->info("  1. HTTPS 服务器正在运行在 " + agent_config.transport_config.sse.host + ":" + std::to_string(agent_config.transport_config.sse.port));
            logger->info("  2. 服务器支持 HTTPS 和 SSE 传输");
            logger->info("  3. SSL 证书配置正确");
            logger->info("  4. 防火墙允许该端口连接");
             return 1;
         }
         
         // 显示可用工具
         auto tools = agent->getAvailableTools();
         logger->info("🔧 可用工具数量: " + std::to_string(tools.size()));
         for (const auto& tool : tools) {
             logger->info("  - " + tool.name + ": " + tool.description);
         }
         
         // 显示可用资源
         try {
             auto resources = agent->getAvailableResources();
             logger->info("📚 可用资源数量: " + std::to_string(resources.size()));
             for (const auto& resource : resources) {
                 logger->info("  - " + resource.name + " (" + resource.uri + ")");
             }
         } catch (const std::exception& e) {
             logger->warning("⚠️ 获取资源列表失败（服务器可能不支持资源功能）: " + std::string(e.what()));
         }
         
         // 显示可用提示
         try {
             auto prompts = agent->getAvailablePrompts();
             logger->info("💡 可用提示数量: " + std::to_string(prompts.size()));
             for (const auto& prompt : prompts) {
                 logger->info("  - " + prompt.name + ": " + prompt.description);
             }
         } catch (const std::exception& e) {
             logger->warning("⚠️ 获取提示列表失败（服务器可能不支持提示功能）: " + std::string(e.what()));
         }
         
         // 交互式对话
         logger->info("\n💬 开始交互式对话 (输入 'quit' 退出)");
         logger->info("=====================================");
         
         std::string user_input;
         while (true) {
             std::cout << "\n用户: ";
             std::getline(std::cin, user_input);
             
             if (user_input == "quit" || user_input == "exit") {
                 logger->info("👋 用户退出对话");
                 break;
             }
             
             if (user_input.empty()) {
                 continue;
             }
             
             if (user_input == "clear") {
                 agent->clearConversation();
                 logger->info("🗑️ 对话历史已清空");
                 continue;
             }
             
             if (user_input == "tools") {
                 auto tools = agent->getAvailableTools();
                 std::cout << "\n可用工具:\n";
                 for (const auto& tool : tools) {
                     std::cout << "  - " << tool.name << ": " << tool.description << "\n";
                 }
                 continue;
             }
             
             if (user_input == "status") {
                 std::cout << "\nAgent 状态:\n";
                 std::cout << "  连接状态: " << (agent->isConnected() ? "已连接" : "未连接") << "\n";
                 std::cout << "  初始化状态: " << (agent->isInitialized() ? "已初始化" : "未初始化") << "\n";
                 auto context = agent->getConversationContext();
                 std::cout << "  对话轮数: " << context.turn_count << "\n";
                 continue;
             }
             
             if (user_input == "resources") {
                 try {
                     auto resources = agent->getAvailableResources();
                     std::cout << "\n可用资源:\n";
                     if (resources.empty()) {
                         std::cout << "  暂无可用资源\n";
                     } else {
                         for (const auto& resource : resources) {
                             std::cout << "  - " << resource.name << " (" << resource.uri << ")\n";
                         }
                     }
                 } catch (const std::exception& e) {
                     std::cout << "\n⚠️ 获取资源列表失败（服务器可能不支持资源功能）: " << e.what() << "\n";
                     logger->warning("获取资源列表失败: " + std::string(e.what()));
                 }
                 continue;
             }
             
             if (user_input == "prompts") {
                 try {
                     auto prompts = agent->getAvailablePrompts();
                     std::cout << "\n可用提示:\n";
                     if (prompts.empty()) {
                         std::cout << "  暂无可用提示\n";
                     } else {
                         for (const auto& prompt : prompts) {
                             std::cout << "  - " << prompt.name << ": " << prompt.description << "\n";
                         }
                     }
                 } catch (const std::exception& e) {
                     std::cout << "\n⚠️ 获取提示列表失败（服务器可能不支持提示功能）: " << e.what() << "\n";
                     logger->warning("获取提示列表失败: " + std::string(e.what()));
                 }
                 continue;
             }
             
             // 处理用户消息
             logger->info("📤 用户输入: " + user_input);
             
             try {
                 std::string response = agent->processMessage(user_input);
                 std::cout << "\n助手: " << response << "\n";
                 logger->info("📥 助手回复: " + response);
             } catch (const std::exception& e) {
                 std::string error_msg = "处理消息时发生错误: " + std::string(e.what());
                 std::cout << "\n助手: " << error_msg << "\n";
                 logger->error("❌ " + error_msg);
             }
         }
         
         // 演示特定功能
         logger->info("\n🧪 演示特定功能");
         logger->info("==================");
         
         // 演示带上下文的对话
         logger->info("📝 演示带上下文的对话");
         std::string context = "用户正在浏览项目文件";
         std::string response = agent->processMessage("帮我查看当前目录下的文件", context);
         std::cout << "\n带上下文的回复: " << response << "\n";
         
         // 演示工具调用
         logger->info("🔧 演示工具调用");
         response = agent->processMessage("帮我读取README.md文件的内容");
         std::cout << "\n工具调用回复: " << response << "\n";
         
         // 演示资源读取
         logger->info("📚 演示资源读取");
         try {
             auto demo_resources = agent->getAvailableResources();
             if (!demo_resources.empty()) {
                 try {
                     auto resource_data = agent->readResource(demo_resources[0].uri);
                     logger->info("资源内容: " + resource_data.dump(2));
                 } catch (const std::exception& e) {
                     logger->error("读取资源失败: " + std::string(e.what()));
                 }
             } else {
                 logger->info("  没有可用的资源");
             }
         } catch (const std::exception& e) {
             logger->warning("⚠️ 获取资源列表失败（服务器可能不支持资源功能）: " + std::string(e.what()));
         }
         
         // 演示提示获取
         logger->info("💡 演示提示获取");
         try {
             auto demo_prompts = agent->getAvailablePrompts();
             if (!demo_prompts.empty()) {
                 try {
                     auto prompt_data = agent->getPrompt(demo_prompts[0].name);
                     logger->info("提示内容: " + prompt_data.dump(2));
                 } catch (const std::exception& e) {
                     logger->error("获取提示失败: " + std::string(e.what()));
                 }
             } else {
                 logger->info("  没有可用的提示");
             }
         } catch (const std::exception& e) {
             logger->warning("⚠️ 获取提示列表失败（服务器可能不支持提示功能）: " + std::string(e.what()));
         }
         
         // 关闭 Agent
         logger->info("\n🛑 关闭 MCP Agent...");
         agent->shutdown();
         logger->info("✅ MCP Agent 已关闭");
         
     } catch (const std::exception& e) {
         logger->error("❌ 程序异常: " + std::string(e.what()));
         return 1;
     }
     
     logger->info("\n✅ MCP Agent HTTPS 演示程序完成");
     return 0;
 }