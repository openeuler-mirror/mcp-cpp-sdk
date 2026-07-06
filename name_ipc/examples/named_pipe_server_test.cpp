#include "named_pipe.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <ctime>
#include <unistd.h>

using namespace mcp;

// 示例消息处理器 - 支持请求-响应
class ExampleServerMessageHandler : public NamedPipeMessageHandler {
public:
    NamedPipe* named_pipe_;
    int message_count = 0;
    
    ExampleServerMessageHandler(NamedPipe* pipe) : named_pipe_(pipe) {}
    
    void onMessage(const NamedPipeMessage& message) override {
        message_count++;
        mcp_logger::getModuleLogger("named_pipe_server")->info(
            "[Message #" + std::to_string(message_count) + "] Received: " + message.content
        );
        
        // 处理请求并发送响应
        try {
            nlohmann::json request = nlohmann::json::parse(message.content);
            
            // 检查是否是请求（有 id 字段）
            if (request.contains("id") && request.contains("method")) {
                std::string method = request["method"].get<std::string>();
                std::string request_id = request["id"].get<std::string>();
                nlohmann::json params = request.value("params", nlohmann::json::object());
                
                mcp_logger::getModuleLogger("named_pipe_server")->info(
                    "Handling request: " + method + " (ID: " + request_id + ")"
                );
                
                nlohmann::json response;
                response["jsonrpc"] = "2.0";
                response["id"] = request_id;
                
                if (method == "ping") {
                    response["result"] = nlohmann::json::object();
                    response["result"]["message"] = "pong";
                    response["result"]["timestamp"] = std::time(nullptr);
                }
                else if (method == "echo") {
                    if (params.is_string()) {
                        response["result"] = params.get<std::string>();
                    } else {
                        response["result"] = params;
                    }
                }
                else if (method == "add") {
                    if (params.is_object() && params.contains("a") && params.contains("b")) {
                        int a = params["a"].get<int>();
                        int b = params["b"].get<int>();
                        response["result"] = a + b;
                    } else {
                        response["error"] = nlohmann::json::object();
                        response["error"]["code"] = -32602;
                        response["error"]["message"] = "Invalid params";
                    }
                }
                else if (method == "get_time") {
                    auto now = std::chrono::system_clock::now();
                    auto time_t = std::chrono::system_clock::to_time_t(now);
                    std::string time_str = std::ctime(&time_t);
                    // Remove the trailing newline character from ctime()
                    if (!time_str.empty() && time_str.back() == '\n') {
                        time_str.pop_back();
                    }
                    response["result"] = time_str;
                }
                else {
                    response["error"] = nlohmann::json::object();
                    response["error"]["code"] = -32601;
                    response["error"]["message"] = "Method not found: " + method;
                }
                
                // 发送响应
                if (named_pipe_ && named_pipe_->isConnected()) {
                    named_pipe_->sendMessage(response.dump());
                    mcp_logger::getModuleLogger("named_pipe_server")->info(
                        "Sent response for request ID: " + request_id
                    );
                }
            }
            // 处理通知（没有 id 字段）
            else if (request.contains("method") && !request.contains("id")) {
                std::string method = request["method"].get<std::string>();
                nlohmann::json params = request.value("params", nlohmann::json::object());
                
                mcp_logger::getModuleLogger("named_pipe_server")->info(
                    "Handling notification: " + method
                );
                
                if (method == "log") {
                    if (params.is_object() && params.contains("message")) {
                        std::string log_message = params["message"].get<std::string>();
                        mcp_logger::getModuleLogger("named_pipe_server")->info(
                            "Client log: " + log_message
                        );
                    }
                }
            }
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("named_pipe_server")->error(
                "Failed to process message: " + std::string(e.what())
            );
        }
    }
    
    void onError(const std::string& error) override {
        mcp_logger::getModuleLogger("named_pipe_server")->error("[ERROR] " + error);
    }
    
    void onConnected() override {
        mcp_logger::getModuleLogger("named_pipe_server")->info("✅ Client connected");
    }
    
    void onDisconnected() override {
        mcp_logger::getModuleLogger("named_pipe_server")->info("❌ Client disconnected");
    }
};

int main(int argc, char* argv[]) {
    // 初始化日志系统
    mcp_logger::LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH;
    log_config.file_path = "logs/named_pipe_server_test.log";
    log_config.enable_timestamp = true;
    log_config.format = "[{timestamp}] [{level}] {message}";
    log_config.auto_flush = true;
    
    mcp_logger::Logger::getInstance().initialize(log_config);
    
    // 获取管道路径（默认为 ./tmp/mcp_named_pipe）
    std::string pipe_path = "./tmp/mcp_named_pipe";
    if (argc > 1) {
        pipe_path = argv[1];
    }
    
    mcp_logger::getModuleLogger("named_pipe_server")->info("=== Named Pipe Server Test ===");
    mcp_logger::getModuleLogger("named_pipe_server")->info("Pipe path: " + pipe_path);
    mcp_logger::getModuleLogger("named_pipe_server")->info("Process ID: " + std::to_string(getpid()));
    
    // 创建命名管道实例
    NamedPipe named_pipe;
    
    // 设置消息处理器（需要传递管道指针以便发送响应）
    auto handler = std::make_shared<ExampleServerMessageHandler>(&named_pipe);
    named_pipe.setMessageHandler(handler);
    
    // 配置命名管道（服务器模式 - 双工模式以支持响应）
    NamedPipeConfig config;
    config.pipe_path = pipe_path;
    config.mode = NamedPipeMode::DUPLEX;  // 使用双工模式以支持响应
    config.buffer_size = 4096;
    config.read_timeout_ms = 1000;
    config.write_timeout_ms = 1000;
    config.auto_create = true;
    
    // 启动命名管道
    if (!named_pipe.start(config)) {
        mcp_logger::getModuleLogger("named_pipe_server")->error("❌ Failed to start named pipe server");
        mcp_logger::Logger::getInstance().shutdown();
        return 1;
    }
    
    mcp_logger::getModuleLogger("named_pipe_server")->info("✅ Named pipe server started successfully");
    mcp_logger::getModuleLogger("named_pipe_server")->info("📡 Server is listening for requests...");
    
    // 定期发送心跳通知
    std::thread heartbeat_thread([&named_pipe]() {
        int counter = 0;
        while (named_pipe.isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(30));
            if (named_pipe.isRunning() && named_pipe.isConnected()) {
                nlohmann::json heartbeat;
                heartbeat["jsonrpc"] = "2.0";
                heartbeat["method"] = "heartbeat";
                heartbeat["params"] = nlohmann::json::object();
                heartbeat["params"]["message"] = "Server heartbeat #" + std::to_string(++counter);
                heartbeat["params"]["timestamp"] = std::time(nullptr);
                
                named_pipe.sendMessage(heartbeat.dump());
                mcp_logger::getModuleLogger("named_pipe_server")->info("💓 Sent heartbeat #" + std::to_string(counter));
            }
        }
    });
    
    // 主循环 - 等待消息
    try {
        while (named_pipe.isRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            
            // 检查是否有消息（消息处理在handler中完成）
            if (named_pipe.hasMessage()) {
                NamedPipeMessage msg = named_pipe.getMessage();
                // 消息已经在handler中处理，这里只是确保消息队列被清空
            }
        }
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("named_pipe_server")->error("Exception: " + std::string(e.what()));
    }
    
    // 等待心跳线程结束
    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }
    
    // 停止命名管道
    mcp_logger::getModuleLogger("named_pipe_server")->info("🛑 Stopping named pipe server...");
    named_pipe.stop();
    mcp_logger::getModuleLogger("named_pipe_server")->info("✅ Named pipe server stopped");
    
    // 关闭日志系统
    mcp_logger::Logger::getInstance().shutdown();
    return 0;
}

