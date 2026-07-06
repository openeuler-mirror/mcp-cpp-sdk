#ifndef NETIO_H
#define NETIO_H

#include "../../common/httplib.h"
#include "../../common/json.hpp"
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
// 注意：NetIO 专门用于 HTTP/HTTPS 传输，所有请求都需要处理 CORS
// IPC 传输使用 PipeCommunication，完全不经过 NetIO
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
    std::string endpoint = "/"; // HTTP endpoint path for server mode
    
    // SSL/TLS 证书配置（用于 HTTPS 服务器）
    std::string cert_file = "";      // 服务器证书文件路径
    std::string key_file = "";       // 服务器私钥文件路径
    std::string ca_file = "";        // CA 证书文件路径（可选，用于客户端证书验证）
    std::string private_key_password = ""; // 私钥密码（可选）
    
    // 转换为JSON
    nlohmann::json toJson() const;
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
    nlohmann::json toJson() const;
    
    // 从JSON创建
    static Message fromJson(const nlohmann::json& j);
};

// 服务端请求处理器接口
class ServerRequestHandler {
public:
    virtual ~ServerRequestHandler() = default;
    
    /**
     * @brief 处理服务器收到的请求
     * 
     * @param request 服务器收到的请求消息
     * @param response 需要返回给客户端的响应消息
     * @return true 表示需要将 response 发送给客户端
     * @return false 表示无需返回内容（例如通知请求）
     */
    virtual bool handleRequest(const Message& request, Message& response) = 0;
};

// 连接请求处理器接口
class ConnectionRequestHandler {
public:
    virtual ~ConnectionRequestHandler() = default;
    virtual bool handleRequest(const Message& request, Message& response) = 0;
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
    std::string session_id;    
    // 转换为JSON
    nlohmann::json toJson() const;
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
    bool isConnected() const;
    ConnectionInfo getConnectionInfo() const;

    // 消息发送
    std::future<Message> sendRequest(const Message& message);
    std::future<Message> sendRequest(const std::string& method, 
                                    const std::string& path, 
                                    const std::string& body = "",
                                    const std::map<std::string, std::string>& headers = {});
    
    // 同步发送
    Message sendRequestSync(const Message& message);
    Message sendRequestSync(const std::string& method, 
                            const std::string& path, 
                            const std::string& body = "",
                            const std::map<std::string, std::string>& headers = {});

    // 服务器功能
    bool startServer(const NetConfig& config);
    bool stopServer();
    bool isServerRunning() const;

    // 回调设置
    void setMessageCallback(MessageCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    void setErrorCallback(ErrorCallback callback);
    void setServerRequestHandler(std::shared_ptr<ServerRequestHandler> handler);
    void setConnectionRequestHandler(std::shared_ptr<ConnectionRequestHandler> handler);
    
    
    // 配置管理
    void updateConfig(const NetConfig& config);
    NetConfig getConfig() const;

    // 统计信息
    nlohmann::json getStatistics() const;

    // 健康检查
    bool healthCheck();

private:
    // 内部实现
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// 工具函数
namespace utils {
    // JSON工具函数
    std::string jsonToString(const nlohmann::json& j);
    nlohmann::json stringToJson(const std::string& s);
    
    // 消息工具函数
    Message createRequest(const std::string& method, 
                          const std::string& path, 
                          const std::string& body = "",
                          const std::map<std::string, std::string>& headers = {});
    
    Message createResponse(int status_code, 
                          const std::string& body = "",
                          const std::map<std::string, std::string>& headers = {});
    
    Message createError(int status_code, 
                        const std::string& error_message,
                        const std::map<std::string, std::string>& headers = {});
    
    // 配置工具函数
    NetConfig createHttpConfig(const std::string& host, int port);
    NetConfig createHttpsConfig(const std::string& host, int port);
    
    // 验证函数
    bool validateConfig(const NetConfig& config);
    bool validateMessage(const Message& message);
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
