#pragma once

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <queue>
#include <functional>
#include "../../common/json.hpp"
#include "../../log/include/logger.h"

namespace mcp {

// 命名管道模式
enum class NamedPipeMode {
    READ = 0,   // 只读模式
    WRITE = 1,  // 只写模式
    DUPLEX = 2  // 双工模式
};

// 命名管道配置
struct NamedPipeConfig {
    std::string pipe_path;           // 命名管道路径
    NamedPipeMode mode = NamedPipeMode::READ;
    int buffer_size = 4096;          // 缓冲区大小
    int read_timeout_ms = 1000;      // 读取超时（毫秒）
    int write_timeout_ms = 1000;     // 写入超时（毫秒）
    bool non_blocking = false;       // 非阻塞模式
    bool auto_create = true;         // 自动创建命名管道
};

// 消息结构
struct NamedPipeMessage {
    std::string content;
    std::string id;  // 可选的消息ID
};

// 消息处理器接口
class NamedPipeMessageHandler {
public:
    virtual ~NamedPipeMessageHandler() = default;
    virtual void onMessage(const NamedPipeMessage& message) = 0;
    virtual void onError(const std::string& error) = 0;
    virtual void onConnected() = 0;
    virtual void onDisconnected() = 0;
};

// 命名管道类
class NamedPipe {
public:
    NamedPipe();
    ~NamedPipe();

    // 启动和停止
    bool start(const NamedPipeConfig& config);
    void stop();
    bool isRunning() const;
    bool isConnected() const;

    // 消息发送
    bool sendMessage(const std::string& message);
    bool sendMessage(const NamedPipeMessage& message);

    // 消息接收
    bool hasMessage() const;
    NamedPipeMessage getMessage();
    std::vector<NamedPipeMessage> getAllMessages();

    // 消息处理
    void setMessageHandler(std::shared_ptr<NamedPipeMessageHandler> handler);

    // 获取配置信息
    std::string getPipePath() const;
    NamedPipeMode getMode() const;

private:
    // 内部实现方法
    bool createNamedPipe();
    void closeNamedPipe();
    bool openNamedPipe();
    void closeConnection();
    
    // 读取线程
    void readThreadFunc();
    void processMessage(const std::string& line);
    
    // POSIX实现方法
    bool createNamedPipeImpl();
    void closeNamedPipeImpl();
    bool openNamedPipeImpl();
    void closeConnectionImpl();
    ssize_t readImpl(char* buffer, size_t size);
    ssize_t writeImpl(const char* buffer, size_t size);
    bool waitForConnection();

private:
    NamedPipeConfig config_;
    bool running_;
    bool connected_;
    std::unique_ptr<std::thread> read_thread_;
    std::shared_ptr<NamedPipeMessageHandler> message_handler_;
    
    // 管道文件描述符
    int pipe_fd_;
    
    // 消息队列
    mutable std::mutex message_mutex_;
    std::queue<NamedPipeMessage> message_queue_;
    
    // 写入互斥锁
    mutable std::mutex write_mutex_;
};

} // namespace mcp

