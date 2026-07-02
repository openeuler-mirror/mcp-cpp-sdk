#include "pipe_communication.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <chrono>

using namespace mcp;

class TestMessageHandler : public MessageHandler {
public:
    std::vector<std::string> received_messages;
    std::vector<std::string> received_errors;
    
    void on_message(const Message& message) override {
        received_messages.push_back(message.content);
    }
    
    void on_error(const std::string& error) override {
        received_errors.push_back(error);
    }
};

void test_basic_functionality() {
    std::cout << "Testing basic functionality..." << std::endl;
    
    PipeCommunication pipe_comm;
    auto handler = std::make_shared<TestMessageHandler>();
    pipe_comm.set_message_handler(handler);
    
    // 测试未启动状态
    assert(!pipe_comm.is_running());
    assert(pipe_comm.get_process_id() == -1);
    assert(!pipe_comm.is_process_alive());
    
    std::cout << "Basic functionality test passed" << std::endl;
}

void test_pipe_creation() {
    std::cout << "Testing pipe creation..." << std::endl;
    
    PipeCommunication pipe_comm;
    PipeConfig config;
    config.command = "echo";
    config.args = {"hello"};
    
    // 测试启动
    bool started = pipe_comm.start(config);
    if (started) {
        assert(pipe_comm.is_running());
        assert(pipe_comm.get_process_id() > 0);
        
        // 等待进程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 测试进程状态
        bool alive = pipe_comm.is_process_alive();
        std::cout << "Process alive: " << alive << std::endl;
        
        // 停止
        pipe_comm.stop();
        assert(!pipe_comm.is_running());
    }
    
    std::cout << "Pipe creation test completed" << std::endl;
}

void test_message_sending() {
    std::cout << "Testing message sending..." << std::endl;
    
    PipeCommunication pipe_comm;
    auto handler = std::make_shared<TestMessageHandler>();
    pipe_comm.set_message_handler(handler);
    
    PipeConfig config;
    config.command = "cat"; // 简单的回显命令
    config.args = {};
    
    if (pipe_comm.start(config)) {
        // 等待进程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 发送消息
        std::string test_message = "Hello, World!";
        bool sent = pipe_comm.send_message(test_message);
        std::cout << "Message sent: " << sent << std::endl;
        
        // 等待消息处理
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 停止
        pipe_comm.stop();
    }
    
    std::cout << "Message sending test completed" << std::endl;
}

int main() {
    std::cout << "Starting pipe communication tests..." << std::endl;
    
    try {
        test_basic_functionality();
        test_pipe_creation();
        test_message_sending();
        
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
