#include "named_pipe.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <unistd.h>

using namespace mcp;

// 示例消息处理器
class ExampleMessageHandler : public NamedPipeMessageHandler {
public:
    int message_count = 0;
    
    void onMessage(const NamedPipeMessage& message) override {
        message_count++;
        mcp_logger::getModuleLogger("named_pipe_server")->info(
            "[Message #" + std::to_string(message_count) + "] Received: " + message.content
        );
    }
    
    void onError(const std::string& error) override {
        mcp_logger::getModuleLogger("named_pipe_server")->error("[ERROR] " + error);
    }
    
    void onConnected() override {
        mcp_logger::getModuleLogger("named_pipe_server")->info("Client connected");
    }
    
    void onDisconnected() override {
        mcp_logger::getModuleLogger("named_pipe_server")->info("Client disconnected");
    }
};

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/named_pipe_server.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    // 获取管道路径（默认为 /tmp/mcp_named_pipe）
    std::string pipe_path = "./tmp/mcp_named_pipe";
    if (argc > 1) {
        pipe_path = argv[1];
    }
    
    mcp_logger::getModuleLogger("named_pipe_server")->info("=== Named Pipe Server Example ===");
    mcp_logger::getModuleLogger("named_pipe_server")->info("Pipe path: " + pipe_path);
    mcp_logger::getModuleLogger("named_pipe_server")->info("Process ID: " + std::to_string(getpid()));
    
    // 创建命名管道实例
    NamedPipe named_pipe;
    
    // 设置消息处理器
    auto handler = std::make_shared<ExampleMessageHandler>();
    named_pipe.setMessageHandler(handler);
    
    // 配置命名管道（服务器模式 - 读取）
    NamedPipeConfig config;
    config.pipe_path = pipe_path;
    config.mode = NamedPipeMode::READ;
    config.buffer_size = 4096;
    config.read_timeout_ms = 1000;
    config.auto_create = true;
    
    // 启动命名管道
    if (!named_pipe.start(config)) {
        mcp_logger::getModuleLogger("named_pipe_server")->error("Failed to start named pipe server");
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    mcp_logger::getModuleLogger("named_pipe_server")->info("✅ Named pipe server started successfully");
    mcp_logger::getModuleLogger("named_pipe_server")->info("Waiting for messages... (Press Ctrl+C to stop)");
    
    // 主循环 - 等待消息
    try {
        while (named_pipe.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 检查是否有消息
            if (named_pipe.hasMessage()) {
                NamedPipeMessage msg = named_pipe.getMessage();
                mcp_logger::getModuleLogger("named_pipe_server")->info("Got message: " + msg.content);
            }
        }
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("named_pipe_server")->error("Exception: " + std::string(e.what()));
    }
    
    // 停止命名管道
    mcp_logger::getModuleLogger("named_pipe_server")->info("Stopping named pipe server...");
    named_pipe.stop();
    mcp_logger::getModuleLogger("named_pipe_server")->info("✅ Named pipe server stopped");
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}

