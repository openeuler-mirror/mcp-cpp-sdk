#ifndef TCP_SOCKET_H
#define TCP_SOCKET_H

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/listener.h>
#include <string>
#include <functional>
#include <memory>

namespace net_tcp {

// 连接状态枚举
enum class ConnectionStatus {
    DISCONNECTED,
    CONNECTING,
    CONNECTED,
    ERROR
};

// TCP Socket 封装类
class TcpSocket {
public:
    // 回调函数类型定义
    using OnConnectCallback = std::function<void(TcpSocket*)>;
    using OnDisconnectCallback = std::function<void(TcpSocket*)>;
    using OnReadCallback = std::function<void(TcpSocket*, const char* data, size_t len)>;
    using OnErrorCallback = std::function<void(TcpSocket*, short what)>;
    using OnAcceptCallback = std::function<void(TcpSocket*, evutil_socket_t fd, struct sockaddr* addr, int socklen)>;

    // 构造函数 - 用于客户端连接
    TcpSocket(struct event_base* base);
    
    // 构造函数 - 用于服务器接受连接
    TcpSocket(struct event_base* base, struct bufferevent* bev);
    
    // 析构函数
    ~TcpSocket();

    // 禁止拷贝构造和赋值
    TcpSocket(const TcpSocket&) = delete;
    TcpSocket& operator=(const TcpSocket&) = delete;

    // 连接到服务器
    bool connect(const std::string& host, int port);
    
    // 发送数据
    bool send(const char* data, size_t len);
    bool send(const std::string& data);
    
    // 读取数据（从内部缓冲区）
    size_t read(char* buffer, size_t max_len);
    std::string readAll();
    
    // 关闭连接
    void close();
    
    // 获取连接状态
    ConnectionStatus getStatus() const { return status_; }
    
    // 获取本地地址和端口
    std::string getLocalAddress() const;
    int getLocalPort() const;
    
    // 获取远程地址和端口
    std::string getRemoteAddress() const;
    int getRemotePort() const;
    
    // 设置回调函数
    void setOnConnectCallback(OnConnectCallback cb) { on_connect_ = cb; }
    void setOnDisconnectCallback(OnDisconnectCallback cb) { on_disconnect_ = cb; }
    void setOnReadCallback(OnReadCallback cb) { on_read_ = cb; }
    void setOnErrorCallback(OnErrorCallback cb) { on_error_ = cb; }
    
    // 获取底层 bufferevent
    struct bufferevent* getBufferevent() const { return bev_; }
    
    // 获取底层 event_base
    struct event_base* getEventBase() const { return base_; }

private:
    // 静态回调函数（供 libevent 调用）
    static void readCallback(struct bufferevent* bev, void* ctx);
    static void writeCallback(struct bufferevent* bev, void* ctx);
    static void eventCallback(struct bufferevent* bev, short what, void* ctx);
    
    // 初始化 bufferevent
    void initBufferevent();
    
    struct event_base* base_;
    struct bufferevent* bev_;
    ConnectionStatus status_;
    
    // 回调函数
    OnConnectCallback on_connect_;
    OnDisconnectCallback on_disconnect_;
    OnReadCallback on_read_;
    OnErrorCallback on_error_;
    
    // 地址信息
    std::string local_address_;
    int local_port_;
    std::string remote_address_;
    int remote_port_;
};

// TCP 服务器类
class TcpServer {
public:
    using OnAcceptCallback = std::function<void(TcpSocket* client)>;
    using OnErrorCallback = std::function<void(const std::string& error)>;

    // 构造函数
    TcpServer();
    
    // 析构函数
    ~TcpServer();

    // 禁止拷贝构造和赋值
    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    // 启动服务器
    bool start(const std::string& host, int port);
    
    // 停止服务器
    void stop();
    
    // 运行事件循环
    void run();
    
    // 运行事件循环（非阻塞）
    void runOnce();
    
    // 设置回调函数
    void setOnAcceptCallback(OnAcceptCallback cb) { on_accept_ = cb; }
    void setOnErrorCallback(OnErrorCallback cb) { on_error_ = cb; }
    
    // 获取监听地址和端口
    std::string getListenAddress() const { return listen_address_; }
    int getListenPort() const { return listen_port_; }
    
    // 检查服务器是否运行
    bool isRunning() const { return is_running_; }

private:
    // 静态回调函数（供 libevent 调用）
    static void acceptCallback(struct evconnlistener* listener, 
                               evutil_socket_t fd,
                               struct sockaddr* addr, 
                               int socklen, 
                               void* ctx);
    
    static void acceptErrorCallback(struct evconnlistener* listener, void* ctx);

    struct event_base* base_;
    struct evconnlistener* listener_;
    bool is_running_;
    std::string listen_address_;
    int listen_port_;
    
    // 回调函数
    OnAcceptCallback on_accept_;
    OnErrorCallback on_error_;
};

} // namespace net_tcp

#endif // TCP_SOCKET_H

