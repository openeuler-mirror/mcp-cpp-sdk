#include "named_pipe.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <map>
#include <mutex>
#include <atomic>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

using namespace mcp;

// 示例消息处理器
class ExampleMessageHandler : public NamedPipeMessageHandler {
public:
    void onMessage(const NamedPipeMessage& message) override {
        mcp_logger::getModuleLogger("named_pipe_client")->info("Received notification: " + message.content);
    }
    
    void onError(const std::string& error) override {
        mcp_logger::getModuleLogger("named_pipe_client")->error("Error: " + error);
    }
    
    void onConnected() override {
        mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Connected to server");
    }
    
    void onDisconnected() override {
        mcp_logger::getModuleLogger("named_pipe_client")->info("❌ Disconnected from server");
    }
};

// 简单的请求-响应管理器
class RequestResponseManager {
public:
    std::string sendRequest(NamedPipe& pipe, const nlohmann::json& request, int timeout_seconds = 10) {
        std::string request_id = request["id"].get<std::string>();
        
        // 创建 promise 和 future
        auto promise = std::make_shared<std::promise<std::string>>();
        auto future = promise->get_future();
        
        // 存储 promise
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_[request_id] = promise;
        }
        
        // 发送请求
        if (!pipe.sendMessage(request.dump())) {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_.erase(request_id);
            return R"({"error": {"code": -1, "message": "Failed to send request"}})";
        }
        
        // 等待响应
        auto status = future.wait_for(std::chrono::seconds(timeout_seconds));
        if (status == std::future_status::timeout) {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_.erase(request_id);
            return R"({"error": {"code": -1, "message": "Request timeout"}})";
        }
        
        // 获取响应
        std::string response = future.get();
        
        // 清理
        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_requests_.erase(request_id);
        }
        
        return response;
    }
    
    void handleResponse(const std::string& response_json) {
        try {
            nlohmann::json response = nlohmann::json::parse(response_json);
            
            // 检查是否是响应消息（有 id 字段）
            if (response.contains("id") && response["id"].is_string()) {
                std::string id = response["id"].get<std::string>();
                
                std::lock_guard<std::mutex> lock(mutex_);
                auto it = pending_requests_.find(id);
                if (it != pending_requests_.end()) {
                    it->second->set_value(response_json);
                }
            }
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("named_pipe_client")->error("Failed to parse response: " + std::string(e.what()));
        }
    }
    
    void processMessages(NamedPipe& pipe) {
        while (pipe.hasMessage()) {
            NamedPipeMessage msg = pipe.getMessage();
            handleResponse(msg.content);
        }
    }

private:
    std::mutex mutex_;
    std::map<std::string, std::shared_ptr<std::promise<std::string>>> pending_requests_;
};

void testPingRequest(NamedPipe& named_pipe, RequestResponseManager& manager) {
    mcp_logger::getModuleLogger("named_pipe_client")->info("🔍 Testing ping request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "ping_1";
    request["method"] = "ping";
    request["params"] = nlohmann::json::object();
    
    manager.processMessages(named_pipe);
    std::string response = manager.sendRequest(named_pipe, request, 10);
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("📨 Ping response: " + response);
}

void testEchoRequest(NamedPipe& named_pipe, RequestResponseManager& manager) {
    mcp_logger::getModuleLogger("named_pipe_client")->info("🔍 Testing echo request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "echo_1";
    request["method"] = "echo";
    request["params"] = "Hello from client!";
    
    manager.processMessages(named_pipe);
    std::string response = manager.sendRequest(named_pipe, request, 10);
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("📨 Echo response: " + response);
}

void testAddRequest(NamedPipe& named_pipe, RequestResponseManager& manager) {
    mcp_logger::getModuleLogger("named_pipe_client")->info("🔍 Testing add request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "add_1";
    request["method"] = "add";
    
    nlohmann::json params;
    params["a"] = 42;
    params["b"] = 58;
    request["params"] = params;
    
    manager.processMessages(named_pipe);
    std::string response = manager.sendRequest(named_pipe, request, 10);
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("📨 Add response: " + response);
}

void testGetTimeRequest(NamedPipe& named_pipe, RequestResponseManager& manager) {
    mcp_logger::getModuleLogger("named_pipe_client")->info("🔍 Testing get_time request...");
    
    nlohmann::json request;
    request["jsonrpc"] = "2.0";
    request["id"] = "time_1";
    request["method"] = "get_time";
    request["params"] = nlohmann::json::object();
    
    manager.processMessages(named_pipe);
    std::string response = manager.sendRequest(named_pipe, request, 10);
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("📨 Time response: " + response);
}

void testNotification(NamedPipe& named_pipe) {
    mcp_logger::getModuleLogger("named_pipe_client")->info("🔍 Testing notification...");
    
    nlohmann::json notification;
    notification["jsonrpc"] = "2.0";
    notification["method"] = "log";
    
    nlohmann::json params;
    params["message"] = "This is a test log message from client";
    notification["params"] = params;
    
    bool success = named_pipe.sendMessage(notification.dump());
    if (success) {
        mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Notification sent successfully");
    } else {
        mcp_logger::getModuleLogger("named_pipe_client")->error("❌ Failed to send notification");
    }
}

void testBatchRequests(NamedPipe& named_pipe, RequestResponseManager& manager) {
    mcp_logger::getModuleLogger("named_pipe_client")->info("🔍 Testing batch requests...");
    
    std::vector<std::string> responses;
    
    // 发送多个并发请求
    for (int i = 0; i < 5; ++i) {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["id"] = "batch_" + std::to_string(i);
        request["method"] = "ping";
        request["params"] = nlohmann::json::object();
        
        manager.processMessages(named_pipe);
        std::string response = manager.sendRequest(named_pipe, request, 10);
        responses.push_back(response);
    }
    
    // 输出所有响应
    for (size_t i = 0; i < responses.size(); ++i) {
        mcp_logger::getModuleLogger("named_pipe_client")->info("📨 Batch response " + std::to_string(i) + ": " + responses[i]);
    }
}

// 服务端进程管理
static pid_t server_process_id = -1;

bool startServerProcess(const std::string& pipe_path) {
    std::string server_command = "./bin/named_pipe_server_test";
    
    // POSIX实现
    server_process_id = fork();
    
    if (server_process_id == -1) {
        mcp_logger::getModuleLogger("named_pipe_client")->error(
            "❌ Failed to fork server process: " + std::string(strerror(errno))
        );
        return false;
    }
    
    if (server_process_id == 0) {
        // 子进程 - 执行服务端程序
        std::vector<char*> args;
        args.push_back(const_cast<char*>(server_command.c_str()));
        if (!pipe_path.empty() && pipe_path != "./tmp/mcp_named_pipe") {
            args.push_back(const_cast<char*>(pipe_path.c_str()));
        }
        args.push_back(nullptr);
        
        execvp(args[0], args.data());
        // 如果execvp失败
        mcp_logger::getModuleLogger("named_pipe_client")->error(
            "❌ Failed to execute server: " + std::string(strerror(errno))
        );
        exit(EXIT_FAILURE);
    } else {
        // 父进程
        // 等待一小段时间，检查子进程是否成功启动
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 检查子进程是否还活着
        int status;
        pid_t result = waitpid(server_process_id, &status, WNOHANG);
        if (result == server_process_id) {
            // 子进程已经退出，说明启动失败
            mcp_logger::getModuleLogger("named_pipe_client")->error(
                "❌ Server process exited immediately, startup failed"
            );
            server_process_id = -1;
            return false;
        }
        
        mcp_logger::getModuleLogger("named_pipe_client")->info(
            "✅ Server process started, PID: " + std::to_string(server_process_id)
        );
        return true;
    }
}

void stopServerProcess() {
    if (server_process_id > 0) {
        mcp_logger::getModuleLogger("named_pipe_client")->info("🛑 Stopping server process...");
        kill(server_process_id, SIGTERM);
        
        // 等待进程退出
        int status;
        int wait_count = 0;
        while (waitpid(server_process_id, &status, WNOHANG) == 0 && wait_count < 10) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }
        
        // 如果还在运行，强制杀死
        if (waitpid(server_process_id, &status, WNOHANG) == 0) {
            kill(server_process_id, SIGKILL);
            waitpid(server_process_id, &status, 0);
        }
        
        server_process_id = -1;
        mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Server process stopped");
    }
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/named_pipe_client_test.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("🚀 Starting Named Pipe Client Test");
    
    // 获取管道路径（默认为 ./tmp/mcp_named_pipe）
    std::string pipe_path = "./tmp/mcp_named_pipe";
    if (argc > 1) {
        pipe_path = argv[1];
    }
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("Pipe path: " + pipe_path);
    
    // 启动服务端进程
    mcp_logger::getModuleLogger("named_pipe_client")->info("🚀 Starting server process...");
    if (!startServerProcess(pipe_path)) {
        mcp_logger::getModuleLogger("named_pipe_client")->error("❌ Failed to start server process");
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    // 等待服务端启动并创建管道
    mcp_logger::getModuleLogger("named_pipe_client")->info("⏳ Waiting for server to start...");
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 创建命名管道实例
    NamedPipe named_pipe;
    
    // 设置消息处理器
    auto handler = std::make_shared<ExampleMessageHandler>();
    named_pipe.setMessageHandler(handler);
    
    // 配置命名管道（客户端模式 - 双工模式）
    NamedPipeConfig config;
    config.pipe_path = pipe_path;
    config.mode = NamedPipeMode::DUPLEX;  // 使用双工模式以支持请求-响应
    config.buffer_size = 4096;
    config.read_timeout_ms = 1000;
    config.write_timeout_ms = 1000;
    config.auto_create = false;  // 客户端不创建管道，只连接
    
    // 启动命名管道
    if (!named_pipe.start(config)) {
        mcp_logger::getModuleLogger("named_pipe_client")->error("❌ Failed to start named pipe client");
        mcp_logger::getModuleLogger("named_pipe_client")->error("Make sure the server is running first");
        stopServerProcess();
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Named Pipe Client started successfully");
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    if (!named_pipe.isConnected()) {
        mcp_logger::getModuleLogger("named_pipe_client")->error("❌ Not connected to server");
        named_pipe.stop();
        stopServerProcess();
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    // 创建请求-响应管理器
    RequestResponseManager manager;
    
    // 启动响应处理线程
    std::atomic<bool> response_thread_running(true);
    std::thread response_thread([&]() {
        while (response_thread_running && named_pipe.isRunning()) {
            manager.processMessages(named_pipe);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    
    // 执行各种测试
    try {
        testPingRequest(named_pipe, manager);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testEchoRequest(named_pipe, manager);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testAddRequest(named_pipe, manager);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testGetTimeRequest(named_pipe, manager);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testNotification(named_pipe);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        testBatchRequests(named_pipe, manager);
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // 监听通知
        mcp_logger::getModuleLogger("named_pipe_client")->info("👂 Listening for notifications for 30 seconds...");
        auto start_time = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(30)) {
            manager.processMessages(named_pipe);
            if (named_pipe.hasMessage()) {
                NamedPipeMessage message = named_pipe.getMessage();
                // 检查是否是通知（没有 id 字段）
                try {
                    nlohmann::json msg_json = nlohmann::json::parse(message.content);
                    if (!msg_json.contains("id")) {
                        mcp_logger::getModuleLogger("named_pipe_client")->info("📨 Received notification: " + message.content);
                    } else {
                        // 是响应，交给管理器处理
                        manager.handleResponse(message.content);
                    }
                } catch (...) {
                    mcp_logger::getModuleLogger("named_pipe_client")->info("📨 Received: " + message.content);
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("named_pipe_client")->error("❌ Test error: " + std::string(e.what()));
    }
    
    // 停止响应处理线程
    response_thread_running = false;
    if (response_thread.joinable()) {
        response_thread.join();
    }
    
    // 停止命名管道
    mcp_logger::getModuleLogger("named_pipe_client")->info("🛑 Stopping named pipe client...");
    named_pipe.stop();
    mcp_logger::getModuleLogger("named_pipe_client")->info("✅ Named Pipe Client stopped");
    
    // 停止服务端进程
    stopServerProcess();
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}

