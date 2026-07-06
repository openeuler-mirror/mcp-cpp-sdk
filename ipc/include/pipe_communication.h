#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>
#include <functional>
#include <queue>
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "../../common/json.hpp"
#include "../../log/include/logger.h"

namespace mcp {

// 通信模式枚举
enum class CommunicationMode {
    CLIENT = 0,  // 客户端模式：启动子进程并与之通信
    SERVER = 1   // 服务端模式：作为子进程与父进程通信
};

// 管道通信配置
struct PipeConfig {
    CommunicationMode mode = CommunicationMode::CLIENT;
    
    // 客户端模式配置
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> env_vars;
    
    // 服务端模式配置
    std::string server_name = "mcp_server";
    std::string server_version = "1.0.0";
    
    // 通用配置
    int buffer_size = 4096;
    int read_timeout_ms = 10;
    int max_retry_count = 10;
    bool enable_logging = true;
    std::string log_file_path = ""; // 空字符串表示输出到控制台
};

// 消息类型
enum class MessageType {
    REQUEST,
    RESPONSE,
    NOTIFICATION
};

// 消息结构
struct Message {
    MessageType type;
    std::string content;
    std::string id; // 用于请求-响应匹配
};

// 消息处理器接口
class MessageHandler {
public:
    virtual ~MessageHandler() = default;
    virtual void onMessage(const Message& message) = 0;
    virtual void onError(const std::string& error) = 0;
};

// 服务端请求处理器接口
class ServerRequestHandler {
public:
    virtual ~ServerRequestHandler() = default;
    virtual std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) = 0;
    virtual void handleNotification(const std::string& method, const std::string& params) = 0;
};

// 管道通信核心类
class PipeCommunication {
public:
    PipeCommunication();
    ~PipeCommunication();

    // 启动和停止
    bool start(const PipeConfig& config);
    void stop();
    bool isRunning() const;

    // 消息发送
    bool sendMessage(const std::string& message);
    std::future<std::string> sendRequest(const std::string& request, int timeout_seconds = 60);

    // 消息处理
    void setMessageHandler(std::shared_ptr<MessageHandler> handler);
    
    // 服务端模式专用方法
    void setServerRequestHandler(std::shared_ptr<ServerRequestHandler> handler);
    bool sendResponse(const std::string& request_id, const std::string& result);
    bool sendError(const std::string& request_id, int error_code, const std::string& error_message);
    bool sendNotification(const std::string& method, const std::string& params);
    
    // 直接获取消息
    bool hasMessage() const;
    Message getMessage();
    std::vector<Message> getAllMessages();

    // 直接获取结果
    bool hasResult() const;
    std::string getResult();
    std::vector<std::string> getAllResults();

    // 进程管理
    int getProcessId() const;
    bool isProcessAlive() const;

private:
    // 平台相关实现
    bool createPipes();
    void closePipes();
    bool startProcess();
    void stopProcess();
    
    // 读取线程
    void readThreadFunc();
    void processMessage(const std::string& line);
    
    // 请求-响应管理
    std::string generateRequestId();
    void handleResponse(const std::string& id, const std::string& content);
    void handleError(const std::string& id, const std::string& error);
    
    // 服务端模式处理
    void handleServerRequest(const nlohmann::json& request);
    void handleServerNotification(const nlohmann::json& notification);
    bool initializeServerMode();

    // 平台相关实现
    bool createPipesImpl();
    void closePipesImpl();
    void closeStdinWriteEnd();  // 关闭stdin写入端，用于通知子进程EOF
    bool startProcessImpl();
    void stopProcessImpl();
    ssize_t readImpl(char* buffer, size_t size);
    ssize_t writeImpl(const char* buffer, size_t size);

private:
    PipeConfig config_;
    bool running_;
    std::unique_ptr<std::thread> read_thread_;
    std::shared_ptr<MessageHandler> message_handler_;
    
    // 进程相关
    int process_id_;
#ifdef _WIN32
    void* process_handle_; // 平台相关句柄
#endif
    
    // 管道相关
    int stdin_pipe_[2];
    int stdout_pipe_[2];
    
    // 请求-响应管理
    std::mutex response_mutex_;
    std::map<std::string, std::promise<std::string>> pending_requests_;
    std::atomic<int> request_id_counter_;
    
    // 消息队列
    mutable std::mutex message_mutex_;
    std::queue<Message> message_queue_;

    // 返回的结果队列
    mutable std::mutex result_mutex_;
    std::queue<std::string> result_queue_;
    
    // 服务端模式相关
    std::shared_ptr<ServerRequestHandler> server_request_handler_;
    std::atomic<bool> server_mode_;
    
    // 子进程输出处理
    void printSubprocessOutput(const std::string& output, const std::string& source = "subprocess");
};

} // namespace mcp
