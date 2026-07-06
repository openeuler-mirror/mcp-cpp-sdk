#include "named_pipe.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>

using namespace mcp;

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/named_pipe_client.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    // 获取管道路径（默认为 /tmp/mcp_named_pipe）
    std::string pipe_path = "./tmp/mcp_named_pipe";
    if (argc > 1) {
        pipe_path = argv[1];
    }
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("=== Named Pipe Client Example ===");
    mcp_logger::getModuleLogger("named_pipe_client")->info("Pipe path: " + pipe_path);
    mcp_logger::getModuleLogger("named_pipe_client")->info("Process ID: " + std::to_string(getpid()));
    
    // 创建命名管道实例
    NamedPipe named_pipe;
    
    // 配置命名管道（客户端模式 - 写入）
    NamedPipeConfig config;
    config.pipe_path = pipe_path;
    config.mode = NamedPipeMode::WRITE;
    config.buffer_size = 4096;
    config.write_timeout_ms = 1000;
    config.auto_create = false;  // 客户端不创建管道，只连接
    
    // 启动命名管道
    if (!named_pipe.start(config)) {
        mcp_logger::getModuleLogger("named_pipe_client")->error("Failed to start named pipe client");
        mcp_logger::getModuleLogger("named_pipe_client")->error("Make sure the server is running first");
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Named pipe client started successfully");
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (!named_pipe.isConnected()) {
        mcp_logger::getModuleLogger("named_pipe_client")->error("Not connected to server");
        named_pipe.stop();
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    // 发送测试消息
    std::vector<std::string> test_messages = {
        R"({"jsonrpc": "2.0", "id": 1, "method": "ping", "params": {}})",
        R"({"jsonrpc": "2.0", "id": 2, "method": "echo", "params": {"message": "Hello from client!"}})",
        R"({"jsonrpc": "2.0", "id": 3, "method": "test", "params": {"data": "test data"}})",
    };
    
    for (size_t i = 0; i < test_messages.size(); ++i) {
        mcp_logger::getModuleLogger("named_pipe_client")->info(
            "Sending message " + std::to_string(i + 1) + "/" + std::to_string(test_messages.size())
        );
        
        if (named_pipe.sendMessage(test_messages[i])) {
            mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Message sent: " + test_messages[i]);
        } else {
            mcp_logger::getModuleLogger("named_pipe_client")->error("❌ Failed to send message");
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // 发送一些简单的文本消息
    mcp_logger::getModuleLogger("named_pipe_client")->info("Sending simple text messages...");
    for (int i = 1; i <= 5; ++i) {
        std::string msg = "Message #" + std::to_string(i) + " from client";
        if (named_pipe.sendMessage(msg)) {
            mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Sent: " + msg);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    
    // 停止命名管道
    mcp_logger::getModuleLogger("named_pipe_client")->info("Stopping named pipe client...");
    named_pipe.stop();
    mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Named pipe client stopped");
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}

