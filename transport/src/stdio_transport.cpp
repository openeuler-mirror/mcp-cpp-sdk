#include "../include/stdio_transport.h"
#include "../../log/include/logger.h"
#include <sstream>
#include <iomanip>

namespace mcp_transport {

// IpcMessageHandlerAdapter 实现
void StdioTransport::IpcMessageHandlerAdapter::onMessage(const mcp::Message& message) {
    if (transport_) {
        TransportMessage transport_msg = TransportMessage::fromIpcMessage(message);
        transport_->enqueueMessage(transport_msg);
    }
}

void StdioTransport::IpcMessageHandlerAdapter::onError(const std::string& error) {
    if (transport_) {
        transport_->triggerErrorCallback(error);
    }
}

// IpcServerRequestHandlerAdapter 实现
std::string StdioTransport::IpcServerRequestHandlerAdapter::handleRequest(
    const std::string& method, 
    const std::string& params, 
    const std::string& request_id) {
    if (transport_ && transport_->server_handler_) {
        return transport_->server_handler_->handleRequest(method, params, request_id);
    }
    return "{\"error\": \"No server request handler available\"}";
}

void StdioTransport::IpcServerRequestHandlerAdapter::handleNotification(
    const std::string& method, 
    const std::string& params) {
    if (transport_ && transport_->server_handler_) {
        transport_->server_handler_->handleNotification(method, params);
    }
}

StdioTransport::StdioTransport() 
    : TransportBase() {
}

StdioTransport::~StdioTransport() {
    stop();
}

bool StdioTransport::start(const TransportConfig& config) {
    // 注意：IPC 传输使用 PipeCommunication，完全不经过 NetIO
    // 因此不需要处理 CORS 预检请求，也不涉及 HTTP/HTTPS 相关逻辑
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        if (getLogger()) {
            getLogger()->warning("StdioTransport is already running");
        }
        return false;
    }
    
    config_ = config;
    setStatus(TransportStatus::CONNECTING);
    
    try {
        // 创建IPC通信实例（不使用 NetIO，所以不需要 CORS 处理）
        ipc_comm_ = std::make_unique<mcp::PipeCommunication>();
        
        // 创建并设置消息处理器适配器
        message_handler_adapter_ = std::make_shared<IpcMessageHandlerAdapter>(this);
        ipc_comm_->setMessageHandler(message_handler_adapter_);
        
        // 设置服务端请求处理器
        if (config.mode == TransportMode::SERVER && server_handler_) {
            server_handler_adapter_ = std::make_shared<IpcServerRequestHandlerAdapter>(this);
            ipc_comm_->setServerRequestHandler(server_handler_adapter_);
        }
        
        // 配置IPC
        mcp::PipeConfig ipc_config;
        ipc_config.mode = (config.mode == TransportMode::SERVER) 
            ? mcp::CommunicationMode::SERVER 
            : mcp::CommunicationMode::CLIENT;
        ipc_config.command = config.ipc.command;
        ipc_config.args = config.ipc.args;
        ipc_config.env_vars = config.ipc.env_vars;
        ipc_config.buffer_size = config.ipc.buffer_size;
        ipc_config.read_timeout_ms = config.ipc.read_timeout_ms;
        ipc_config.max_retry_count = config.ipc.max_retry_count;
        ipc_config.enable_logging = config.ipc.enable_logging;
        ipc_config.log_file_path = config.ipc.log_file_path;
        
        // 服务端模式配置
        if (config.mode == TransportMode::SERVER) {
            ipc_config.server_name = config.server.server_name;
            ipc_config.server_version = config.server.server_version;
        }
        
        if (getLogger()) {
            getLogger()->info("Starting StdioTransport with IPC module");
            getLogger()->info("IPC config: command=" + ipc_config.command + 
                ", args_count=" + std::to_string(ipc_config.args.size()) +
                ", buffer_size=" + std::to_string(ipc_config.buffer_size) +
                ", timeout=" + std::to_string(ipc_config.read_timeout_ms) + "ms" +
                ", max_retries=" + std::to_string(ipc_config.max_retry_count));
        }
        
        // 启动IPC通信
        if (!ipc_comm_->start(ipc_config)) {
            if (getLogger()) {
                getLogger()->error("Failed to start IPC communication");
            }
            setStatus(TransportStatus::ERROR);
            triggerErrorCallback("Failed to start IPC communication");
            return false;
        }
        
        running_ = true;
        setStatus(TransportStatus::CONNECTED);
        
        if (getLogger()) {
            getLogger()->info("StdioTransport started successfully with IPC module");
        }
        
        triggerConnectionCallback(getConnectionInfo());
        return true;
    } catch (const std::exception& e) {
        if (getLogger()) {
            getLogger()->error("StdioTransport start error: " + std::string(e.what()));
        }
        setStatus(TransportStatus::ERROR);
        triggerErrorCallback("StdioTransport start error: " + std::string(e.what()));
        return false;
    }
}

bool StdioTransport::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        return true;
    }
    
    running_ = false;
    setStatus(TransportStatus::DISCONNECTED);
    
    // 停止IPC通信
    if (ipc_comm_) {
        ipc_comm_->stop();
        ipc_comm_.reset();
    }
    
    // 清空消息队列
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
        std::queue<TransportMessage> empty;
        message_queue_.swap(empty);
    }
    
    // 清理适配器
    message_handler_adapter_.reset();
    server_handler_adapter_.reset();
    
    if (getLogger()) {
        getLogger()->info("StdioTransport stopped");
    }
    
    triggerConnectionCallback(getConnectionInfo());
    return true;
}


ConnectionInfo StdioTransport::getConnectionInfo() const {
    ConnectionInfo info;
    info.status = getStatus();
    info.type = TransportType::IPC;  // Stdio使用IPC类型
    
    if (ipc_comm_) {
        info.netio_info.remote_address = "localhost";
        info.netio_info.remote_port = 0;
        info.netio_info.local_address = "localhost";
        info.netio_info.local_port = 0;
        info.netio_info.connection_id = std::to_string(ipc_comm_->getProcessId());
    }
    
    return info;
}

std::future<TransportMessage> StdioTransport::sendRequest(const TransportMessage& message) {
    return std::async(std::launch::async, [this, message]() {
        return sendRequestSync(message);
    });
}

TransportMessage StdioTransport::sendRequestSync(const TransportMessage& message) {
    if (!running_ || !ipc_comm_) {
        TransportMessage error_msg;
        error_msg.status_code = 500;
        error_msg.error_message = "Transport is not running";
        return error_msg;
    }
    
    // 对于stdio通信，直接使用message.content（已经是JSON-RPC格式）
    // 而不是转换为包含HTTP字段的JSON格式
    std::string json_str = message.content;
    
    // 使用IPC模块发送请求
    try {
        auto future = ipc_comm_->sendRequest(json_str, 60); // 60秒超时
        std::string response_str = future.get();
        
        if (response_str.empty()) {
            TransportMessage error_msg;
            error_msg.status_code = 500;
            error_msg.error_message = "Failed to get response from IPC";
            return error_msg;
        }
        
        messages_sent_++;
        messages_received_++;
        bytes_sent_ += json_str.size();
        bytes_received_ += response_str.size();
        
        // 解析响应
        try {
            auto json = nlohmann::json::parse(response_str);
            TransportMessage response;
            response.content = response_str;
            response.timestamp = std::chrono::system_clock::now();
            
            if (json.contains("id")) {
                response.id = json["id"].is_string() ? json["id"].get<std::string>() : json["id"].dump();
            }
            
            if (json.contains("result")) {
                response.status_code = 200;
            } else if (json.contains("error")) {
                response.status_code = json["error"].value("code", 500);
                response.error_message = json["error"].value("message", "");
            }
            
            return response;
        } catch (const std::exception& e) {
            TransportMessage error_msg;
            error_msg.status_code = 500;
            error_msg.error_message = "Failed to parse response: " + std::string(e.what());
            return error_msg;
        }
    } catch (const std::exception& e) {
        TransportMessage error_msg;
        error_msg.status_code = 500;
        error_msg.error_message = "Failed to send request: " + std::string(e.what());
        return error_msg;
    }
}

void StdioTransport::setMessageCallback(MessageCallback callback) {
    TransportBase::setMessageCallback(callback);
    // IPC模块的消息处理器已经设置，会自动调用enqueueMessage
}


void StdioTransport::setServerRequestHandler(std::shared_ptr<TransportServerRequestHandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    server_handler_ = handler;
    
    // 如果IPC已经启动，更新服务端处理器适配器
    if (ipc_comm_ && config_.mode == TransportMode::SERVER && handler) {
        if (!server_handler_adapter_) {
            server_handler_adapter_ = std::make_shared<IpcServerRequestHandlerAdapter>(this);
        }
        ipc_comm_->setServerRequestHandler(server_handler_adapter_);
    }
}

std::string StdioTransport::getTransportTypeName() const {
    return "stdio";
}

bool StdioTransport::sendJsonRpcMessage(const std::string& json_message) {
    if (!ipc_comm_ || !running_) {
        return false;
    }
    return ipc_comm_->sendMessage(json_message);
}

nlohmann::json StdioTransport::getExtendedStatistics() const {
    nlohmann::json extended = nlohmann::json::object();  // 显式初始化为对象
    if (ipc_comm_) {
        extended["ipc_process_id"] = ipc_comm_->getProcessId();
        extended["ipc_process_alive"] = ipc_comm_->isProcessAlive();
    }
    return extended;
}

bool StdioTransport::doHealthCheck() const {
    if (!ipc_comm_ || !running_) {
        return false;
    }
    return ipc_comm_->isRunning() && ipc_comm_->isProcessAlive();
}

} // namespace mcp_transport

