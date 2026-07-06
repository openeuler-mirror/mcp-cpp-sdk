#include "tcp_socket.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <algorithm>

using namespace net_tcp;

// 客户端连接管理器
class ClientManager {
public:
    void addClient(TcpSocket* client) {
        clients_.push_back(client);
        std::cout << "Client connected. Total clients: " << clients_.size() << std::endl;
    }
    
    void removeClient(TcpSocket* client) {
        clients_.erase(
            std::remove_if(clients_.begin(), clients_.end(),
                [client](TcpSocket* c) { return c == client; }),
            clients_.end()
        );
        std::cout << "Client disconnected. Total clients: " << clients_.size() << std::endl;
    }
    
    void broadcast(const std::string& message, TcpSocket* sender = nullptr) {
        for (auto* client : clients_) {
            if (client != sender && client->getStatus() == ConnectionStatus::CONNECTED) {
                client->send(message);
            }
        }
    }
    
    size_t getClientCount() const {
        return clients_.size();
    }
    
private:
    std::vector<TcpSocket*> clients_;
};

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    int port = 8888;
    
    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }
    if (argc >= 3) {
        host = argv[2];
    }
    
    std::cout << "TCP Server Example" << std::endl;
    std::cout << "Starting server on " << host << ":" << port << std::endl;
    
    // 创建服务器
    TcpServer server;
    
    // 客户端管理器
    ClientManager client_manager;
    
    // 设置接受连接回调
    server.setOnAcceptCallback([&client_manager](TcpSocket* client) {
        client_manager.addClient(client);
        
        std::cout << "New client connected from " 
                  << client->getRemoteAddress() << ":" 
                  << client->getRemotePort() << std::endl;
        
        // 发送欢迎消息
        std::string welcome = "Welcome to TCP Server! Type 'quit' to disconnect.\n";
        client->send(welcome);
        
        // 设置客户端回调
        client->setOnReadCallback([&client_manager, client](TcpSocket* sock, const char* data, size_t len) {
            std::string message(data, len);
            std::cout << "Received from " << sock->getRemoteAddress() 
                      << ":" << sock->getRemotePort() 
                      << " (" << len << " bytes): " << message;
            
            // 处理特殊命令
            if (message.find("quit") != std::string::npos || 
                message.find("exit") != std::string::npos) {
                sock->close();
                return;
            }
            
            // 回显消息
            std::string echo = "Echo: " + message;
            sock->send(echo);
            
            // 广播给其他客户端
            std::string broadcast_msg = "Broadcast from " + sock->getRemoteAddress() + 
                                       ": " + message;
            client_manager.broadcast(broadcast_msg, sock);
        });
        
        client->setOnDisconnectCallback([&client_manager, client](TcpSocket* sock) {
            std::cout << "Client " << sock->getRemoteAddress() 
                      << ":" << sock->getRemotePort() << " disconnected" << std::endl;
            client_manager.removeClient(client);
            delete client;
        });
        
        client->setOnErrorCallback([](TcpSocket* sock, short what) {
            std::cerr << "Error on client " << sock->getRemoteAddress() 
                      << ":" << sock->getRemotePort() << ": " << what << std::endl;
        });
    });
    
    server.setOnErrorCallback([](const std::string& error) {
        std::cerr << "Server error: " << error << std::endl;
    });
    
    // 启动服务器
    if (!server.start(host, port)) {
        std::cerr << "Failed to start server on " << host << ":" << port << std::endl;
        return 1;
    }
    
    std::cout << "Server started successfully. Listening on " 
              << server.getListenAddress() << ":" << server.getListenPort() << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    
    // 在单独的线程中运行事件循环
    std::thread server_thread([&server]() {
        server.run();
    });
    
    // 主线程处理控制台输入
    std::string line;
    while (server.isRunning()) {
        std::cout << "Server> ";
        std::getline(std::cin, line);
        
        if (line == "quit" || line == "exit" || line == "stop") {
            std::cout << "Stopping server..." << std::endl;
            server.stop();
            break;
        } else if (line == "status") {
            std::cout << "Server status: Running" << std::endl;
            std::cout << "Connected clients: " << client_manager.getClientCount() << std::endl;
        } else if (!line.empty()) {
            std::cout << "Unknown command: " << line << std::endl;
            std::cout << "Available commands: quit, exit, stop, status" << std::endl;
        }
    }
    
    // 等待服务器线程结束
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    std::cout << "Server stopped" << std::endl;
    return 0;
}

