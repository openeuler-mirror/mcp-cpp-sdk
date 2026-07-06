#include "udp_socket.h"
#include <iostream>
#include <string>
#include <thread>

using namespace net_tcp;

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 8889;
    int local_port = 0; // 0 表示系统自动分配
    
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = std::stoi(argv[2]);
    }
    if (argc >= 4) {
        local_port = std::stoi(argv[3]);
    }
    
    std::cout << "UDP Client Example" << std::endl;
    std::cout << "Sending to " << host << ":" << port << std::endl;
    
    // 创建 event_base
    struct event_base* base = event_base_new();
    if (!base) {
        std::cerr << "Failed to create event_base" << std::endl;
        return 1;
    }
    
    // 创建 UdpSocket
    UdpSocket* socket = new UdpSocket(base);
    
    // 绑定说明：
    // 1. 如果 local_port > 0：绑定到指定端口（用于需要固定端口的场景）
    // 2. 如果 local_port == 0：不绑定，系统会在第一次发送时自动分配临时端口
    //    但这样无法接收服务器的回复！如果需要接收回复，应该绑定到端口 0（自动分配）
    // 3. 如果设置了接收回调，建议绑定到端口 0，这样既能发送也能接收
    if (local_port > 0) {
        // 绑定到指定端口
        if (!socket->bind("0.0.0.0", local_port)) {
            std::cerr << "Failed to bind to local port " << local_port << std::endl;
            delete socket;
            event_base_free(base);
            return 1;
        }
        std::cout << "Bound to local port: " << socket->getLocalPort() << std::endl;
    } else {
        // 如果需要接收服务器回复，自动绑定到端口 0（系统自动分配）
        // 这样既能发送数据，也能接收回复
        if (!socket->bind("0.0.0.0", 0)) {
            std::cerr << "Failed to auto-bind socket" << std::endl;
            delete socket;
            event_base_free(base);
            return 1;
        }
        std::cout << "Auto-bound to local port: " << socket->getLocalPort() 
                  << " (for receiving server responses)" << std::endl;
    }
    
    // 设置回调函数
    socket->setOnReadCallback([](UdpSocket* sock, const char* data, size_t len,
                                 const std::string& from_addr, int from_port) {
        std::string message(data, len);
        std::cout << "Received from " << from_addr << ":" << from_port 
                  << " (" << len << " bytes): " << message;
    });
    
    socket->setOnErrorCallback([](UdpSocket* sock, short what) {
        std::cerr << "Error occurred: " << what << std::endl;
    });
    
    // 在单独的线程中运行事件循环
    bool running = true;
    std::thread event_thread([base, &running]() {
        event_base_dispatch(base);
        running = false;
    });
    
    // 主线程处理用户输入
    std::string line;
    std::cout << "Enter messages to send (or 'quit' to exit):" << std::endl;
    
    while (running) {
        std::cout << "> ";
        std::getline(std::cin, line);
        
        if (line == "quit" || line == "exit") {
            break;
        }
        
        if (!line.empty()) {
            if (socket->sendTo(line, host, port)) {
                std::cout << "Sent: " << line << std::endl;
            } else {
                std::cerr << "Failed to send message" << std::endl;
            }
        }
    }
    
    // 停止事件循环
    socket->close();
    event_base_loopbreak(base);
    if (event_thread.joinable()) {
        event_thread.join();
    }
    
    delete socket;
    event_base_free(base);
    
    std::cout << "Client exited" << std::endl;
    return 0;
}

