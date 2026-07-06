#include "tcp_socket.h"
#include <event2/util.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

namespace net_tcp {

// TcpSocket 实现

TcpSocket::TcpSocket(struct event_base* base)
    : base_(base)
    , bev_(nullptr)
    , status_(ConnectionStatus::DISCONNECTED)
    , local_port_(0)
    , remote_port_(0)
{
    initBufferevent();
}

TcpSocket::TcpSocket(struct event_base* base, struct bufferevent* bev)
    : base_(base)
    , bev_(bev)
    , status_(ConnectionStatus::CONNECTED)
    , local_port_(0)
    , remote_port_(0)
{
    if (bev_) {
        // 设置回调函数
        bufferevent_setcb(bev_, readCallback, writeCallback, eventCallback, this);
        bufferevent_enable(bev_, EV_READ | EV_WRITE);
        
        // 获取地址信息
        evutil_socket_t fd = bufferevent_getfd(bev_);
        if (fd >= 0) {
            struct sockaddr_storage local_addr, remote_addr;
            socklen_t addr_len = sizeof(local_addr);
            
            // 获取本地地址
            if (getsockname(fd, (struct sockaddr*)&local_addr, &addr_len) == 0) {
                char addr_str[INET6_ADDRSTRLEN];
                if (local_addr.ss_family == AF_INET) {
                    struct sockaddr_in* sin = (struct sockaddr_in*)&local_addr;
                    inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
                    local_address_ = addr_str;
                    local_port_ = ntohs(sin->sin_port);
                } else if (local_addr.ss_family == AF_INET6) {
                    struct sockaddr_in6* sin6 = (struct sockaddr_in6*)&local_addr;
                    inet_ntop(AF_INET6, &sin6->sin6_addr, addr_str, sizeof(addr_str));
                    local_address_ = addr_str;
                    local_port_ = ntohs(sin6->sin6_port);
                }
            }
            
            // 获取远程地址
            addr_len = sizeof(remote_addr);
            if (getpeername(fd, (struct sockaddr*)&remote_addr, &addr_len) == 0) {
                char addr_str[INET6_ADDRSTRLEN];
                if (remote_addr.ss_family == AF_INET) {
                    struct sockaddr_in* sin = (struct sockaddr_in*)&remote_addr;
                    inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
                    remote_address_ = addr_str;
                    remote_port_ = ntohs(sin->sin_port);
                } else if (remote_addr.ss_family == AF_INET6) {
                    struct sockaddr_in6* sin6 = (struct sockaddr_in6*)&remote_addr;
                    inet_ntop(AF_INET6, &sin6->sin6_addr, addr_str, sizeof(addr_str));
                    remote_address_ = addr_str;
                    remote_port_ = ntohs(sin6->sin6_port);
                }
            }
        }
    }
}

TcpSocket::~TcpSocket() {
    close();
}

void TcpSocket::initBufferevent() {
    if (bev_) {
        bufferevent_free(bev_);
    }
    bev_ = bufferevent_socket_new(base_, -1, BEV_OPT_CLOSE_ON_FREE);
    if (bev_) {
        bufferevent_setcb(bev_, readCallback, writeCallback, eventCallback, this);
    }
}

bool TcpSocket::connect(const std::string& host, int port) {
    if (!bev_) {
        initBufferevent();
        if (!bev_) {
            return false;
        }
    }
    
    status_ = ConnectionStatus::CONNECTING;
    
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
        hints.ai_socktype = SOCK_STREAM;
        
        int err = evutil_getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &result);
        if (err != 0) {
            status_ = ConnectionStatus::ERROR;
            return false;
        }
        
        if (result && result->ai_addr) {
            if (bufferevent_socket_connect(bev_, result->ai_addr, result->ai_addrlen) < 0) {
                evutil_freeaddrinfo(result);
                status_ = ConnectionStatus::ERROR;
                return false;
            }
            evutil_freeaddrinfo(result);
        } else {
            evutil_freeaddrinfo(result);
            status_ = ConnectionStatus::ERROR;
            return false;
        }
    } else {
        if (bufferevent_socket_connect(bev_, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
            status_ = ConnectionStatus::ERROR;
            return false;
        }
    }
    
    bufferevent_enable(bev_, EV_READ | EV_WRITE);
    return true;
}

bool TcpSocket::send(const char* data, size_t len) {
    if (!bev_ || status_ != ConnectionStatus::CONNECTED) {
        return false;
    }
    return bufferevent_write(bev_, data, len) == 0;
}

bool TcpSocket::send(const std::string& data) {
    return send(data.c_str(), data.length());
}

size_t TcpSocket::read(char* buffer, size_t max_len) {
    if (!bev_ || status_ != ConnectionStatus::CONNECTED) {
        return 0;
    }
    return bufferevent_read(bev_, buffer, max_len);
}

std::string TcpSocket::readAll() {
    if (!bev_ || status_ != ConnectionStatus::CONNECTED) {
        return "";
    }
    
    struct evbuffer* input = bufferevent_get_input(bev_);
    size_t len = evbuffer_get_length(input);
    if (len == 0) {
        return "";
    }
    
    std::string data(len, '\0');
    evbuffer_copyout(input, &data[0], len);
    evbuffer_drain(input, len);
    return data;
}

void TcpSocket::close() {
    if (bev_) {
        bufferevent_disable(bev_, EV_READ | EV_WRITE);
        bufferevent_free(bev_);
        bev_ = nullptr;
    }
    status_ = ConnectionStatus::DISCONNECTED;
}

std::string TcpSocket::getLocalAddress() const {
    return local_address_;
}

int TcpSocket::getLocalPort() const {
    return local_port_;
}

std::string TcpSocket::getRemoteAddress() const {
    return remote_address_;
}

int TcpSocket::getRemotePort() const {
    return remote_port_;
}

void TcpSocket::readCallback(struct bufferevent* bev, void* ctx) {
    TcpSocket* socket = static_cast<TcpSocket*>(ctx);
    if (socket && socket->on_read_) {
        struct evbuffer* input = bufferevent_get_input(bev);
        size_t len = evbuffer_get_length(input);
        if (len > 0) {
            // 分配缓冲区并读取数据
            char* data = new char[len];
            evbuffer_remove(input, data, len);
            socket->on_read_(socket, data, len);
            delete[] data;
        }
    }
}

void TcpSocket::writeCallback(struct bufferevent* /*bev*/, void* /*ctx*/) {
    // 写入缓冲区已清空，可以继续写入
    // 如果需要，可以在这里添加回调
}

void TcpSocket::eventCallback(struct bufferevent* bev, short what, void* ctx) {
    TcpSocket* socket = static_cast<TcpSocket*>(ctx);
    if (!socket) {
        return;
    }
    
    if (what & BEV_EVENT_CONNECTED) {
        socket->status_ = ConnectionStatus::CONNECTED;
        
        // 获取地址信息
        evutil_socket_t fd = bufferevent_getfd(bev);
        if (fd >= 0) {
            struct sockaddr_storage local_addr, remote_addr;
            socklen_t addr_len = sizeof(local_addr);
            
            // 获取本地地址
            if (getsockname(fd, (struct sockaddr*)&local_addr, &addr_len) == 0) {
                char addr_str[INET6_ADDRSTRLEN];
                if (local_addr.ss_family == AF_INET) {
                    struct sockaddr_in* sin = (struct sockaddr_in*)&local_addr;
                    inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
                    socket->local_address_ = addr_str;
                    socket->local_port_ = ntohs(sin->sin_port);
                }
            }
            
            // 获取远程地址
            addr_len = sizeof(remote_addr);
            if (getpeername(fd, (struct sockaddr*)&remote_addr, &addr_len) == 0) {
                char addr_str[INET6_ADDRSTRLEN];
                if (remote_addr.ss_family == AF_INET) {
                    struct sockaddr_in* sin = (struct sockaddr_in*)&remote_addr;
                    inet_ntop(AF_INET, &sin->sin_addr, addr_str, sizeof(addr_str));
                    socket->remote_address_ = addr_str;
                    socket->remote_port_ = ntohs(sin->sin_port);
                }
            }
        }
        
        if (socket->on_connect_) {
            socket->on_connect_(socket);
        }
    } else if (what & (BEV_EVENT_EOF | BEV_EVENT_ERROR)) {
        socket->status_ = ConnectionStatus::DISCONNECTED;
        if (socket->on_disconnect_) {
            socket->on_disconnect_(socket);
        }
        if (what & BEV_EVENT_ERROR && socket->on_error_) {
            socket->on_error_(socket, what);
        }
    }
}

// TcpServer 实现

TcpServer::TcpServer()
    : base_(nullptr)
    , listener_(nullptr)
    , is_running_(false)
    , listen_port_(0)
{
    base_ = event_base_new();
}

TcpServer::~TcpServer() {
    stop();
    if (base_) {
        event_base_free(base_);
        base_ = nullptr;
    }
}

bool TcpServer::start(const std::string& host, int port) {
    if (is_running_) {
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
    
    listener_ = evconnlistener_new_bind(
        base_,
        acceptCallback,
        this,
        LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
        -1,
        (struct sockaddr*)&sin,
        sizeof(sin)
    );
    
    if (!listener_) {
        return false;
    }
    
    evconnlistener_set_error_cb(listener_, acceptErrorCallback);
    
    listen_address_ = host.empty() ? "0.0.0.0" : host;
    listen_port_ = port;
    is_running_ = true;
    
    return true;
}

void TcpServer::stop() {
    if (listener_) {
        evconnlistener_free(listener_);
        listener_ = nullptr;
    }
    is_running_ = false;
}

void TcpServer::run() {
    if (base_ && is_running_) {
        event_base_dispatch(base_);
    }
}

void TcpServer::runOnce() {
    if (base_ && is_running_) {
        event_base_loop(base_, EVLOOP_NONBLOCK);
    }
}

void TcpServer::acceptCallback(struct evconnlistener* /*listener*/,
                                evutil_socket_t fd,
                                struct sockaddr* /*addr*/,
                                int /*socklen*/,
                                void* ctx) {
    TcpServer* server = static_cast<TcpServer*>(ctx);
    if (!server || !server->base_) {
        close(fd);
        return;
    }
    
    // 设置 socket 为非阻塞模式
    evutil_make_socket_nonblocking(fd);
    
    // 创建 bufferevent
    struct bufferevent* bev = bufferevent_socket_new(server->base_, fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev) {
        close(fd);
        return;
    }
    
    // 创建 TcpSocket 对象
    TcpSocket* client = new TcpSocket(server->base_, bev);
    
    // 调用接受回调
    if (server->on_accept_) {
        server->on_accept_(client);
    } else {
        // 如果没有设置回调，直接关闭连接
        delete client;
    }
}

void TcpServer::acceptErrorCallback(struct evconnlistener* /*listener*/, void* ctx) {
    TcpServer* server = static_cast<TcpServer*>(ctx);
    if (server && server->on_error_) {
        int err = EVUTIL_SOCKET_ERROR();
        std::string error = "Accept error: " + std::string(evutil_socket_error_to_string(err));
        server->on_error_(error);
    }
}

} // namespace net_tcp

