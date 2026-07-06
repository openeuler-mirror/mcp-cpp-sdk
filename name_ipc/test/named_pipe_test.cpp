#include "named_pipe.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cassert>
#include <unistd.h>
#include <sys/stat.h>

using namespace mcp;

// 测试消息处理器
class TestMessageHandler : public NamedPipeMessageHandler {
public:
    std::vector<NamedPipeMessage> received_messages;
    std::vector<std::string> errors;
    bool connected = false;
    bool disconnected = false;
    
    void onMessage(const NamedPipeMessage& message) override {
        received_messages.push_back(message);
    }
    
    void onError(const std::string& error) override {
        errors.push_back(error);
    }
    
    void onConnected() override {
        connected = true;
    }
    
    void onDisconnected() override {
        disconnected = true;
    }
};

void testBasicCommunication() {
    std::cout << "=== Test: Basic Communication ===" << std::endl;
    
    std::string pipe_path = "/tmp/test_named_pipe_" + std::to_string(getpid());
    
    // 创建服务器（读端）
    NamedPipe server;
    NamedPipeConfig server_config;
    server_config.pipe_path = pipe_path;
    server_config.mode = NamedPipeMode::READ;
    server_config.buffer_size = 1024;
    
    auto server_handler = std::make_shared<TestMessageHandler>();
    server.setMessageHandler(server_handler);
    
    assert(server.start(server_config));
    std::cout << "✅ Server started" << std::endl;
    
    // 等待服务器就绪
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 创建客户端（写端）
    NamedPipe client;
    NamedPipeConfig client_config;
    client_config.pipe_path = pipe_path;
    client_config.mode = NamedPipeMode::WRITE;
    client_config.buffer_size = 1024;
    client_config.auto_create = false;
    
    assert(client.start(client_config));
    std::cout << "✅ Client started" << std::endl;
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 发送消息
    std::string test_message = "Hello from client!";
    assert(client.sendMessage(test_message));
    std::cout << "✅ Message sent" << std::endl;
    
    // 等待消息处理
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 检查消息接收
    assert(server.hasMessage());
    NamedPipeMessage received = server.getMessage();
    assert(received.content == test_message);
    std::cout << "✅ Message received: " << received.content << std::endl;
    
    // 清理
    client.stop();
    server.stop();
    
    // 清理FIFO文件
    unlink(pipe_path.c_str());
    
    std::cout << "✅ Basic communication test passed" << std::endl;
}

void testMultipleMessages() {
    std::cout << "\n=== Test: Multiple Messages ===" << std::endl;
    
    std::string pipe_path = "/tmp/test_named_pipe_multi_" + std::to_string(getpid());
    
    // 创建服务器
    NamedPipe server;
    NamedPipeConfig server_config;
    server_config.pipe_path = pipe_path;
    server_config.mode = NamedPipeMode::READ;
    
    auto server_handler = std::make_shared<TestMessageHandler>();
    server.setMessageHandler(server_handler);
    
    assert(server.start(server_config));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 创建客户端
    NamedPipe client;
    NamedPipeConfig client_config;
    client_config.pipe_path = pipe_path;
    client_config.mode = NamedPipeMode::WRITE;
    client_config.auto_create = false;
    
    assert(client.start(client_config));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 发送多条消息
    std::vector<std::string> messages = {
        "Message 1",
        "Message 2",
        "Message 3",
        "Message 4",
        "Message 5"
    };
    
    for (const auto& msg : messages) {
        assert(client.sendMessage(msg));
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    // 检查所有消息
    assert(server.getAllMessages().size() == messages.size());
    std::cout << "✅ All " << messages.size() << " messages received" << std::endl;
    
    // 清理
    client.stop();
    server.stop();
    unlink(pipe_path.c_str());
    
    std::cout << "✅ Multiple messages test passed" << std::endl;
}

void testJSONMessages() {
    std::cout << "\n=== Test: JSON Messages ===" << std::endl;
    
    std::string pipe_path = "/tmp/test_named_pipe_json_" + std::to_string(getpid());
    
    // 创建服务器
    NamedPipe server;
    NamedPipeConfig server_config;
    server_config.pipe_path = pipe_path;
    server_config.mode = NamedPipeMode::READ;
    
    auto server_handler = std::make_shared<TestMessageHandler>();
    server.setMessageHandler(server_handler);
    
    assert(server.start(server_config));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 创建客户端
    NamedPipe client;
    NamedPipeConfig client_config;
    client_config.pipe_path = pipe_path;
    client_config.mode = NamedPipeMode::WRITE;
    client_config.auto_create = false;
    
    assert(client.start(client_config));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 发送JSON消息
    std::string json_msg = R"({"jsonrpc": "2.0", "id": 123, "method": "test", "params": {}})";
    assert(client.sendMessage(json_msg));
    
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 检查消息
    assert(server.hasMessage());
    NamedPipeMessage received = server.getMessage();
    assert(received.content == json_msg);
    assert(received.id == "123");
    std::cout << "✅ JSON message received with ID: " << received.id << std::endl;
    
    // 清理
    client.stop();
    server.stop();
    unlink(pipe_path.c_str());
    
    std::cout << "✅ JSON messages test passed" << std::endl;
}

int main() {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::WARNING;  // 减少测试输出
    log_config.target = mcp_logger::LogTarget::CONSOLE;
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    std::cout << "Starting named pipe tests..." << std::endl;
    
    try {
        testBasicCommunication();
        testMultipleMessages();
        testJSONMessages();
        
        std::cout << "\n✅ All tests passed!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed: " << e.what() << std::endl;
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}

