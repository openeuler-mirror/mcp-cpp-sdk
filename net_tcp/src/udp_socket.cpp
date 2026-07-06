#include "udp_socket.h"
#include <event2/util.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace net_tcp {

// UdpSocket 实现

UdpSocket::UdpSocket(struct event_base* base)
    : base_(base)
    , read_event_(nullptr)
    , socket_fd_(-1)
    , local_port_(0)
{
}

UdpSocket::~UdpSocket() {
    close();
}

bool UdpSocket::initSocket() {
    if (socket_fd_ >= 0) {
        return true;
    }
    
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // 设置 socket 为非阻塞模式
    evutil_make_socket_nonblocking(socket_fd_);
    
    // 设置 socket 选项：允许地址重用
    int reuse = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    return true;
}

bool UdpSocket::bind(const std::string& host, int port) {
    if (!initSocket()) {
        return false;
    }
    
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    if (host.empty() || host == "0.0.0.0") {
        sin.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host.c_str(), &sin.sin_addr) <= 0) {
            return false;
        }
    }
    
    if (::bind(socket_fd_, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        close();
        return false;
    }
    
    // 获取绑定的地址信息
    struct sockaddr_in local_addr;
    socklen_t addr_len = sizeof(local_addr);
    if (getsockname(socket_fd_, (struct sockaddr*)&local_addr, &addr_len) == 0) {
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &local_addr.sin_addr, addr_str, sizeof(addr_str));
        local_address_ = addr_str;
        local_port_ = ntohs(local_addr.sin_port);
    }
    
    // 创建读事件
    read_event_ = event_new(base_, socket_fd_, EV_READ | EV_PERSIST, readCallback, this);
    if (!read_event_) {
        close();
        return false;
    }
    
    event_add(read_event_, nullptr);
    return true;
}

bool UdpSocket::sendTo(const char* data, size_t len, const std::string& host, int port) {
    if (socket_fd_ < 0 && !initSocket()) {
        return false;
    }
    
    // 如果 socket 还没有绑定，且需要接收回复，自动绑定到端口 0（系统自动分配）
    // 这样服务器才能知道回复到哪个端口
    if (read_event_ == nullptr) {
        // 还没有绑定，自动绑定到端口 0（系统自动分配）
        if (!bind("0.0.0.0", 0)) {
            return false;
        }
    }
    
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &sin.sin_addr) <= 0) {
        // 尝试 DNS 解析
        struct evutil_addrinfo hints;
        struct evutil_addrinfo* result = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        
        int err = evutil_getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
        if (err != 0) {
            return false;
        }
        
        if (result && result->ai_addr) {
            ssize_t sent = sendto(socket_fd_, data, len, 0, result->ai_addr, result->ai_addrlen);
            evutil_freeaddrinfo(result);
            return sent == static_cast<ssize_t>(len);
        } else {
            evutil_freeaddrinfo(result);
            return false;
        }
    } else {
        ssize_t sent = sendto(socket_fd_, data, len, 0, (struct sockaddr*)&sin, sizeof(sin));
        return sent == static_cast<ssize_t>(len);
    }
}

bool UdpSocket::sendTo(const std::string& data, const std::string& host, int port) {
    return sendTo(data.c_str(), data.length(), host, port);
}

void UdpSocket::close() {
    if (read_event_) {
        event_del(read_event_);
        event_free(read_event_);
        read_event_ = nullptr;
    }
    
    if (socket_fd_ >= 0) {
        evutil_closesocket(socket_fd_);
        socket_fd_ = -1;
    }
}

std::string UdpSocket::getLocalAddress() const {
    return local_address_;
}

int UdpSocket::getLocalPort() const {
    return local_port_;
}

void UdpSocket::readCallback(evutil_socket_t fd, short /*what*/, void* ctx) {
    UdpSocket* socket = static_cast<UdpSocket*>(ctx);
    if (!socket || !socket->on_read_) {
        return;
    }
    
    char buffer[UdpSocket::BUFFER_SIZE];
    struct sockaddr_in from_addr;
    socklen_t addr_len = sizeof(from_addr);
    
    ssize_t len = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, 
                          (struct sockaddr*)&from_addr, &addr_len);
    
    if (len > 0) {
        buffer[len] = '\0';
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, addr_str, sizeof(addr_str));
        std::string from_address = addr_str;
        int from_port = ntohs(from_addr.sin_port);
        
        socket->on_read_(socket, buffer, len, from_address, from_port);
    } else if (len < 0) {
        // 错误处理
        if (socket->on_error_) {
            socket->on_error_(socket, EV_READ);
        }
    }
}

// UdpServer 实现

UdpServer::UdpServer()
    : base_(nullptr)
    , read_event_(nullptr)
    , socket_fd_(-1)
    , is_running_(false)
    , listen_port_(0)
{
    base_ = event_base_new();
}

UdpServer::~UdpServer() {
    stop();
    if (base_) {
        event_base_free(base_);
        base_ = nullptr;
    }
}

bool UdpServer::start(const std::string& host, int port) {
    if (is_running_) {
        return false;
    }
    
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        return false;
    }
    
    // 设置 socket 为非阻塞模式
    evutil_make_socket_nonblocking(socket_fd_);
    
    // 设置 socket 选项
    int reuse = 1;
    setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    if (host.empty() || host == "0.0.0.0") {
        sin.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, host.c_str(), &sin.sin_addr) <= 0) {
            evutil_closesocket(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
    }
    
    if (::bind(socket_fd_, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        evutil_closesocket(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    // 创建读事件
    read_event_ = event_new(base_, socket_fd_, EV_READ | EV_PERSIST, readCallback, this);
    if (!read_event_) {
        evutil_closesocket(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    event_add(read_event_, nullptr);
    
    listen_address_ = host.empty() ? "0.0.0.0" : host;
    listen_port_ = port;
    is_running_ = true;
    
    return true;
}

void UdpServer::stop() {
    if (read_event_) {
        event_del(read_event_);
        event_free(read_event_);
        read_event_ = nullptr;
    }
    
    if (socket_fd_ >= 0) {
        evutil_closesocket(socket_fd_);
        socket_fd_ = -1;
    }
    
    is_running_ = false;
}

void UdpServer::run() {
    if (base_ && is_running_) {
        event_base_dispatch(base_);
    }
}

void UdpServer::runOnce() {
    if (base_ && is_running_) {
        event_base_loop(base_, EVLOOP_NONBLOCK);
    }
}

bool UdpServer::sendTo(const char* data, size_t len, const std::string& host, int port) {
    if (socket_fd_ < 0) {
        return false;
    }
    
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host.c_str(), &sin.sin_addr) <= 0) {
        // 尝试 DNS 解析
        struct evutil_addrinfo hints;
        struct evutil_addrinfo* result = nullptr;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        
        int err = evutil_getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
        if (err != 0) {
            return false;
        }
        
        if (result && result->ai_addr) {
            ssize_t sent = sendto(socket_fd_, data, len, 0, result->ai_addr, result->ai_addrlen);
            evutil_freeaddrinfo(result);
            return sent == static_cast<ssize_t>(len);
        } else {
            evutil_freeaddrinfo(result);
            return false;
        }
    } else {
        ssize_t sent = sendto(socket_fd_, data, len, 0, (struct sockaddr*)&sin, sizeof(sin));
        return sent == static_cast<ssize_t>(len);
    }
}

bool UdpServer::sendTo(const std::string& data, const std::string& host, int port) {
    return sendTo(data.c_str(), data.length(), host, port);
}

void UdpServer::readCallback(evutil_socket_t fd, short /*what*/, void* ctx) {
    UdpServer* server = static_cast<UdpServer*>(ctx);
    if (!server || !server->on_message_) {
        return;
    }
    
    char buffer[65507]; // UDP 最大数据包大小
    struct sockaddr_in from_addr;
    socklen_t addr_len = sizeof(from_addr);
    
    ssize_t len = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, 
                          (struct sockaddr*)&from_addr, &addr_len);
    
    if (len > 0) {
        buffer[len] = '\0';
        char addr_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &from_addr.sin_addr, addr_str, sizeof(addr_str));
        std::string from_address = addr_str;
        int from_port = ntohs(from_addr.sin_port);
        
        server->on_message_(buffer, len, from_address, from_port);
    } else if (len < 0) {
        // 错误处理
        if (server->on_error_) {
            int err = EVUTIL_SOCKET_ERROR();
            std::string error = "Receive error: " + std::string(evutil_socket_error_to_string(err));
            server->on_error_(error);
        }
    }
}

} // namespace net_tcp

