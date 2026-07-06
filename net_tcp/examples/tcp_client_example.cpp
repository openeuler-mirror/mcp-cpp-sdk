#include "tcp_socket.h"
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace net_tcp;

int main(int argc, char* argv[]) {
    std::string host = "127.0.0.1";
    int port = 8888;
    
    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        port = std::stoi(argv[2]);
    }
    
    std::cout << "TCP Client Example" << std::endl;
    std::cout << "Connecting to " << host << ":" << port << std::endl;
    
    // 创建 event_base
    struct event_base* base = event_base_new();
    if (!base) {
        std::cerr << "Failed to create event_base" << std::endl;
        return 1;
    }
    
    // 创建 TcpSocket
    TcpSocket* socket = new TcpSocket(base);
    
    // 设置回调函数
    socket->setOnConnectCallback([socket](TcpSocket* sock) {
        std::cout << "Connected to server!" << std::endl;
        std::cout << "Local: " << sock->getLocalAddress() << ":" << sock->getLocalPort() << std::endl;
        std::cout << "Remote: " << sock->getRemoteAddress() << ":" << sock->getRemotePort() << std::endl;
        
        // 发送欢迎消息
        std::string message = "Hello from client!\n";
        sock->send(message);
        std::cout << "Sent: " << message;
    });
    
    socket->setOnReadCallback([](TcpSocket* sock, const char* data, size_t len) {
        std::string message(data, len);
        std::cout << "Received (" << len << " bytes): " << message;
        
        // 回显收到的消息
        if (message.find("quit") != std::string::npos) {
            sock->close();
        }
    });
    
    socket->setOnDisconnectCallback([](TcpSocket* sock) {
        std::cout << "Disconnected from server" << std::endl;
    });
    
    socket->setOnErrorCallback([](TcpSocket* sock, short what) {
        std::cerr << "Error occurred: " << what << std::endl;
    });
    
    // 连接到服务器
    if (!socket->connect(host, port)) {
        std::cerr << "Failed to connect to " << host << ":" << port << std::endl;
        delete socket;
        event_base_free(base);
        return 1;
    }
    
    // 在单独的线程中运行事件循环
    bool running = true;
    std::thread event_thread([base, &running]() {
        event_base_dispatch(base);
        running = false;
    });
    
    // 主线程处理用户输入
    std::string line;
    while (running && socket->getStatus() == ConnectionStatus::CONNECTED) {
        std::cout << "Enter message (or 'quit' to exit): ";
        std::getline(std::cin, line);
        
        if (line == "quit" || line == "exit") {
            socket->close();
            break;
        }
        
        if (!line.empty()) {
            socket->send(line + "\n");
        }
    }
    
    // 停止事件循环
    event_base_loopbreak(base);
    if (event_thread.joinable()) {
        event_thread.join();
    }
    
    delete socket;
    event_base_free(base);
    
    std::cout << "Client exited" << std::endl;
    return 0;
}

