#include "../include/transport_base.h"
#include "../../log/include/logger.h"
#include "../../net/include/netio.h"
#include "../../ipc/include/pipe_communication.h"

namespace mcp_transport {

// ========== TransportMessage 方法实现 ==========

nlohmann::json TransportMessage::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["content"] = content;
    j["method"] = method;
    j["path"] = path;
    j["headers"] = headers;
    j["status_code"] = status_code;
    j["error_message"] = error_message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::seconds>(
        timestamp.time_since_epoch()).count();
    return j;
}

TransportMessage TransportMessage::fromJson(const nlohmann::json& j) {
    TransportMessage msg;
    if (j.contains("id")) msg.id = j["id"];
    if (j.contains("content")) msg.content = j["content"];
    if (j.contains("method")) msg.method = j["method"];
    if (j.contains("path")) msg.path = j["path"];
    if (j.contains("headers")) msg.headers = j["headers"].get<std::map<std::string, std::string>>();
    if (j.contains("status_code")) msg.status_code = j["status_code"];
    if (j.contains("error_message")) msg.error_message = j["error_message"];
    if (j.contains("timestamp")) {
        auto time_sec = j["timestamp"].get<int64_t>();
        msg.timestamp = std::chrono::system_clock::time_point(
            std::chrono::seconds(time_sec));
    } else {
        msg.timestamp = std::chrono::system_clock::now();
    }
    return msg;
}

TransportMessage TransportMessage::fromIpcMessage(const mcp::Message& msg) {
    TransportMessage transport_msg;
    transport_msg.id = msg.id;
    transport_msg.content = msg.content;
    transport_msg.method = "POST"; // IPC 默认使用 POST
    transport_msg.path = "/";
    transport_msg.timestamp = std::chrono::system_clock::now();
    return transport_msg;
}

TransportMessage TransportMessage::fromNetioMessage(const netio::Message& msg) {
    TransportMessage transport_msg;
    transport_msg.id = msg.id;
    transport_msg.content = msg.body;
    transport_msg.method = msg.method;
    transport_msg.path = msg.path;
    transport_msg.headers = msg.headers;
    transport_msg.status_code = msg.status_code;
    transport_msg.error_message = msg.error_message;
    transport_msg.timestamp = msg.timestamp;
    return transport_msg;
}

mcp::Message TransportMessage::toIpcMessage() const {
    mcp::Message ipc_msg;
    ipc_msg.id = id;
    ipc_msg.content = content;
    ipc_msg.type = mcp::MessageType::REQUEST;
    return ipc_msg;
}

netio::Message TransportMessage::toNetioMessage() const {
    netio::Message netio_msg;
    netio_msg.id = id;
    netio_msg.body = content;
    netio_msg.method = method;
    netio_msg.path = path;
    netio_msg.headers = headers;
    netio_msg.status_code = status_code;
    netio_msg.error_message = error_message;
    netio_msg.timestamp = timestamp;
    netio_msg.type = netio::MessageType::REQUEST;
    return netio_msg;
}

// ========== ConnectionInfo 方法实现 ==========

nlohmann::json ConnectionInfo::toJson() const {
    nlohmann::json j;
    j["status"] = static_cast<int>(status);
    j["type"] = static_cast<int>(type);
    j["netio_info"] = netio_info.toJson();
    return j;
}

// ========== TransportBase 实现 ==========

TransportBase::TransportBase() 
    : status_(TransportStatus::DISCONNECTED)
    , running_(false)
    , messages_sent_(0)
    , messages_received_(0)
    , bytes_sent_(0)
    , bytes_received_(0)
    , module_logger_(mcp_logger::getModuleLogger("transport_base")) {
}

TransportBase::~TransportBase() {
    // 注意：不要在析构函数中调用纯虚函数
    // 子类应该在析构前调用stop()
}

bool TransportBase::isRunning() const {
    return running_.load();
}

TransportStatus TransportBase::getStatus() const {
    return status_.load();
}

std::future<TransportMessage> TransportBase::sendRequest(
    const std::string& content,
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& headers) {
    
    TransportMessage msg;
    msg.content = content;
    msg.method = method;
    msg.path = path;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    
    return sendRequest(msg);
}

TransportMessage TransportBase::sendRequestSync(
    const std::string& content,
    const std::string& method,
    const std::string& path,
    const std::map<std::string, std::string>& headers) {
    
    TransportMessage msg;
    msg.content = content;
    msg.method = method;
    msg.path = path;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    
    return sendRequestSync(msg);
}

bool TransportBase::hasMessage() const {
    // 使用锁保护，确保线程安全
    // 注意：虽然getMessage()也会获取同一个锁，但这是正常的读操作，不会导致死锁
    // 死锁通常发生在多个锁的情况下，这里只有一个锁
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return !message_queue_.empty();
}

TransportMessage TransportBase::getMessage() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    if (message_queue_.empty()) {
        TransportMessage empty_msg;
        return empty_msg;
    }
    
    TransportMessage msg = message_queue_.front();
    message_queue_.pop();
    return msg;
}

std::vector<TransportMessage> TransportBase::getAllMessages() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    
    std::vector<TransportMessage> messages;
    while (!message_queue_.empty()) {
        messages.push_back(message_queue_.front());
        message_queue_.pop();
    }
    
    return messages;
}

void TransportBase::setMaxQueueSize(size_t max_size) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    max_queue_size_ = max_size;
}

size_t TransportBase::getQueueSize() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return message_queue_.size();
}

bool TransportBase::enqueueMessage(const TransportMessage& message) {
    // 先检查队列大小，避免在锁内进行复杂操作
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        // 检查队列大小限制（max_queue_size_ == 0 表示无限制）
        if (max_queue_size_ > 0 && message_queue_.size() >= max_queue_size_) {
            if (getLogger()) {
                getLogger()->warning("Message queue is full, dropping message. Queue size: " + 
                    std::to_string(message_queue_.size()));
            }
            return false;
        }
        message_queue_.push(message);
    }
    // 在锁外通知和触发回调，避免死锁和性能问题
    queue_cv_.notify_one();
    // 回调在锁外调用，避免回调函数中再次获取锁导致死锁
    // 同时避免慢回调阻塞生产者线程
    triggerMessageCallback(message);
    return true;
}

void TransportBase::setMessageCallback(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    message_callback_ = callback;
}

void TransportBase::setConnectionCallback(ConnectionCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_callback_ = callback;
}

void TransportBase::setErrorCallback(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_callback_ = callback;
}

void TransportBase::updateConfig(const TransportConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

TransportConfig TransportBase::getConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

nlohmann::json TransportBase::getStatistics() const {
    // 使用锁保护，确保读取多个原子变量时的一致性
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 读取队列大小需要单独的锁
    size_t queue_size = 0;
    {
        std::lock_guard<std::mutex> queue_lock(queue_mutex_);
        queue_size = message_queue_.size();
    }
    
    nlohmann::json stats = {
        {"type", getTransportTypeName()},
        {"running", running_.load()},
        {"status", static_cast<int>(getStatus())},
        {"messages_sent", messages_sent_.load()},
        {"messages_received", messages_received_.load()},
        {"bytes_sent", bytes_sent_.load()},
        {"bytes_received", bytes_received_.load()},
        {"queue_size", queue_size},
        {"max_queue_size", max_queue_size_}
    };
    
    // 添加子类的扩展统计信息
    nlohmann::json extended = getExtendedStatistics();
    if (!extended.empty()) {
        stats.update(extended);
    }
    
    return stats;
}


nlohmann::json TransportBase::getExtendedStatistics() const {
    return nlohmann::json::object();
}

bool TransportBase::healthCheck() {
    if (!running_.load()) {
        return false;
    }
    return doHealthCheck();
}

bool TransportBase::doHealthCheck() const {
    return getStatus() == TransportStatus::CONNECTED;
}

bool TransportBase::sendResponse(const std::string& request_id, const std::string& result) {
    nlohmann::json response = {
        {"jsonrpc", "2.0"},
        {"id", request_id},
        {"result", nlohmann::json::parse(result)}
    };
    
    return sendJsonRpcMessage(response.dump());
}

bool TransportBase::sendError(const std::string& request_id, int error_code, const std::string& error_message) {
    nlohmann::json error = {
        {"jsonrpc", "2.0"},
        {"id", request_id},
        {"error", {
            {"code", error_code},
            {"message", error_message}
        }}
    };
    
    return sendJsonRpcMessage(error.dump());
}

bool TransportBase::sendNotification(const std::string& method, const std::string& params) {
    nlohmann::json notification = {
        {"jsonrpc", "2.0"},
        {"method", method},
        {"params", nlohmann::json::parse(params)}
    };
    
    return sendJsonRpcMessage(notification.dump());
}

void TransportBase::setSseServerRequestHandler(std::shared_ptr<SseServerRequestHandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    sse_server_handler_ = handler;
}

void TransportBase::setConnectionRequestHandler(std::shared_ptr<SseServerConnectHandler> handler) {
    std::lock_guard<std::mutex> lock(mutex_);
    connection_handler_ = handler;
}

std::shared_ptr<mcp_logger::Logger::ModuleLogger> TransportBase::getLogger() const {
    return module_logger_;
}

void TransportBase::setStatus(TransportStatus status) {
    status_.store(status);
}

void TransportBase::triggerMessageCallback(const TransportMessage& message) {
    if (message_callback_) {
        try {
            message_callback_(message);
        } catch (const std::exception& e) {
            if (module_logger_) {
                module_logger_->error("Error in message callback: " + std::string(e.what()));
            }
        }
    }
}

void TransportBase::triggerConnectionCallback(const ConnectionInfo& info) {
    if (connection_callback_) {
        try {
            connection_callback_(info);
        } catch (const std::exception& e) {
            if (module_logger_) {
                module_logger_->error("Error in connection callback: " + std::string(e.what()));
            }
        }
    }
}

void TransportBase::triggerErrorCallback(const std::string& error) {
    if (error_callback_) {
        try {
            error_callback_(error);
        } catch (const std::exception& e) {
            if (module_logger_) {
                module_logger_->error("Error in error callback: " + std::string(e.what()));
            }
        }
    }
}

// ========== 基于NetIO的通用功能实现 ==========
// 注意：NetIO 专门用于 HTTP/HTTPS 传输，总是需要处理 CORS
// IPC 传输（StdioTransport）使用 PipeCommunication，不调用这些方法

netio::Protocol TransportBase::getNetioProtocol() const {
    // 默认返回HTTP，子类应该覆盖此方法
    // 注意：只有 HTTP/HTTPS 传输才需要实现此方法
    return netio::Protocol::HTTP;
}

bool TransportBase::initNetIO() {
    // 注意：只有 HTTP/HTTPS 传输才需要初始化 NetIO
    // IPC 传输使用 PipeCommunication，不调用此方法
    if (netio_) {
        if (getLogger()) {
            getLogger()->warning("NetIO already initialized");
        }
        return true;
    }
    
    netio_ = std::make_unique<netio::NetIO>();
    return netio_ != nullptr;
}

void TransportBase::cleanupNetIO() {
    stopNetIOReceiveThread();
    
    if (netio_) {
        if (config_.mode == TransportMode::SERVER) {
            netio_->stopServer();
        } else {
            netio_->disconnect();
        }
        netio_.reset();
    }
}

bool TransportBase::startNetIOClient() {
    if (!netio_) {
        if (!initNetIO()) {
            return false;
        }
    }
    
    netio::NetConfig net_config;
    net_config.host = config_.sse.host;
    net_config.port = config_.sse.port;
    net_config.protocol = getNetioProtocol();
    net_config.timeout_seconds = config_.sse.timeout_seconds;
    net_config.max_retries = config_.sse.max_retries;
    net_config.headers = config_.sse.headers;
    net_config.user_agent = config_.sse.user_agent;
    net_config.keep_alive = config_.sse.keep_alive;
    net_config.auto_reconnect = config_.sse.auto_reconnect;
    net_config.reconnect_interval = config_.sse.reconnect_interval;
    net_config.endpoint = config_.sse.endpoint;
    
    // 允许子类添加特定配置
    configureNetIO(net_config);
    
    return netio_->connect(net_config);
}

bool TransportBase::startNetIOServer() {
    // 注意：此方法仅用于 HTTP/HTTPS 传输
    // IPC 传输（StdioTransport）使用 PipeCommunication::start()，不调用此方法
    if (!netio_) {
        if (!initNetIO()) {
            return false;
        }
    }
    
    netio::NetConfig net_config;
    net_config.host = config_.sse.host;
    net_config.port = config_.sse.port;
    net_config.protocol = getNetioProtocol();
    net_config.endpoint = config_.sse.endpoint;
    net_config.headers = config_.sse.headers;  // CORS headers, only used by HTTP/HTTPS transport

    // 允许子类添加特定配置
    configureNetIO(net_config);

    return netio_->startServer(net_config);
}

void TransportBase::stopNetIO() {
    if (netio_) {
        if (config_.mode == TransportMode::SERVER) {
            netio_->stopServer();
        } else {
            netio_->disconnect();
        }
    }
}

netio::Message TransportBase::toNetioMessage(const TransportMessage& msg) {
    netio::Message netio_msg;
    netio_msg.id = msg.id;
    netio_msg.method = msg.method;
    netio_msg.path = msg.path;
    netio_msg.body = msg.content;
    netio_msg.headers = msg.headers;
    netio_msg.status_code = msg.status_code;
    netio_msg.error_message = msg.error_message;
    netio_msg.timestamp = msg.timestamp;
    
    // 根据消息内容确定类型
    if (!msg.id.empty() && msg.status_code == 200) {
        netio_msg.type = netio::MessageType::RESPONSE;
    } else if (!msg.id.empty()) {
        netio_msg.type = netio::MessageType::REQUEST;
    } else {
        netio_msg.type = netio::MessageType::NOTIFICATION;
    }
    
    return netio_msg;
}

TransportMessage TransportBase::fromNetioMessage(const netio::Message& msg) {
    TransportMessage transport_msg;
    transport_msg.id = msg.id;
    transport_msg.content = msg.body;
    transport_msg.method = msg.method;
    transport_msg.path = msg.path;
    transport_msg.headers = msg.headers;
    transport_msg.status_code = msg.status_code;
    transport_msg.error_message = msg.error_message;
    transport_msg.timestamp = msg.timestamp;
    
    return transport_msg;
}

void TransportBase::handleNetIOServerRequest(const netio::Message& request) {
    if (sse_server_handler_) {
        netio::Message response;
        bool should_respond = sse_server_handler_->handleRequest(request, response);
        
        if (should_respond && netio_) {
            netio_->sendRequestSync(response);
        }
    }
}

void TransportBase::startNetIOReceiveThread() {
    if (receive_thread_.joinable()) {
        if (getLogger()) {
            getLogger()->warning("Receive thread already running");
        }
        return;
    }
    
    receive_thread_ = std::thread(&TransportBase::netIOReceiveThreadFunc, this);
}

void TransportBase::stopNetIOReceiveThread() {
    if (receive_thread_.joinable()) {
        queue_cv_.notify_all();
        receive_thread_.join();
    }
}

void TransportBase::netIOReceiveThreadFunc() {
    // NetIO通过回调接收消息，这里主要用于连接监控和重连
    while (running_.load()) {
        // 检查连接状态
        if (netio_ && !netio_->isConnected() && config_.mode == TransportMode::CLIENT) {
            // 如果连接断开，尝试重连
            if (config_.sse.auto_reconnect) {
                std::this_thread::sleep_for(std::chrono::seconds(config_.sse.reconnect_interval));
                startNetIOClient();
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void TransportBase::configureNetIO(netio::NetConfig& net_config) {
    // 如果是HTTPS，从TransportConfig中读取证书信息并设置到NetConfig
    if (config_.type == TransportType::HTTPS) {
        net_config.cert_file = config_.https.cert_file;
        net_config.key_file = config_.https.key_file;
        net_config.ca_file = config_.https.ca_file;
    }
    // 子类可以覆盖以添加更多特定配置
}

} // namespace mcp_transport

