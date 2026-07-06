#ifndef UDP_SOCKET_H
#define UDP_SOCKET_H

#include <event2/event.h>
#include <event2/util.h>
#include <string>
#include <functional>
#include <memory>

namespace net_tcp {

// UDP Socket 封装类
class UdpSocket {
public:
    // 回调函数类型定义
    using OnReadCallback = std::function<void(UdpSocket*, const char* data, size_t len, 
                                               const std::string& from_addr, int from_port)>;
    using OnErrorCallback = std::function<void(UdpSocket*, short what)>;

    // 构造函数
    UdpSocket(struct event_base* base);
    
    // 析构函数
    ~UdpSocket();

    // 禁止拷贝构造和赋值
    UdpSocket(const UdpSocket&) = delete;
    UdpSocket& operator=(const UdpSocket&) = delete;

    // 绑定到本地地址和端口（用于服务器）
    bool bind(const std::string& host, int port);
    
    // 发送数据到指定地址
    bool sendTo(const char* data, size_t len, const std::string& host, int port);
    bool sendTo(const std::string& data, const std::string& host, int port);
    
    // 关闭 socket
    void close();
    
    // 获取本地地址和端口
    std::string getLocalAddress() const;
    int getLocalPort() const;
    
    // 设置回调函数
    void setOnReadCallback(OnReadCallback cb) { on_read_ = cb; }
    void setOnErrorCallback(OnErrorCallback cb) { on_error_ = cb; }
    
    // 获取底层 socket 文件描述符
    evutil_socket_t getSocket() const { return socket_fd_; }
    
    // 获取底层 event_base
    struct event_base* getEventBase() const { return base_; }

private:
    // 静态回调函数（供 libevent 调用）
    static void readCallback(evutil_socket_t fd, short what, void* ctx);
    
    // 初始化 socket
    bool initSocket();
    
    struct event_base* base_;
    struct event* read_event_;
    evutil_socket_t socket_fd_;
    
    // 回调函数
    OnReadCallback on_read_;
    OnErrorCallback on_error_;
    
    // 地址信息
    std::string local_address_;
    int local_port_;
    
    // 接收缓冲区
    static const size_t BUFFER_SIZE = 65507; // UDP 最大数据包大小
};

// UDP 服务器类
class UdpServer {
public:
    using OnMessageCallback = std::function<void(const char* data, size_t len,
                                                  const std::string& from_addr, int from_port)>;
    using OnErrorCallback = std::function<void(const std::string& error)>;

    // 构造函数
    UdpServer();
    
    // 析构函数
    ~UdpServer();

    // 禁止拷贝构造和赋值
    UdpServer(const UdpServer&) = delete;
    UdpServer& operator=(const UdpServer&) = delete;

    // 启动服务器
    bool start(const std::string& host, int port);
    
    // 停止服务器
    void stop();
    
    // 运行事件循环
    void run();
    
    // 运行事件循环（非阻塞）
    void runOnce();
    
    // 发送数据到指定地址
    bool sendTo(const char* data, size_t len, const std::string& host, int port);
    bool sendTo(const std::string& data, const std::string& host, int port);
    
    // 设置回调函数
    void setOnMessageCallback(OnMessageCallback cb) { on_message_ = cb; }
    void setOnErrorCallback(OnErrorCallback cb) { on_error_ = cb; }
    
    // 获取监听地址和端口
    std::string getListenAddress() const { return listen_address_; }
    int getListenPort() const { return listen_port_; }
    
    // 检查服务器是否运行
    bool isRunning() const { return is_running_; }

private:
    // 静态回调函数（供 libevent 调用）
    static void readCallback(evutil_socket_t fd, short what, void* ctx);
    
    struct event_base* base_;
    struct event* read_event_;
    evutil_socket_t socket_fd_;
    bool is_running_;
    std::string listen_address_;
    int listen_port_;
    
    // 回调函数
    OnMessageCallback on_message_;
    OnErrorCallback on_error_;
};

} // namespace net_tcp

#endif // UDP_SOCKET_H

