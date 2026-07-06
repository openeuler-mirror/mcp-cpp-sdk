#include "udp_socket.h"
#include <iostream>
#include <string>
#include <thread>
#include <map>

using namespace net_tcp;

int main(int argc, char* argv[]) {
    std::string host = "0.0.0.0";
    int port = 8889;
    
    if (argc >= 2) {
        port = std::stoi(argv[1]);
    }
    if (argc >= 3) {
        host = argv[2];
    }
    
    std::cout << "UDP Server Example" << std::endl;
    std::cout << "Starting server on " << host << ":" << port << std::endl;
    
    // 创建服务器
    UdpServer server;
    
    // 客户端地址映射（用于记录连接的客户端）
    std::map<std::string, std::pair<std::string, int>> clients;
    
    // 设置消息回调
    server.setOnMessageCallback([&server, &clients](const char* data, size_t len,
                                                      const std::string& from_addr, int from_port) {
        std::string message(data, len);
        std::string client_key = from_addr + ":" + std::to_string(from_port);
        
        std::cout << "Received from " << from_addr << ":" << from_port 
                  << " (" << len << " bytes): " << message;
        
        // 记录客户端
        clients[client_key] = std::make_pair(from_addr, from_port);
        
        // 处理特殊命令
        if (message.find("quit") != std::string::npos || 
            message.find("exit") != std::string::npos) {
            std::cout << "Client " << from_addr << ":" << from_port << " requested disconnect" << std::endl;
            clients.erase(client_key);
            return;
        }
        
        // 回显消息
        std::string echo = "Echo: " + message;
        server.sendTo(echo, from_addr, from_port);
        std::cout << "Sent echo to " << from_addr << ":" << from_port << std::endl;
        
        // 广播给其他客户端
        std::string broadcast_msg = "Broadcast from " + from_addr + ":" + std::to_string(from_port) + 
                                   ": " + message;
        for (const auto& client : clients) {
            if (client.first != client_key) {
                server.sendTo(broadcast_msg, client.second.first, client.second.second);
            }
        }
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
            std::cout << "Connected clients: " << clients.size() << std::endl;
        } else if (line.find("send ") == 0) {
            // 格式: send <host> <port> <message>
            size_t pos1 = line.find(' ', 5);
            if (pos1 != std::string::npos) {
                size_t pos2 = line.find(' ', pos1 + 1);
                if (pos2 != std::string::npos) {
                    std::string target_host = line.substr(5, pos1 - 5);
                    int target_port = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
                    std::string msg = line.substr(pos2 + 1);
                    if (server.sendTo(msg, target_host, target_port)) {
                        std::cout << "Sent message to " << target_host << ":" << target_port << std::endl;
                    } else {
                        std::cerr << "Failed to send message" << std::endl;
                    }
                }
            }
        } else if (!line.empty()) {
            std::cout << "Unknown command: " << line << std::endl;
            std::cout << "Available commands: quit, exit, stop, status, send <host> <port> <message>" << std::endl;
        }
    }
    
    // 等待服务器线程结束
    if (server_thread.joinable()) {
        server_thread.join();
    }
    
    std::cout << "Server stopped" << std::endl;
    return 0;
}

