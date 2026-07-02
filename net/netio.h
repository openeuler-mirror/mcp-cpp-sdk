#ifndef NETIO_H
#define NETIO_H

#include "../common/httplib.h"
#include "../common/json.hpp"
#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <future>
#include <mutex>
#include <thread>
#include <atomic>

namespace netio {

// 连接状态枚举
enum class ConnectionStatus {
    DISCONNECTED = 0,
    CONNECTING = 1,
    CONNECTED = 2,
    ERROR = 3
};

// 消息类型枚举
enum class MessageType {
    REQUEST = 0,
    RESPONSE = 1,
    NOTIFICATION = 2,
    ERROR = 3
};

// 协议类型
enum class Protocol {
    HTTP = 0,
    HTTPS = 1,
    WEBSOCKET = 2
};

// 网络配置结构
struct NetConfig {
    std::string host = "localhost";
    int port = 8080;
    Protocol protocol = Protocol::HTTP;
    int timeout_seconds = 30;
    int max_retries = 3;
    std::map<std::string, std::string> headers;
    std::string user_agent = "NetIO/1.0";
    bool keep_alive = true;
    bool auto_reconnect = false;
    int reconnect_interval = 5; // seconds
    
    // 转换为JSON
    nlohmann::json to_json() const;
};

// 消息结构
struct Message {
    std::string id;
    MessageType type;
    std::string method;
    std::string path;
    std::string body;
    std::map<std::string, std::string> headers;
    int status_code = 200;
    std::string error_message;
    std::chrono::system_clock::time_point timestamp;
    
    // 转换为JSON
    nlohmann::json to_json() const;
    
    // 从JSON创建
    static Message from_json(const nlohmann::json& j);
};

// 连接信息结构
struct ConnectionInfo {
    std::string connection_id;
    std::string remote_address;
    int remote_port;
    std::string local_address;
    int local_port;
    ConnectionStatus status;
    std::chrono::system_clock::time_point connected_at;
    std::chrono::system_clock::time_point last_activity;
    
    // 转换为JSON
    nlohmann::json to_json() const;
};

// 事件回调类型
using MessageCallback = std::function<void(const Message& message)>;
using ConnectionCallback = std::function<void(const ConnectionInfo& info)>;
using ErrorCallback = std::function<void(const std::string& error)>;

// 网络IO接口类
class NetIO {
public:
    NetIO();
    virtual ~NetIO();

    // 连接管理
    bool connect(const NetConfig& config);
    bool disconnect();
    bool is_connected() const;
    ConnectionInfo get_connection_info() const;

    // 消息发送
    std::future<Message> send_request(const Message& message);
    std::future<Message> send_request(const std::string& method, 
                                    const std::string& path, 
                                    const std::string& body = "",
                                    const std::map<std::string, std::string>& headers = {});
    
    // 同步发送
    Message send_request_sync(const Message& message);
    Message send_request_sync(const std::string& method, 
                            const std::string& path, 
                            const std::string& body = "",
                            const std::map<std::string, std::string>& headers = {});

    // 服务器功能
    bool start_server(const NetConfig& config);
    bool stop_server();
    bool is_server_running() const;

    // 回调设置
    void set_message_callback(MessageCallback callback);
    void set_connection_callback(ConnectionCallback callback);
    void set_error_callback(ErrorCallback callback);

    // 配置管理
    void update_config(const NetConfig& config);
    NetConfig get_config() const;

    // 统计信息
    nlohmann::json get_statistics() const;

    // 健康检查
    bool health_check();

private:
    // 内部实现
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// 工具函数
namespace utils {
    // JSON工具函数
    std::string json_to_string(const nlohmann::json& j);
    nlohmann::json string_to_json(const std::string& s);
    
    // 消息工具函数
    Message create_request(const std::string& method, 
                          const std::string& path, 
                          const std::string& body = "",
                          const std::map<std::string, std::string>& headers = {});
    
    Message create_response(int status_code, 
                          const std::string& body = "",
                          const std::map<std::string, std::string>& headers = {});
    
    Message create_error(int status_code, 
                        const std::string& error_message,
                        const std::map<std::string, std::string>& headers = {});
    
    // 配置工具函数
    NetConfig create_http_config(const std::string& host, int port);
    NetConfig create_https_config(const std::string& host, int port);
    
    // 验证函数
    bool validate_config(const NetConfig& config);
    bool validate_message(const Message& message);
}

// 全局错误处理
class NetIOError : public std::exception {
public:
    explicit NetIOError(const std::string& message) : message_(message) {}
    const char* what() const noexcept override { return message_.c_str(); }
    
private:
    std::string message_;
};

} // namespace netio

#endif // NETIO_H
