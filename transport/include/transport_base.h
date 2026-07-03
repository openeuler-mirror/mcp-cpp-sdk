#pragma once

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <future>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <thread>
#include "../../common/json.hpp"
#include "../../net/include/netio.h"
#include "../../ipc/include/pipe_communication.h"
#include "../../log/include/logger.h"

namespace mcp_transport {

// ========== 类型定义 ==========

// 传输类型枚举
enum class TransportType {
    IPC = 0,    // 进程间通信 (PipeCommunication)
    HTTP = 1,   // HTTP 通信 (NetIO)
    HTTPS = 2,  // HTTPS 通信 (NetIO)
    AUTO = 3    // 自动选择
};

// 通信模式枚举
enum class TransportMode {
    CLIENT = 0,  // 客户端模式：连接到远程服务
    SERVER = 1   // 服务端模式：作为服务提供者
};

// 传输状态枚举
enum class TransportStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};

// 统一的消息结构
struct TransportMessage {
    std::string id;
    std::string content;
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    int status_code = 200;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
    
    // 转换为JSON
    nlohmann::json toJson() const;
    
    // 从JSON创建
    static TransportMessage fromJson(const nlohmann::json& j);
    
    // 从IPC消息创建
    static TransportMessage fromIpcMessage(const mcp::Message& msg);
    
    // 从NetIO消息创建
    static TransportMessage fromNetioMessage(const netio::Message& msg);
    
    // 转换为IPC消息
    mcp::Message toIpcMessage() const;
    
    // 转换为NetIO消息
    netio::Message toNetioMessage() const;
};

// 传输配置结构
struct TransportConfig {
    TransportType type = TransportType::AUTO;
    TransportMode mode = TransportMode::CLIENT;
    std::string name = "mcp_transport";
    
    // IPC 配置
    struct {
        std::string command;
        std::vector<std::string> args;
        std::map<std::string, std::string> env_vars;
        int buffer_size = 4096;
        int read_timeout_ms = 10;
        int max_retry_count = 10;
        bool enable_logging = true;
        std::string log_file_path = "";
        mcp_logger::LogLevel log_level = mcp_logger::LogLevel::INFO;
    } ipc;
    
    // 服务端模式配置
    struct {
        std::string server_name = "mcp_transport_server";
        std::string server_version = "1.0.0";
        std::string description = "MCP Transport Server";
    } server;
    
    // SSE 配置
    struct {
        std::string host = "localhost";
        int port = 8080;
        netio::Protocol protocol = netio::Protocol::HTTP;
        int timeout_seconds = 30;
        int max_retries = 3;
        std::map<std::string, std::string> headers;
        std::string user_agent = "MCP-Transport/1.0";
        bool keep_alive = true;
        bool auto_reconnect = false;
        int reconnect_interval = 5;
        std::string endpoint = "/"; // HTTP endpoint path for server mode
    } sse;
    
    // HTTPS 配置（可选，仅当 type == HTTPS 时使用）
    struct {
        std::string cert_file = "";      // 证书文件路径（服务器必需）
        std::string key_file = "";       // 私钥文件路径（服务器必需）
        std::string ca_file = "";        // CA证书文件路径（可选，用于客户端验证）
        bool verify_peer = true;         // 是否验证对等方证书（客户端）
        bool verify_hostname = true;     // 是否验证主机名（客户端）
    } https;
    
    // 通用配置
    bool enable_logging = true;
    std::string log_file_path = "";
    int log_level = 1; // 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR
};

// 连接信息结构
struct ConnectionInfo {
    netio::ConnectionInfo netio_info;
    TransportStatus status = TransportStatus::DISCONNECTED;
    TransportType type = TransportType::AUTO;
    
    // 转换为JSON
    nlohmann::json toJson() const;
};

// 服务端请求处理器接口
class TransportServerRequestHandler {
public:
    virtual ~TransportServerRequestHandler() = default;
    virtual std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) = 0;
    virtual void handleNotification(const std::string& method, const std::string& params) = 0;
};

// 服务端连接处理器接口
class SseServerConnectHandler {
public:
    virtual ~SseServerConnectHandler() = default;
    virtual bool handleRequest(const netio::Message& request, netio::Message& response) = 0;
};

// 服务端请求处理器接口
class SseServerRequestHandler {
public:
    virtual ~SseServerRequestHandler() = default;
    virtual bool handleRequest(const netio::Message& request, netio::Message& response) = 0;
};

// 事件回调类型
using MessageCallback = std::function<void(const TransportMessage& message)>;
using ConnectionCallback = std::function<void(const ConnectionInfo& info)>;
using ErrorCallback = std::function<void(const std::string& error)>;

// ========== Transport基类 ==========

/**
 * @brief Transport基类，定义所有传输实现的通用接口
 * 
 * 所有具体的传输实现（StdioTransport, HttpTransport, HttpsTransport, TcpTransport）
 * 都应该继承自此类并实现纯虚函数
 */
class TransportBase {
public:
    TransportBase();
    virtual ~TransportBase();

    // ========== 连接管理 ==========
    /**
     * @brief 启动传输连接
     * @param config 传输配置
     * @return 成功返回true，失败返回false
     */
    virtual bool start(const TransportConfig& config) = 0;
    
    /**
     * @brief 停止传输连接
     * @return 成功返回true，失败返回false
     */
    virtual bool stop() = 0;
    
    /**
     * @brief 检查传输是否正在运行
     * @return 运行中返回true，否则返回false
     */
    bool isRunning() const;
    
    /**
     * @brief 获取当前传输状态
     * @return 传输状态
     */
    TransportStatus getStatus() const;
    
    /**
     * @brief 获取连接信息
     * @return 连接信息结构
     */
    virtual ConnectionInfo getConnectionInfo() const = 0;

    // ========== 消息发送 ==========
    /**
     * @brief 异步发送请求
     * @param message 要发送的消息
     * @return 返回future，包含响应消息
     */
    virtual std::future<TransportMessage> sendRequest(const TransportMessage& message) = 0;
    
    /**
     * @brief 异步发送请求（便捷方法）
     * @param content 消息内容
     * @param method HTTP方法（如"POST", "GET"等）
     * @param path 请求路径
     * @param headers HTTP头
     * @return 返回future，包含响应消息
     */
    std::future<TransportMessage> sendRequest(
        const std::string& content,
        const std::string& method = "POST",
        const std::string& path = "/",
        const std::map<std::string, std::string>& headers = {});
    
    /**
     * @brief 同步发送请求
     * @param message 要发送的消息
     * @return 响应消息
     */
    virtual TransportMessage sendRequestSync(const TransportMessage& message) = 0;
    
    /**
     * @brief 同步发送请求（便捷方法）
     * @param content 消息内容
     * @param method HTTP方法
     * @param path 请求路径
     * @param headers HTTP头
     * @return 响应消息
     */
    TransportMessage sendRequestSync(
        const std::string& content,
        const std::string& method = "POST",
        const std::string& path = "/",
        const std::map<std::string, std::string>& headers = {});

    // ========== 消息接收 ==========
    /**
     * @brief 检查是否有消息可读
     * @return 有消息返回true，否则返回false
     */
    bool hasMessage() const;
    
    /**
     * @brief 获取一条消息
     * @return 消息对象，如果没有消息则返回空消息
     */
    TransportMessage getMessage();
    
    /**
     * @brief 获取所有待处理的消息
     * @return 消息列表
     */
    std::vector<TransportMessage> getAllMessages();
    
    /**
     * @brief 将消息添加到队列（供子类使用）
     * @param message 要添加的消息
     * @return 成功返回true，队列满时返回false
     */
    bool enqueueMessage(const TransportMessage& message);
    
    /**
     * @brief 设置消息队列最大大小
     * @param max_size 最大队列大小，0表示无限制
     */
    void setMaxQueueSize(size_t max_size);
    
    /**
     * @brief 获取当前队列大小
     * @return 当前队列中的消息数量
     */
    size_t getQueueSize() const;

    // ========== 回调设置 ==========
    /**
     * @brief 设置消息回调
     * @param callback 消息回调函数
     */
    virtual void setMessageCallback(MessageCallback callback);
    
    /**
     * @brief 设置连接状态回调
     * @param callback 连接回调函数
     */
    void setConnectionCallback(ConnectionCallback callback);
    
    /**
     * @brief 设置错误回调
     * @param callback 错误回调函数
     */
    void setErrorCallback(ErrorCallback callback);

    // ========== 服务端模式专用方法 ==========
    /**
     * @brief 设置服务端请求处理器
     * @param handler 请求处理器
     */
    virtual void setServerRequestHandler(std::shared_ptr<TransportServerRequestHandler> handler) = 0;
    
    /**
     * @brief 设置SSE服务端请求处理器
     * @param handler SSE请求处理器
     */
    virtual void setSseServerRequestHandler(std::shared_ptr<SseServerRequestHandler> handler);
    
    /**
     * @brief 设置连接请求处理器
     * @param handler 连接处理器
     */
    virtual void setConnectionRequestHandler(std::shared_ptr<SseServerConnectHandler> handler);
    
    /**
     * @brief 发送响应
     * @param request_id 请求ID
     * @param result 响应结果
     * @return 成功返回true，失败返回false
     */
    bool sendResponse(const std::string& request_id, const std::string& result);
    
    /**
     * @brief 发送错误响应
     * @param request_id 请求ID
     * @param error_code 错误码
     * @param error_message 错误消息
     * @return 成功返回true，失败返回false
     */
    bool sendError(const std::string& request_id, int error_code, const std::string& error_message);
    
    /**
     * @brief 发送通知
     * @param method 方法名
     * @param params 参数
     * @return 成功返回true，失败返回false
     */
    bool sendNotification(const std::string& method, const std::string& params);
    
    /**
     * @brief 实际发送JSON-RPC消息（供子类覆盖以实现具体的发送逻辑）
     * @param json_message JSON格式的消息字符串
     * @return 成功返回true，失败返回false
     */
    virtual bool sendJsonRpcMessage(const std::string& json_message) = 0;

    // ========== 配置管理 ==========
    /**
     * @brief 更新配置
     * @param config 新配置
     */
    void updateConfig(const TransportConfig& config);
    
    /**
     * @brief 获取当前配置
     * @return 当前配置
     */
    TransportConfig getConfig() const;

    // ========== 统计信息 ==========
    /**
     * @brief 获取统计信息
     * @return JSON格式的统计信息
     * @note 此方法使用锁保护，确保统计信息的一致性
     */
    nlohmann::json getStatistics() const;
    
    /**
     * @brief 获取传输类型名称（子类必须实现）
     * @return 传输类型名称，如 "stdio", "http", "https", "tcp"
     */
    virtual std::string getTransportTypeName() const = 0;
    
    /**
     * @brief 获取扩展统计信息（供子类覆盖以添加特定统计）
     * @return JSON格式的扩展统计信息，默认为空对象
     */
    virtual nlohmann::json getExtendedStatistics() const;

    // ========== 健康检查 ==========
    /**
     * @brief 健康检查
     * @return 健康返回true，否则返回false
     */
    bool healthCheck();
    
    /**
     * @brief 执行具体的健康检查（供子类覆盖）
     * @return 健康返回true，否则返回false
     */
    virtual bool doHealthCheck() const;

protected:
    // ========== 基于NetIO的通用功能（供使用NetIO的子类使用） ==========
    /**
     * @brief 获取NetIO协议类型（供使用NetIO的子类实现）
     * @return 协议类型（HTTP或HTTPS）
     */
    virtual netio::Protocol getNetioProtocol() const;
    
    /**
     * @brief 初始化NetIO实例（供使用NetIO的子类调用）
     * @return 成功返回true，失败返回false
     */
    bool initNetIO();
    
    /**
     * @brief 清理NetIO实例（供使用NetIO的子类调用）
     */
    void cleanupNetIO();
    
    /**
     * @brief 启动NetIO客户端（供使用NetIO的子类调用）
     * @return 成功返回true，失败返回false
     */
    bool startNetIOClient();
    
    /**
     * @brief 启动NetIO服务端（供使用NetIO的子类调用）
     * @return 成功返回true，失败返回false
     */
    bool startNetIOServer();
    
    /**
     * @brief 停止NetIO连接（供使用NetIO的子类调用）
     */
    void stopNetIO();
    
    /**
     * @brief 将TransportMessage转换为NetIO Message（可被子类覆盖）
     * @param msg Transport消息
     * @return NetIO消息
     */
    virtual netio::Message toNetioMessage(const TransportMessage& msg);
    
    /**
     * @brief 将NetIO Message转换为TransportMessage（可被子类覆盖）
     * @param msg NetIO消息
     * @return Transport消息
     */
    virtual TransportMessage fromNetioMessage(const netio::Message& msg);
    
    /**
     * @brief 处理服务端请求（供使用NetIO的子类调用）
     * @param request 请求消息
     */
    void handleNetIOServerRequest(const netio::Message& request);
    
    /**
     * @brief 启动接收线程（供使用NetIO的子类调用）
     */
    void startNetIOReceiveThread();
    
    /**
     * @brief 停止接收线程（供使用NetIO的子类调用）
     */
    void stopNetIOReceiveThread();
    
    /**
     * @brief 接收线程函数（供使用NetIO的子类使用）
     */
    void netIOReceiveThreadFunc();
    
    /**
     * @brief 配置NetIO（供使用NetIO的子类覆盖以添加特定配置）
     * @param net_config NetIO配置（将被修改）
     */
    virtual void configureNetIO(netio::NetConfig& net_config);

protected:
    // 受保护的辅助方法，供子类使用
    /**
     * @brief 获取日志记录器
     * @return 日志记录器
     */
    std::shared_ptr<mcp_logger::Logger::ModuleLogger> getLogger() const;
    
    /**
     * @brief 更新状态
     * @param status 新状态
     */
    void setStatus(TransportStatus status);
    
    /**
     * @brief 触发消息回调
     * @param message 消息
     */
    void triggerMessageCallback(const TransportMessage& message);
    
    /**
     * @brief 触发连接回调
     * @param info 连接信息
     */
    void triggerConnectionCallback(const ConnectionInfo& info);
    
    /**
     * @brief 触发错误回调
     * @param error 错误消息
     */
    void triggerErrorCallback(const std::string& error);

    // 受保护的成员变量
    TransportConfig config_;
    std::atomic<TransportStatus> status_;
    std::atomic<bool> running_;
    
    // 回调函数
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;
    
    // 消息队列（供子类使用）
    std::queue<TransportMessage> message_queue_;
    mutable std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    size_t max_queue_size_ = 1000;  // 队列最大大小，防止内存问题
    
    // 统计信息（供子类使用）
    std::atomic<uint64_t> messages_sent_;
    std::atomic<uint64_t> messages_received_;
    std::atomic<uint64_t> bytes_sent_;
    std::atomic<uint64_t> bytes_received_;
    
    // 线程安全
    mutable std::mutex mutex_;
    
    // NetIO相关成员（供使用NetIO的子类使用）
    std::unique_ptr<netio::NetIO> netio_;
    std::thread receive_thread_;
    std::shared_ptr<SseServerRequestHandler> sse_server_handler_;
    std::shared_ptr<SseServerConnectHandler> connection_handler_;

private:
    // Transport模块日志记录器
    std::shared_ptr<mcp_logger::Logger::ModuleLogger> module_logger_;
};

} // namespace mcp_transport

