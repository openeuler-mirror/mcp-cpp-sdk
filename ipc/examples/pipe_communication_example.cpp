#include "pipe_communication.h"
#include "../../mcp_protocol/include/mcp_protocol_messages.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <ctime>     // for std::time
#include <unistd.h>  // for getpid()

using namespace mcp;

// 使用全局函数进行日志记录

// 示例消息处理器
class ExampleMessageHandler : public MessageHandler {
public:
    int message_count = 0;
    
    void onMessage(const Message& message) override {
        message_count++;
        mcp_logger::getModuleLogger("ipc_example")->info("[Message #" + std::to_string(message_count) + "] Received: " + message.content);
    }
    
    void onError(const std::string& error) override {
        mcp_logger::getModuleLogger("ipc_example")->error("[ERROR] " + error);
    }
};

// 函数声明
bool setupPipeCommunication(PipeCommunication& pipe_comm);
void printCommunicationInfo(const PipeCommunication& pipe_comm, const PipeConfig& config);
bool checkServerStatus(const PipeCommunication& pipe_comm);
void testImmediateToolCall(PipeCommunication& pipe_comm);
void testBatchMessages(PipeCommunication& pipe_comm);
void testRequestResponseMode(PipeCommunication& pipe_comm);
void testListDirectoriesTool(PipeCommunication& pipe_comm);
void testWriteFileTool(PipeCommunication& pipe_comm);
void testEditFileTool(PipeCommunication& pipe_comm);
void testSearchFilesTool(PipeCommunication& pipe_comm);
void testErrorCases(PipeCommunication& pipe_comm);
void printProcessStatus(const PipeCommunication& pipe_comm);
void printStatistics(PipeCommunication& pipe_comm);
void runContinuousTest(PipeCommunication& pipe_comm);

int main() {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    
    // 同时输出到文件和控制台
    log_config.target = mcp_logger::LogTarget::BOTH;
    
    // 设置日志文件路径
    log_config.file_path = "logs/ipc_example.log";
    
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;  // 自动刷新，确保日志立即写入文件
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    // 使用全局函数进行日志记录
    
    // 创建管道通信实例
    PipeCommunication pipe_comm;
    
    // 设置管道通信
    if (!setupPipeCommunication(pipe_comm)) {
        return 1;
    }
    
    // 检查服务器状态
    if (!checkServerStatus(pipe_comm)) {
        return 1;
    }
    
    // 执行各种测试
    testImmediateToolCall(pipe_comm);
    testBatchMessages(pipe_comm);
    testRequestResponseMode(pipe_comm);
    testErrorCases(pipe_comm);
    
    // 打印状态和统计信息
    printProcessStatus(pipe_comm);
    printStatistics(pipe_comm);
    
    // 运行持续测试（可选） - 已禁用，避免程序无限运行
    runContinuousTest(pipe_comm);
    
    // 停止管道通信
    mcp_logger::getModuleLogger("ipc_example")->info("🛑 停止管道通信...");
    pipe_comm.stop();
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 管道通信已停止");
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}

// 设置管道通信
bool setupPipeCommunication(PipeCommunication& pipe_comm) {
    // 设置消息处理器（暂时注释掉，直接使用同步方式）
    // auto handler = std::make_shared<ExampleMessageHandler>();
    // pipe_comm.setMessageHandler(handler);
    
    // 配置管道通信 - 使用MCP服务器
    PipeConfig config;
    config.command = "npx";
    // 使用测试专用目录，避免与build目录冲突
    config.args = {"-y", "@modelcontextprotocol/server-filesystem", "/Users/apple/wangjiandong/go_mcp/cpp_mcp_sdk_new/cpp_mcp_sdk/ipc/build"};
    config.env_vars["NODE_ENV"] = "development";
    config.buffer_size = 2048;
    config.read_timeout_ms = 100;  // 增加超时时间
    
    // 启动管道通信
    if (!pipe_comm.start(config)) {
        mcp_logger::getModuleLogger("ipc_example")->error("Failed to start pipe communication");
        mcp_logger::getModuleLogger("ipc_example")->error("请确保已安装 Node.js 和 npm，并且可以运行 npx 命令");
        mcp_logger::getModuleLogger("ipc_example")->error("可以尝试运行: npm install -g @modelcontextprotocol/server-filesystem");
        return false;
    }
    
    printCommunicationInfo(pipe_comm, config);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    return true;
}

// 打印通信信息
void printCommunicationInfo(const PipeCommunication& pipe_comm, const PipeConfig& config) {
    mcp_logger::getModuleLogger("ipc_example")->info("=== 管道通信启动成功 ===");
    mcp_logger::getModuleLogger("ipc_example")->info("主进程ID: " + std::to_string(getpid()));
    mcp_logger::getModuleLogger("ipc_example")->info("子进程ID: " + std::to_string(pipe_comm.getProcessId()));
    
    std::string cmd = "命令: " + config.command;
    for (const auto& arg : config.args) {
        cmd += " " + arg;
    }
    mcp_logger::getModuleLogger("ipc_example")->info(cmd);
    
    mcp_logger::getModuleLogger("ipc_example")->info("缓冲区大小: " + std::to_string(config.buffer_size) + " bytes");
    mcp_logger::getModuleLogger("ipc_example")->info("读取超时: " + std::to_string(config.read_timeout_ms) + " ms");
    
    std::string env_vars = "环境变量: ";
    for (const auto& env : config.env_vars) {
        env_vars += env.first + "=" + env.second + " ";
    }
    mcp_logger::getModuleLogger("ipc_example")->info(env_vars);
}

// 检查服务器状态
bool checkServerStatus(const PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("=== 检查MCP服务器状态 ===");
    mcp_logger::getModuleLogger("ipc_example")->info("🔄 管道通信运行中: " + std::string(pipe_comm.isRunning() ? "是" : "否"));
    mcp_logger::getModuleLogger("ipc_example")->info("💓 子进程存活: " + std::string(pipe_comm.isProcessAlive() ? "是" : "否"));
    
    if (!pipe_comm.isRunning() || !pipe_comm.isProcessAlive()) {
        mcp_logger::getModuleLogger("ipc_example")->error("❌ MCP服务器没有正常运行，无法进行工具调用测试");
        mcp_logger::getModuleLogger("ipc_example")->error("请检查是否安装了 @modelcontextprotocol/server-filesystem 包");
        mcp_logger::getModuleLogger("ipc_example")->error("可以运行: npm install -g @modelcontextprotocol/server-filesystem");
        return false;
    }
    return true;
}

// 测试立即工具调用
void testImmediateToolCall(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("=== 立即测试工具调用 ===");
    std::string test_request = createListAllowedDirectoriesMessage(999);
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送测试请求: " + test_request);
    
    auto response_future = pipe_comm.sendRequest(test_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 测试请求发送成功");
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
    }
}

// 测试批量消息
void testBatchMessages(PipeCommunication& pipe_comm) {
    std::vector<std::string> test_messages = {
        createInitializeMessage("cpp-mcp-client", "1.0.0"),
        createPingMessage(1),
        createListToolsMessage(2),
        createListAllowedDirectoriesMessage(3),
        createWriteFileMessage("test_output.txt", "Hello from C++ MCP client!\nThis is a test file created by the write_file tool.", 4),
        createCreateDirectoryMessage("test_dir", 5),
        createListDirectoryMessage(".", 6),
        createReadTextFileMessage("test_output.txt", 5, -1, 7),  // 读取前5行
        createGetFileInfoMessage("test_output.txt", 8),
    };
    
    for (size_t i = 0; i < test_messages.size(); ++i) {
        const auto& msg = test_messages[i];
        mcp_logger::getModuleLogger("ipc_example")->info("--- 消息 " + std::to_string(i + 1) + "/" + std::to_string(test_messages.size()) + " ---");
        mcp_logger::getModuleLogger("ipc_example")->info("📤 发送消息: " + msg);
        
        auto response_future = pipe_comm.sendRequest(msg);
        mcp_logger::getModuleLogger("ipc_example")->info("✅ 消息发送成功");
        
        // 等待一下让子进程处理
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const auto timeout = std::chrono::seconds(1);
        auto status = response_future.wait_for(timeout);
    
        if (status == std::future_status::ready) {
            nlohmann::json response = response_future.get();
            mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
        }
    }
}

// 测试请求-响应模式
void testRequestResponseMode(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("=== 请求-响应模式测试 ===");
    
    testListDirectoriesTool(pipe_comm);
    testWriteFileTool(pipe_comm);
    testEditFileTool(pipe_comm);
    testSearchFilesTool(pipe_comm);
}

// 测试list_allowed_directories工具调用
void testListDirectoriesTool(PipeCommunication& pipe_comm) {
    std::string directories_request = createListAllowedDirectoriesMessage(100);
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送list_allowed_directories工具调用: " + directories_request);
    
    auto response_future = pipe_comm.sendRequest(directories_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ list_allowed_directories请求发送成功");
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
    }
}

// 测试write_file工具调用
void testWriteFileTool(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试write_file工具调用 ---");
    std::string write_file_request = createWriteFileMessage("response_test.txt", 
        "This file was created by the write_file tool call from C++ MCP client.\n"
        "Timestamp: " + std::to_string(std::time(nullptr)) + "\n"
        "This demonstrates the write_file functionality." + "\n" + "Hello from C++ MCP client! This is a test file", 101);
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送write_file工具调用: " + write_file_request);
    
    auto response_future = pipe_comm.sendRequest(write_file_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ write_file请求发送成功");
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
    }
}

// 测试edit_file工具调用
void testEditFileTool(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试edit_file工具调用 ---");
    std::vector<std::pair<std::string, std::string>> edits = {
        {"Hello from C++ MCP client!", "Hello from C++ MCP client! [EDITED]"},
        {"This is a test file", "This is a test file [MODIFIED]"}
    };
    std::string edit_file_request = createEditFileMessage("response_test.txt", edits, false, 102);
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送edit_file工具调用: " + edit_file_request);
    
    auto response_future = pipe_comm.sendRequest(edit_file_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ edit_file请求发送成功");
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
    }
}

// 测试search_files工具调用
void testSearchFilesTool(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试search_files工具调用 ---");
    std::string search_files_request = createSearchFilesMessage(".", "response_test.txt", {}, 103);
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送search_files工具调用: " + search_files_request);
   
    auto response_future = pipe_comm.sendRequest(search_files_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ search_files请求发送成功");
    
    // 等待一下让子进程处理
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status = response_future.wait_for(timeout);
    
    if (status == std::future_status::ready) {
        nlohmann::json response = response_future.get();
        mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
    }
}

// 测试错误案例
void testErrorCases(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("=== 错误案例测试 ===");
    
    // 测试1: 无效的JSON消息
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试1: 无效的JSON消息 ---");
    std::string invalid_json = "{\"invalid\": json, \"missing\": \"quote}";
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送无效JSON: " + invalid_json);
    
    auto response_future1 = pipe_comm.sendRequest(invalid_json);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 无效JSON发送成功");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto timeout = std::chrono::seconds(1);
    auto status1 = response_future1.wait_for(timeout);
    
    if (status1 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future1.get();
            mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("ipc_example")->warning("❌ 捕获到异常: " + std::string(e.what()));
        }
    } else {
        mcp_logger::getModuleLogger("ipc_example")->warning("⏰ 响应超时");
    }
    
    // 测试2: 不存在的工具调用
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试2: 不存在的工具调用 ---");
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
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送不存在的工具调用: " + non_existent_tool);
    
    auto response_future2 = pipe_comm.sendRequest(non_existent_tool);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 不存在工具调用发送成功");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status2 = response_future2.wait_for(timeout);
    
    if (status2 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future2.get();
            mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("ipc_example")->warning("❌ 捕获到异常: " + std::string(e.what()));
        }
    } else {
        mcp_logger::getModuleLogger("ipc_example")->warning("⏰ 响应超时");
    }
    
    // 测试3: 错误的文件路径
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试3: 错误的文件路径 ---");
    std::string invalid_path_request = createReadTextFileMessage("/nonexistent/path/file.txt", 5, -1, 888);
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送错误路径请求: " + invalid_path_request);
    
    auto response_future3 = pipe_comm.sendRequest(invalid_path_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 错误路径请求发送成功");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status3 = response_future3.wait_for(timeout);
    
    if (status3 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future3.get();
            mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("ipc_example")->warning("❌ 捕获到异常: " + std::string(e.what()));
        }
    } else {
        mcp_logger::getModuleLogger("ipc_example")->warning("⏰ 响应超时");
    }
    
    // 测试4: 空的请求
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试4: 空请求 ---");
    std::string empty_request = "";
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送空请求");
    
    auto response_future4 = pipe_comm.sendRequest(empty_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 空请求发送成功");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status4 = response_future4.wait_for(timeout);
    
    if (status4 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future4.get();
            mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("ipc_example")->warning("❌ 捕获到异常: " + std::string(e.what()));
        }
    } else {
        mcp_logger::getModuleLogger("ipc_example")->warning("⏰ 响应超时");
    }
    
    // 测试5: 格式正确但参数错误的请求
    mcp_logger::getModuleLogger("ipc_example")->info("--- 测试5: 参数错误的请求 ---");
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
    mcp_logger::getModuleLogger("ipc_example")->info("📤 发送参数错误请求: " + wrong_params_request);
    
    auto response_future5 = pipe_comm.sendRequest(wrong_params_request);
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 参数错误请求发送成功");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    auto status5 = response_future5.wait_for(timeout);
    
    if (status5 == std::future_status::ready) {
        try {
            nlohmann::json response = response_future5.get();
            mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("ipc_example")->warning("❌ 捕获到异常: " + std::string(e.what()));
        }
    } else {
        mcp_logger::getModuleLogger("ipc_example")->warning("⏰ 响应超时");
    }
    
    mcp_logger::getModuleLogger("ipc_example")->info("✅ 错误案例测试完成");
}

// 打印进程状态
void printProcessStatus(const PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("=== 进程状态检查 ===");
    mcp_logger::getModuleLogger("ipc_example")->info("🔄 管道通信运行中: " + std::string(pipe_comm.isRunning() ? "是" : "否"));
    mcp_logger::getModuleLogger("ipc_example")->info("💓 子进程存活: " + std::string(pipe_comm.isProcessAlive() ? "是" : "否"));
    mcp_logger::getModuleLogger("ipc_example")->info("🆔 子进程ID: " + std::to_string(pipe_comm.getProcessId()));
    
    // 等待一段时间让所有消息处理完成
    mcp_logger::getModuleLogger("ipc_example")->info("⏳ 等待消息处理完成...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
}

// 打印统计信息
void printStatistics(PipeCommunication& pipe_comm) {
    mcp_logger::getModuleLogger("ipc_example")->info("=== 统计信息 ===");
    mcp_logger::getModuleLogger("ipc_example")->info("📊 通过回调接收的消息数: 0 (回调已禁用)");
    
    // 获取所有剩余消息
    std::vector<Message> remaining_messages = pipe_comm.getAllMessages();
    mcp_logger::getModuleLogger("ipc_example")->info("📨 剩余消息数: " + std::to_string(remaining_messages.size()));
    
    if (!remaining_messages.empty()) {
        mcp_logger::getModuleLogger("ipc_example")->info("📋 剩余消息列表:");
        for (size_t i = 0; i < remaining_messages.size(); ++i) {
            mcp_logger::getModuleLogger("ipc_example")->info("  " + std::to_string(i + 1) + ". " + remaining_messages[i].content);
        }
    }
}

// 运行持续测试
void runContinuousTest(PipeCommunication& pipe_comm) {
    std::vector<std::string> test_messages = {
        createInitializeMessage("cpp-mcp-client", "1.0.0"),
        createPingMessage(1),
        createListToolsMessage(2),
        createListAllowedDirectoriesMessage(3),
        createWriteFileMessage("test_output.txt", "Hello from C++ MCP client!\nThis is a test file created by the write_file tool.", 4),
        createCreateDirectoryMessage("test_dir", 5),
        createListDirectoryMessage(".", 6),
        createReadTextFileMessage("test_output.txt", 5, -1, 7),
        createGetFileInfoMessage("test_output.txt", 8),
    };
    
    while (true) {
        const auto& msg = test_messages[1];
        
        mcp_logger::getModuleLogger("ipc_example")->info("tick message " + msg);
        
        auto response_future = pipe_comm.sendRequest(msg);
        mcp_logger::getModuleLogger("ipc_example")->info("✅ 消息发送成功");
        
        // 等待一下让子进程处理
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        const auto timeout = std::chrono::seconds(1);
        auto status = response_future.wait_for(timeout);
    
        if (status == std::future_status::ready) {
            nlohmann::json response = response_future.get();
            mcp_logger::getModuleLogger("ipc_example")->info("Received response: " + response.dump());
        }
        
        // 等待一下让子进程处理
        std::this_thread::sleep_for(std::chrono::seconds(60));
    }
}
