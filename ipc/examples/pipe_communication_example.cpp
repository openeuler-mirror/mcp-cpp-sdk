#include "pipe_communication.h"
#include "mcp_protocol_messages.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>     // for std::time
#include <unistd.h>  // for getpid()

using namespace mcp;
using namespace mcp_protocol;

// 示例消息处理器
class ExampleMessageHandler : public MessageHandler {
public:
    int message_count = 0;
    
    void on_message(const Message& message) override {
        message_count++;
        std::cout << "[Message #" << message_count << "] Received: " << message.content << std::endl;
    }
    
    void on_error(const std::string& error) override {
        std::cerr << "[ERROR] " << error << std::endl;
    }
};

// 函数声明
bool setup_pipe_communication(PipeCommunication& pipe_comm);
void print_communication_info(const PipeCommunication& pipe_comm, const PipeConfig& config);
bool check_server_status(const PipeCommunication& pipe_comm);
void test_immediate_tool_call(PipeCommunication& pipe_comm);
void test_batch_messages(PipeCommunication& pipe_comm);
void test_request_response_mode(PipeCommunication& pipe_comm);
void test_list_directories_tool(PipeCommunication& pipe_comm);
void test_write_file_tool(PipeCommunication& pipe_comm);
void test_edit_file_tool(PipeCommunication& pipe_comm);
void test_search_files_tool(PipeCommunication& pipe_comm);
void test_error_cases(PipeCommunication& pipe_comm);
void print_process_status(const PipeCommunication& pipe_comm);
void print_statistics(PipeCommunication& pipe_comm);
void run_continuous_test(PipeCommunication& pipe_comm);

int main() {
    // 创建管道通信实例
    PipeCommunication pipe_comm;
    
    // 设置管道通信
    if (!setup_pipe_communication(pipe_comm)) {
        return 1;
    }
    
    // 检查服务器状态
    if (!check_server_status(pipe_comm)) {
        return 1;
    }
    
    // 执行各种测试
    test_immediate_tool_call(pipe_comm);
    test_batch_messages(pipe_comm);
    test_request_response_mode(pipe_comm);
    test_error_cases(pipe_comm);
    
    // 打印状态和统计信息
    print_process_status(pipe_comm);
    print_statistics(pipe_comm);
    
    // 运行持续测试（可选）
    run_continuous_test(pipe_comm);
    
    // 停止管道通信
    std::cout << "\n🛑 停止管道通信..." << std::endl;
    pipe_comm.stop();
    std::cout << "✅ 管道通信已停止" << std::endl;
    return 0;
}

// 设置管道通信
bool setup_pipe_communication(PipeCommunication& pipe_comm) {
    // 设置消息处理器（暂时注释掉，直接使用同步方式）
    // auto handler = std::make_shared<ExampleMessageHandler>();
    // pipe_comm.set_message_handler(handler);
    
    // 配置管道通信 - 使用MCP服务器
    PipeConfig config;
    config.command = "npx";
    config.args = {"-y", "@modelcontextprotocol/server-filesystem", "."};
    config.env_vars["NODE_ENV"] = "development";
    config.buffer_size = 2048;
    config.read_timeout_ms = 100;  // 增加超时时间
    
    // 启动管道通信
    if (!pipe_comm.start(config)) {
        std::cerr << "Failed to start pipe communication" << std::endl;
        std::cerr << "请确保已安装 Node.js 和 npm，并且可以运行 npx 命令" << std::endl;
        std::cerr << "可以尝试运行: npm install -g @modelcontextprotocol/server-filesystem" << std::endl;
        return false;
    }
    
    print_communication_info(pipe_comm, config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return true;
}

// 打印通信信息
void print_communication_info(const PipeCommunication& pipe_comm, const PipeConfig& config) {
    std::cout << "=== 管道通信启动成功 ===" << std::endl;
    std::cout << "主进程ID: " << getpid() << std::endl;
    std::cout << "子进程ID: " << pipe_comm.get_process_id() << std::endl;
    std::cout << "命令: " << config.command;
    for (const auto& arg : config.args) {
        std::cout << " " << arg;
    }
    std::cout << std::endl;
    std::cout << "缓冲区大小: " << config.buffer_size << " bytes" << std::endl;
    std::cout << "读取超时: " << config.read_timeout_ms << " ms" << std::endl;
    std::cout << "环境变量: ";
    for (const auto& env : config.env_vars) {
        std::cout << env.first << "=" << env.second << " ";
    }
    std::cout << std::endl;
}

// 检查服务器状态
bool check_server_status(const PipeCommunication& pipe_comm) {
    std::cout << "\n=== 检查MCP服务器状态 ===" << std::endl;
    std::cout << "🔄 管道通信运行中: " << (pipe_comm.is_running() ? "是" : "否") << std::endl;
    std::cout << "💓 子进程存活: " << (pipe_comm.is_process_alive() ? "是" : "否") << std::endl;
    
    if (!pipe_comm.is_running() || !pipe_comm.is_process_alive()) {
        std::cout << "❌ MCP服务器没有正常运行，无法进行工具调用测试" << std::endl;
        std::cout << "请检查是否安装了 @modelcontextprotocol/server-filesystem 包" << std::endl;
        std::cout << "可以运行: npm install -g @modelcontextprotocol/server-filesystem" << std::endl;
        return false;
    }
    return true;
}

// 测试立即工具调用
void test_immediate_tool_call(PipeCommunication& pipe_comm) {
    std::cout << "\n=== 立即测试工具调用 ===" << std::endl;
    std::string test_request = create_list_allowed_directories_message(999);
    std::cout << "📤 发送测试请求: " << test_request << std::endl;
    
    auto response_future = pipe_comm.send_request(test_request);
    std::cout << "✅ 测试请求发送成功" << std::endl;
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        std::cout << "Received response: " << response.dump() << std::endl;
    }
}

// 测试批量消息
void test_batch_messages(PipeCommunication& pipe_comm) {
    std::vector<std::string> test_messages = {
        create_initialize_message("cpp-mcp-client", "1.0.0"),
        create_ping_message(1),
        create_list_tools_message(2),
        create_list_allowed_directories_message(3),
        create_write_file_message("test_output.txt", "Hello from C++ MCP client!\nThis is a test file created by the write_file tool.", 4),
        create_create_directory_message("test_dir", 5),
        create_list_directory_message(".", 6),
        create_read_text_file_message("test_output.txt", 5, -1, 7),  // 读取前5行
        create_get_file_info_message("test_output.txt", 8),
    };
    
    for (size_t i = 0; i < test_messages.size(); ++i) {
        const auto& msg = test_messages[i];
        std::cout << "\n--- 消息 " << (i + 1) << "/" << test_messages.size() << " ---" << std::endl;
        std::cout << "📤 发送消息: " << msg << std::endl;
        
        auto response_future = pipe_comm.send_request(msg);
        std::cout << "✅ 消息发送成功" << std::endl;
        
        // 等待一下让子进程处理
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const auto timeout = std::chrono::seconds(1);
        auto status = response_future.wait_for(timeout);
    
        if (status == std::future_status::ready) {
            nlohmann::json response = response_future.get();
            std::cout << "Received response: " << response.dump() << std::endl;
        }
    }
}

// 测试请求-响应模式
void test_request_response_mode(PipeCommunication& pipe_comm) {
    std::cout << "\n=== 请求-响应模式测试 ===" << std::endl;
    
    test_list_directories_tool(pipe_comm);
    test_write_file_tool(pipe_comm);
    test_edit_file_tool(pipe_comm);
    test_search_files_tool(pipe_comm);
}

// 测试list_allowed_directories工具调用
void test_list_directories_tool(PipeCommunication& pipe_comm) {
    std::string directories_request = create_list_allowed_directories_message(100);
    std::cout << "📤 发送list_allowed_directories工具调用: " << directories_request << std::endl;
    
    auto response_future = pipe_comm.send_request(directories_request);
    std::cout << "✅ list_allowed_directories请求发送成功" << std::endl;
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        std::cout << "Received response: " << response.dump() << std::endl;
    }
}

// 测试write_file工具调用
void test_write_file_tool(PipeCommunication& pipe_comm) {
    std::cout << "\n--- 测试write_file工具调用 ---" << std::endl;
    std::string write_file_request = create_write_file_message("response_test.txt", 
        "This file was created by the write_file tool call from C++ MCP client.\n"
        "Timestamp: " + std::to_string(std::time(nullptr)) + "\n"
        "This demonstrates the write_file functionality." + "\n" + "Hello from C++ MCP client! This is a test file", 101);
    std::cout << "📤 发送write_file工具调用: " << write_file_request << std::endl;
    
    auto response_future = pipe_comm.send_request(write_file_request);
    std::cout << "✅ write_file请求发送成功" << std::endl;
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        std::cout << "Received response: " << response.dump() << std::endl;
    }
}

// 测试edit_file工具调用
void test_edit_file_tool(PipeCommunication& pipe_comm) {
    std::cout << "\n--- 测试edit_file工具调用 ---" << std::endl;
    std::vector<std::pair<std::string, std::string>> edits = {
        {"Hello from C++ MCP client!", "Hello from C++ MCP client! [EDITED]"},
        {"This is a test file", "This is a test file [MODIFIED]"}
    };
    std::string edit_file_request = create_edit_file_message("response_test.txt", edits, false, 102);
    std::cout << "📤 发送edit_file工具调用: " << edit_file_request << std::endl;
    
    auto response_future = pipe_comm.send_request(edit_file_request);
    std::cout << "✅ edit_file请求发送成功" << std::endl;
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        std::cout << "Received response: " << response.dump() << std::endl;
    }
}

// 测试search_files工具调用
void test_search_files_tool(PipeCommunication& pipe_comm) {
    std::cout << "\n--- 测试search_files工具调用 ---" << std::endl;
    std::string search_files_request = create_search_files_message(".", "response_test.txt", {}, 103);
    std::cout << "📤 发送search_files工具调用: " << search_files_request << std::endl;
   
    auto response_future = pipe_comm.send_request(search_files_request);
    std::cout << "✅ search_files请求发送成功" << std::endl;
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        std::cout << "Received response: " << response.dump() << std::endl;
    }
}

// 测试错误案例
void test_error_cases(PipeCommunication& pipe_comm) {
    std::cout << "\n=== 错误案例测试 ===" << std::endl;
    
    // 测试1: 无效的JSON消息
    std::cout << "\n--- 测试1: 无效的JSON消息 ---" << std::endl;
    std::string invalid_json = "{\"invalid\": json, \"missing\": \"quote}";
    std::cout << "📤 发送无效JSON: " << invalid_json << std::endl;
    
    auto response_future1 = pipe_comm.send_request(invalid_json);
    std::cout << "✅ 无效JSON发送成功" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status1 = response_future1.wait_for(timeout);
    
    if (status1 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future1.get();
            std::cout << "Received response: " << response.dump() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ 捕获到异常: " << e.what() << std::endl;
        }
    } else {
        std::cout << "⏰ 响应超时" << std::endl;
    }
    
    // 测试2: 不存在的工具调用
    std::cout << "\n--- 测试2: 不存在的工具调用 ---" << std::endl;
    std::string non_existent_tool = R"({
        "jsonrpc": "2.0",
        "id": 999,
        "method": "tools/call",
        "params": {
            "name": "non_existent_tool",
            "arguments": {
                "param1": "value1"
            }
        }
    })";
    std::cout << "📤 发送不存在的工具调用: " << non_existent_tool << std::endl;
    
    auto response_future2 = pipe_comm.send_request(non_existent_tool);
    std::cout << "✅ 不存在工具调用发送成功" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status2 = response_future2.wait_for(timeout);
    
    if (status2 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future2.get();
            std::cout << "Received response: " << response.dump() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ 捕获到异常: " << e.what() << std::endl;
        }
    } else {
        std::cout << "⏰ 响应超时" << std::endl;
    }
    
    // 测试3: 错误的文件路径
    std::cout << "\n--- 测试3: 错误的文件路径 ---" << std::endl;
    std::string invalid_path_request = create_read_text_file_message("/nonexistent/path/file.txt", 5, -1, 888);
    std::cout << "📤 发送错误路径请求: " << invalid_path_request << std::endl;
    
    auto response_future3 = pipe_comm.send_request(invalid_path_request);
    std::cout << "✅ 错误路径请求发送成功" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status3 = response_future3.wait_for(timeout);
    
    if (status3 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future3.get();
            std::cout << "Received response: " << response.dump() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ 捕获到异常: " << e.what() << std::endl;
        }
    } else {
        std::cout << "⏰ 响应超时" << std::endl;
    }
    
    // 测试4: 空的请求
    std::cout << "\n--- 测试4: 空请求 ---" << std::endl;
    std::string empty_request = "";
    std::cout << "📤 发送空请求" << std::endl;
    
    auto response_future4 = pipe_comm.send_request(empty_request);
    std::cout << "✅ 空请求发送成功" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status4 = response_future4.wait_for(timeout);
    
    if (status4 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future4.get();
            std::cout << "Received response: " << response.dump() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ 捕获到异常: " << e.what() << std::endl;
        }
    } else {
        std::cout << "⏰ 响应超时" << std::endl;
    }
    
    // 测试5: 格式正确但参数错误的请求
    std::cout << "\n--- 测试5: 参数错误的请求 ---" << std::endl;
    std::string wrong_params_request = R"({
        "jsonrpc": "2.0",
        "id": 777,
        "method": "tools/call",
        "params": {
            "name": "write_file",
            "arguments": {
                "path": "",
                "content": "test"
            }
        }
    })";
    std::cout << "📤 发送参数错误请求: " << wrong_params_request << std::endl;
    
    auto response_future5 = pipe_comm.send_request(wrong_params_request);
    std::cout << "✅ 参数错误请求发送成功" << std::endl;
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status5 = response_future5.wait_for(timeout);
    
    if (status5 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future5.get();
            std::cout << "Received response: " << response.dump() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "❌ 捕获到异常: " << e.what() << std::endl;
        }
    } else {
        std::cout << "⏰ 响应超时" << std::endl;
    }
    
    std::cout << "\n✅ 错误案例测试完成" << std::endl;
}

// 打印进程状态
void print_process_status(const PipeCommunication& pipe_comm) {
    std::cout << "\n=== 进程状态检查 ===" << std::endl;
    std::cout << "🔄 管道通信运行中: " << (pipe_comm.is_running() ? "是" : "否") << std::endl;
    std::cout << "💓 子进程存活: " << (pipe_comm.is_process_alive() ? "是" : "否") << std::endl;
    std::cout << "🆔 子进程ID: " << pipe_comm.get_process_id() << std::endl;
    
    // 等待一段时间让所有消息处理完成
    std::cout << "⏳ 等待消息处理完成..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

// 打印统计信息
void print_statistics(PipeCommunication& pipe_comm) {
    std::cout << "\n=== 统计信息 ===" << std::endl;
    std::cout << "📊 通过回调接收的消息数: 0 (回调已禁用)" << std::endl;
    
    // 获取所有剩余消息
    std::vector<Message> remaining_messages = pipe_comm.get_all_messages();
    std::cout << "📨 剩余消息数: " << remaining_messages.size() << std::endl;
    
    if (!remaining_messages.empty()) {
        std::cout << "📋 剩余消息列表:" << std::endl;
        for (size_t i = 0; i < remaining_messages.size(); ++i) {
            std::cout << "  " << (i + 1) << ". " << remaining_messages[i].content << std::endl;
        }
    }
}

// 运行持续测试
void run_continuous_test(PipeCommunication& pipe_comm) {
    std::vector<std::string> test_messages = {
        create_initialize_message("cpp-mcp-client", "1.0.0"),
        create_ping_message(1),
        create_list_tools_message(2),
        create_list_allowed_directories_message(3),
        create_write_file_message("test_output.txt", "Hello from C++ MCP client!\nThis is a test file created by the write_file tool.", 4),
        create_create_directory_message("test_dir", 5),
        create_list_directory_message(".", 6),
        create_read_text_file_message("test_output.txt", 5, -1, 7),
        create_get_file_info_message("test_output.txt", 8),
    };
    
    while (true) {
        const auto& msg = test_messages[1];
        
        std::cout << "tick message " << msg << std::endl;
        
        auto response_future = pipe_comm.send_request(msg);
        std::cout << "✅ 消息发送成功" << std::endl;
        
        // 等待一下让子进程处理
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const auto timeout = std::chrono::seconds(1);
        auto status = response_future.wait_for(timeout);
    
        if (status == std::future_status::ready) {
            nlohmann::json response = response_future.get();
            std::cout << "Received response: " << response.dump() << std::endl;
        }
        
        // 等待一下让子进程处理
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}
