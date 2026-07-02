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
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

// 包含依赖的头文件
#include "../ipc/include/pipe_communication.h"
#include "../net/netio.h"
#include "../common/json.hpp"
#include "../log/logger.h"

namespace mcp_transport {

// 日志宏定义
#define TRANSPORT_LOG_DEBUG(msg) mcp_logger::Logger::getInstance().debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_LOG_INFO(msg) mcp_logger::Logger::getInstance().info(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_LOG_WARNING(msg) mcp_logger::Logger::getInstance().warning(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_LOG_ERROR(msg) mcp_logger::Logger::getInstance().error(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_LOG_FATAL(msg) mcp_logger::Logger::getInstance().fatal(msg, __FILE__, __LINE__, __FUNCTION__)

// 模块化日志宏定义
#define TRANSPORT_MODULE_LOG_DEBUG(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_MODULE_LOG_INFO(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->info(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_MODULE_LOG_WARNING(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->warning(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_MODULE_LOG_ERROR(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->error(msg, __FILE__, __LINE__, __FUNCTION__)
#define TRANSPORT_MODULE_LOG_FATAL(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->fatal(msg, __FILE__, __LINE__, __FUNCTION__)

// 传输类型枚举
enum class TransportType {
    IPC = 0,    // 进程间通信 (PipeCommunication)
    SSE = 1,    // SSE 通信 (NetIO)
    AUTO = 2    // 自动选择
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
    nlohmann::json to_json() const;
    
    // 从JSON创建
    static TransportMessage from_json(const nlohmann::json& j);
    
    // 从IPC消息创建
    static TransportMessage from_ipc_message(const mcp::Message& msg);
    
    // 从NetIO消息创建
    static TransportMessage from_netio_message(const netio::Message& msg);
    
    // 转换为IPC消息
    mcp::Message to_ipc_message() const;
    
    // 转换为NetIO消息
    netio::Message to_netio_message() const;
};

// 传输配置结构
struct TransportConfig {
    TransportType type = TransportType::AUTO;
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
        mcp::LogLevel log_level = mcp::LogLevel::INFO;
    } ipc;
    
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
    } sse;
    
    // 通用配置
    bool enable_logging = true;
    std::string log_file_path = "";
    int log_level = 1; // 0=DEBUG, 1=INFO, 2=WARNING, 3=ERROR
    
    // 从JSON配置文件加载
    static TransportConfig from_json_file(const std::string& file_path);
    static TransportConfig from_json(const nlohmann::json& j);
    
    // 转换为JSON
    nlohmann::json to_json() const;
    
    // 验证配置
    bool validate() const;
};

// 连接信息结构
struct ConnectionInfo {
    std::string connection_id;
    std::string remote_address;
    int remote_port;
    std::string local_address;
    int local_port;
    TransportStatus status;
    TransportType type;
    std::chrono::system_clock::time_point connected_at;
    std::chrono::system_clock::time_point last_activity;
    
    // 转换为JSON
    nlohmann::json to_json() const;
};

// 事件回调类型
using MessageCallback = std::function<void(const TransportMessage& message)>;
using ConnectionCallback = std::function<void(const ConnectionInfo& info)>;
using ErrorCallback = std::function<void(const std::string& error)>;

// 统一的传输接口类
class Transport {
public:
    Transport();
    virtual ~Transport();

    // 连接管理
    bool start(const TransportConfig& config);
    bool stop();
    bool is_running() const;
    TransportStatus get_status() const;
    ConnectionInfo get_connection_info() const;

    // 消息发送
    std::future<TransportMessage> send_request(const TransportMessage& message);
    std::future<TransportMessage> send_request(const std::string& content, 
                                              const std::string& method = "POST",
                                              const std::string& path = "/",
                                              const std::map<std::string, std::string>& headers = {});
    
    // 同步发送
    TransportMessage send_request_sync(const TransportMessage& message);
    TransportMessage send_request_sync(const std::string& content, 
                                     const std::string& method = "POST",
                                     const std::string& path = "/",
                                     const std::map<std::string, std::string>& headers = {});

    // 消息接收
    bool has_message() const;
    TransportMessage get_message();
    std::vector<TransportMessage> get_all_messages();

    // 回调设置
    void set_message_callback(MessageCallback callback);
    void set_connection_callback(ConnectionCallback callback);
    void set_error_callback(ErrorCallback callback);

    // 配置管理
    void update_config(const TransportConfig& config);
    TransportConfig get_config() const;

    // 统计信息
    nlohmann::json get_statistics() const;

    // 健康检查
    bool health_check();

    // 日志功能已集成到统一的log模块中
    // 使用 TRANSPORT_LOG_* 宏进行日志记录
    
    // 保留旧的日志接口以保持向后兼容性
    void log(int level, const std::string& message);
    void set_log_level(int level);
    void set_log_file(const std::string& file_path);
    void enable_logging(bool enable);

protected:
    // 内部实现
    class Impl;    
private:
    // 内部实现
    //class Impl;
    std::unique_ptr<Impl> impl_;
};

// 工具函数
namespace utils {
    // 配置工具函数
    TransportConfig create_ipc_config(const std::string& command, 
                                    const std::vector<std::string>& args = {},
                                    const std::map<std::string, std::string>& env_vars = {});
    
    TransportConfig create_sse_config(const std::string& host, int port, 
                                     netio::Protocol protocol = netio::Protocol::HTTP);
    
    TransportConfig create_sse_secure_config(const std::string& host, int port);
    
    // 消息工具函数
    TransportMessage create_request(const std::string& content, 
                                  const std::string& method = "POST",
                                  const std::string& path = "/",
                                  const std::map<std::string, std::string>& headers = {});
    
    TransportMessage create_response(const std::string& content, 
                                   int status_code = 200,
                                   const std::map<std::string, std::string>& headers = {});
    
    TransportMessage create_error(const std::string& error_message, 
                                int status_code = 500,
                                const std::map<std::string, std::string>& headers = {});
    
    // 验证函数
    bool validate_config(const TransportConfig& config);
    bool validate_message(const TransportMessage& message);
    
    // JSON工具函数
    std::string json_to_string(const nlohmann::json& j);
    nlohmann::json string_to_json(const std::string& s);
}

// 全局错误处理
class TransportError : public std::exception {
public:
    explicit TransportError(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }
    
private:
    std::string message_;
};

// 前向声明
class TransportMessageHandler;

} // namespace mcp_transport
