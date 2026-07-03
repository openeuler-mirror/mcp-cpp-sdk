#include "../include/http_transport.h"
#include "../../log/include/logger.h"

namespace mcp_transport {

HttpTransport::HttpTransport()
    : TransportBase() {
}

HttpTransport::~HttpTransport() {
    stop();
}

bool HttpTransport::start(const TransportConfig& config) {
    // 注意：HTTP 传输使用 NetIO，需要处理 CORS 预检请求
    // IPC 传输（StdioTransport）使用 PipeCommunication，不经过此路径
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        if (getLogger()) {
            getLogger()->warning("HttpTransport is already running");
        }
        return false;
    }
    
    config_ = config;
    setStatus(TransportStatus::CONNECTING);
    
    // 如果是服务器模式，先初始化NetIO并设置处理器
    // NetIO 会自动处理 CORS 预检请求（OPTIONS 方法）
    if (config.mode == TransportMode::SERVER) {
        if (!initNetIO()) {
            setStatus(TransportStatus::ERROR);
            triggerErrorCallback("Failed to initialize NetIO");
            return false;
        }
        
        // 设置服务端请求处理器适配器
        if (sse_server_handler_) {
            class SseServerRequestHandlerAdapter : public netio::ServerRequestHandler {
            public:
                explicit SseServerRequestHandlerAdapter(std::shared_ptr<SseServerRequestHandler> handler)
                    : handler_(handler) {}
                
                bool handleRequest(const netio::Message& request, netio::Message& response) override {
                    if (handler_) {
                        return handler_->handleRequest(request, response);
                    }
                    return false;
                }
            private:
                std::shared_ptr<SseServerRequestHandler> handler_;
            };
            
            auto adapter = std::make_shared<SseServerRequestHandlerAdapter>(sse_server_handler_);
            netio_->setServerRequestHandler(adapter);
        }
        
        // 设置连接请求处理器适配器
        if (connection_handler_) {
            class SseConnectionRequestHandlerAdapter : public netio::ConnectionRequestHandler {
            public:
                explicit SseConnectionRequestHandlerAdapter(std::shared_ptr<SseServerConnectHandler> handler)
                    : handler_(handler) {}
                
                bool handleRequest(const netio::Message& request, netio::Message& response) override {
                    if (handler_) {
                        return handler_->handleRequest(request, response);
                    }
                    return false;
                }
            private:
                std::shared_ptr<SseServerConnectHandler> handler_;
            };
            
            auto adapter = std::make_shared<SseConnectionRequestHandlerAdapter>(connection_handler_);
            netio_->setConnectionRequestHandler(adapter);
        }
    }
    
    bool success = false;
    if (config.mode == TransportMode::CLIENT) {
        success = startNetIOClient();
    } else {
        success = startNetIOServer();
    }
    
    if (success) {
        running_ = true;
        setStatus(TransportStatus::CONNECTED);
        
        // 启动接收线程
        startNetIOReceiveThread();
        
        if (getLogger()) {
            getLogger()->info("HttpTransport started successfully");
        }
        
        triggerConnectionCallback(getConnectionInfo());
    } else {
        setStatus(TransportStatus::ERROR);
        triggerErrorCallback("Failed to start HttpTransport");
    }
    
    return success;
}

bool HttpTransport::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        return true;
    }
    
    running_ = false;
    setStatus(TransportStatus::DISCONNECTED);
    
    // 停止NetIO和接收线程
    cleanupNetIO();
    
    // 清空消息队列
    std::queue<TransportMessage> empty;
    message_queue_.swap(empty);
    
    if (getLogger()) {
        getLogger()->info("HttpTransport stopped");
    }
    
    triggerConnectionCallback(getConnectionInfo());
    return true;
}


ConnectionInfo HttpTransport::getConnectionInfo() const {
    ConnectionInfo info;
    info.status = getStatus();
    info.type = TransportType::HTTP;
    
    if (netio_) {
        info.netio_info = netio_->getConnectionInfo();
    }
    
    return info;
}

netio::Protocol HttpTransport::getNetioProtocol() const {
    return netio::Protocol::HTTP;
}

std::future<TransportMessage> HttpTransport::sendRequest(const TransportMessage& message) {
    return std::async(std::launch::async, [this, message]() {
        return sendRequestSync(message);
    });
}


TransportMessage HttpTransport::sendRequestSync(const TransportMessage& message) {
    if (!running_ || !netio_) {
        TransportMessage error_msg;
        error_msg.status_code = 500;
        error_msg.error_message = "Transport is not running";
        return error_msg;
    }
    
    netio::Message netio_msg = toNetioMessage(message);
    auto future = netio_->sendRequest(netio_msg);
    netio::Message response = future.get();
    
    messages_sent_++;
    messages_received_++;
    bytes_sent_ += message.content.size();
    bytes_received_ += response.body.size();
    
    return fromNetioMessage(response);
}


void HttpTransport::setMessageCallback(MessageCallback callback) {
    // 先调用基类方法设置回调
    TransportBase::setMessageCallback(callback);
    
    // 设置NetIO的消息回调
    if (netio_) {
        netio_->setMessageCallback([this, callback](const netio::Message& msg) {
            TransportMessage transport_msg = fromNetioMessage(msg);
            enqueueMessage(transport_msg);
        });
    }
}


void HttpTransport::setServerRequestHandler(std::shared_ptr<TransportServerRequestHandler> /*handler*/) {
    // HttpTransport使用SSE处理器
}

void HttpTransport::setSseServerRequestHandler(std::shared_ptr<SseServerRequestHandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    sse_server_handler_ = handler;
    
    // 设置NetIO的服务端处理器（如果NetIO已初始化）
    if (netio_ && handler) {
        class SseServerRequestHandlerAdapter : public netio::ServerRequestHandler {
        public:
            explicit SseServerRequestHandlerAdapter(std::shared_ptr<SseServerRequestHandler> handler)
                : handler_(handler) {}
            
            bool handleRequest(const netio::Message& request, netio::Message& response) override {
                if (handler_) {
                    return handler_->handleRequest(request, response);
                }
                return false;
            }
        private:
            std::shared_ptr<SseServerRequestHandler> handler_;
        };
        
        auto adapter = std::make_shared<SseServerRequestHandlerAdapter>(handler);
        netio_->setServerRequestHandler(adapter);
    }
}

void HttpTransport::setConnectionRequestHandler(std::shared_ptr<SseServerConnectHandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_handler_ = handler;
    
    // 设置NetIO的连接请求处理器（如果NetIO已初始化）
    if (netio_ && handler) {
        class SseConnectionRequestHandlerAdapter : public netio::ConnectionRequestHandler {
        public:
            explicit SseConnectionRequestHandlerAdapter(std::shared_ptr<SseServerConnectHandler> handler)
                : handler_(handler) {}
            
            bool handleRequest(const netio::Message& request, netio::Message& response) override {
                if (handler_) {
                    return handler_->handleRequest(request, response);
                }
                return false;
            }
        private:
            std::shared_ptr<SseServerConnectHandler> handler_;
        };
        
        auto adapter = std::make_shared<SseConnectionRequestHandlerAdapter>(handler);
        netio_->setConnectionRequestHandler(adapter);
    }
}

std::string HttpTransport::getTransportTypeName() const {
    return "http";
}

bool HttpTransport::sendJsonRpcMessage(const std::string& json_message) {
    if (!netio_) {
        return false;
    }
    
    // 将JSON-RPC消息转换为NetIO消息并发送
    try {
        auto json = nlohmann::json::parse(json_message);
        netio::Message netio_msg;
        
        if (json.contains("id")) {
            netio_msg.id = json["id"].is_string() ? json["id"].get<std::string>() : json["id"].dump();
        }
        
        if (json.contains("method")) {
            netio_msg.method = json["method"];
            netio_msg.type = json.contains("id") ? netio::MessageType::REQUEST : netio::MessageType::NOTIFICATION;
        } else if (json.contains("result")) {
            netio_msg.type = netio::MessageType::RESPONSE;
            netio_msg.body = json["result"].dump();
        } else if (json.contains("error")) {
            netio_msg.type = netio::MessageType::ERROR;
            netio_msg.status_code = json["error"].value("code", 500);
            netio_msg.error_message = json["error"].value("message", "");
        }
        
        netio_->sendRequestSync(netio_msg);
        return true;
    } catch (...) {
        return false;
    }
}

nlohmann::json HttpTransport::getExtendedStatistics() const {
    nlohmann::json extended = nlohmann::json::object();  // 显式初始化为对象
    if (netio_) {
        extended["netio_stats"] = netio_->getStatistics();
    }
    return extended;
}

bool HttpTransport::doHealthCheck() const {
    if (!netio_) {
        return false;
    }
    return netio_->healthCheck();
}

} // namespace mcp_transport

