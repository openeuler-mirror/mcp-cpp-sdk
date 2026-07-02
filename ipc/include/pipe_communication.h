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
#include "../common/json.hpp"

namespace mcp {

// 日志级别
enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

// 管道通信配置
struct PipeConfig {
    std::string command;
    std::vector<std::string> args;
    std::map<std::string, std::string> env_vars;
    int buffer_size = 4096;
    int read_timeout_ms = 10;
    int max_retry_count = 10;
    bool enable_logging = true;
    std::string log_file_path = ""; // 空字符串表示输出到控制台
    LogLevel log_level = LogLevel::INFO;
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
    virtual void on_message(const Message& message) = 0;
    virtual void on_error(const std::string& error) = 0;
};

// 管道通信核心类
class PipeCommunication {
public:
    PipeCommunication();
    ~PipeCommunication();

    // 启动和停止
    bool start(const PipeConfig& config);
    void stop();
    bool is_running() const;

    // 消息发送
    bool send_message(const std::string& message);
    std::future<std::string> send_request(const std::string& request, int timeout_seconds = 60);

    // 消息处理
    void set_message_handler(std::shared_ptr<MessageHandler> handler);
    
    // 直接获取消息
    bool has_message() const;
    Message get_message();
    std::vector<Message> get_all_messages();

    // 直接获取结果
    bool has_result() const;
    std::string get_result();
    std::vector<std::string> get_all_results();

    // 进程管理
    int get_process_id() const;
    bool is_process_alive() const;
    
    // 日志功能
    void log(LogLevel level, const std::string& message);
    void set_log_level(LogLevel level);
    void set_log_file(const std::string& file_path);
    void enable_logging(bool enable);

private:
    // 平台相关实现
    bool create_pipes();
    void close_pipes();
    bool start_process();
    void stop_process();
    
    // 读取线程
    void read_thread_func();
    void process_message(const std::string& line);
    
    // 请求-响应管理
    std::string generate_request_id();
    void handle_response(const std::string& id, const std::string& content);
    void handle_error(const std::string& id, const std::string& error);

    // 平台相关实现
    bool create_pipes_impl();
    void close_pipes_impl();
    bool start_process_impl();
    void stop_process_impl();
    ssize_t read_impl(char* buffer, size_t size);
    ssize_t write_impl(const char* buffer, size_t size);

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
    
    // 日志相关
    mutable std::mutex log_mutex_;
    std::ofstream log_file_;
    LogLevel current_log_level_;
    bool logging_enabled_;
    std::string log_file_path_;
    
    // 子进程输出处理
    void log_subprocess_output(const std::string& output, const std::string& source = "subprocess");
    std::string get_timestamp() const;
    std::string log_level_to_string(LogLevel level) const;
};

} // namespace mcp
