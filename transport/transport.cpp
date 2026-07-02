#include "transport.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <thread>
#include <filesystem>

namespace mcp_transport {

// TransportMessage 实现
nlohmann::json TransportMessage::to_json() const {
    nlohmann::json j;
    j["id"] = id;
    j["content"] = content;
    j["method"] = method;
    j["path"] = path;
    j["headers"] = headers;
    j["status_code"] = status_code;
    j["error_message"] = error_message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    return j;
}

TransportMessage TransportMessage::from_json(const nlohmann::json& j) {
    TransportMessage msg;
    msg.id = j.value("id", "");
    msg.content = j.value("content", "");
    msg.method = j.value("method", "POST");
    msg.path = j.value("path", "/");
    msg.status_code = j.value("status_code", 200);
    msg.error_message = j.value("error_message", "");
    
    if (j.contains("headers") && j["headers"].is_object()) {
        for (auto& [key, value] : j["headers"].items()) {
            msg.headers[key] = value.get<std::string>();
        }
    }
    
    if (j.contains("timestamp")) {
        auto timestamp_ms = j["timestamp"].get<uint64_t>();
        msg.timestamp = std::chrono::system_clock::time_point(
            std::chrono::milliseconds(timestamp_ms));
    } else {
        msg.timestamp = std::chrono::system_clock::now();
    }
    
    return msg;
}

TransportMessage TransportMessage::from_ipc_message(const mcp::Message& msg) {
    TransportMessage transport_msg;
    transport_msg.id = msg.id;
    transport_msg.content = msg.content;
    transport_msg.method = "POST"; // IPC 默认使用 POST
    transport_msg.path = "/";
    transport_msg.timestamp = std::chrono::system_clock::now();
    return transport_msg;
}

TransportMessage TransportMessage::from_netio_message(const netio::Message& msg) {
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

mcp::Message TransportMessage::to_ipc_message() const {
    mcp::Message ipc_msg;
    ipc_msg.id = id;
    ipc_msg.content = content;
    ipc_msg.type = mcp::MessageType::REQUEST;
    return ipc_msg;
}

netio::Message TransportMessage::to_netio_message() const {
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

// ConnectionInfo 实现
nlohmann::json ConnectionInfo::to_json() const {
    nlohmann::json j;
    j["connection_id"] = connection_id;
    j["remote_address"] = remote_address;
    j["remote_port"] = remote_port;
    j["local_address"] = local_address;
    j["local_port"] = local_port;
    j["status"] = static_cast<int>(status);
    j["type"] = static_cast<int>(type);
    j["connected_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        connected_at.time_since_epoch()).count();
    j["last_activity"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        last_activity.time_since_epoch()).count();
    return j;
}

// TransportConfig 实现
TransportConfig TransportConfig::from_json_file(const std::string& file_path) {
    try {
        std::ifstream file(file_path);
        if (!file.is_open()) {
            throw TransportError("Cannot open config file: " + file_path);
        }
        
        nlohmann::json j;
        file >> j;
        return from_json(j);
    } catch (const std::exception& e) {
        throw TransportError("Failed to load config from file: " + std::string(e.what()));
    }
}

TransportConfig TransportConfig::from_json(const nlohmann::json& j) {
    TransportConfig config;
    
    // 基本配置
    if (j.contains("type")) {
        config.type = static_cast<TransportType>(j["type"].get<int>());
    }
    if (j.contains("name")) {
        config.name = j["name"].get<std::string>();
    }
    
    // IPC 配置
    if (j.contains("ipc")) {
        const auto& ipc_config = j["ipc"];
        if (ipc_config.contains("command")) {
            config.ipc.command = ipc_config["command"].get<std::string>();
        }
        if (ipc_config.contains("args") && ipc_config["args"].is_array()) {
            for (const auto& arg : ipc_config["args"]) {
                config.ipc.args.push_back(arg.get<std::string>());
            }
        }
        if (ipc_config.contains("env_vars") && ipc_config["env_vars"].is_object()) {
            for (auto& [key, value] : ipc_config["env_vars"].items()) {
                config.ipc.env_vars[key] = value.get<std::string>();
            }
        }
        if (ipc_config.contains("buffer_size")) {
            config.ipc.buffer_size = ipc_config["buffer_size"].get<int>();
        }
        if (ipc_config.contains("read_timeout_ms")) {
            config.ipc.read_timeout_ms = ipc_config["read_timeout_ms"].get<int>();
        }
        if (ipc_config.contains("max_retry_count")) {
            config.ipc.max_retry_count = ipc_config["max_retry_count"].get<int>();
        }
        if (ipc_config.contains("enable_logging")) {
            config.ipc.enable_logging = ipc_config["enable_logging"].get<bool>();
        }
        if (ipc_config.contains("log_file_path")) {
            config.ipc.log_file_path = ipc_config["log_file_path"].get<std::string>();
        }
        if (ipc_config.contains("log_level")) {
            config.ipc.log_level = static_cast<mcp::LogLevel>(ipc_config["log_level"].get<int>());
        }
    }
    
    // SSE 配置
    if (j.contains("sse")) {
        const auto& sse_config = j["sse"];
        if (sse_config.contains("host")) {
            config.sse.host = sse_config["host"].get<std::string>();
        }
        if (sse_config.contains("port")) {
            config.sse.port = sse_config["port"].get<int>();
        }
        if (sse_config.contains("protocol")) {
            config.sse.protocol = static_cast<netio::Protocol>(sse_config["protocol"].get<int>());
        }
        if (sse_config.contains("timeout_seconds")) {
            config.sse.timeout_seconds = sse_config["timeout_seconds"].get<int>();
        }
        if (sse_config.contains("max_retries")) {
            config.sse.max_retries = sse_config["max_retries"].get<int>();
        }
        if (sse_config.contains("headers") && sse_config["headers"].is_object()) {
            for (auto& [key, value] : sse_config["headers"].items()) {
                config.sse.headers[key] = value.get<std::string>();
            }
        }
        if (sse_config.contains("user_agent")) {
            config.sse.user_agent = sse_config["user_agent"].get<std::string>();
        }
        if (sse_config.contains("keep_alive")) {
            config.sse.keep_alive = sse_config["keep_alive"].get<bool>();
        }
        if (sse_config.contains("auto_reconnect")) {
            config.sse.auto_reconnect = sse_config["auto_reconnect"].get<bool>();
        }
        if (sse_config.contains("reconnect_interval")) {
            config.sse.reconnect_interval = sse_config["reconnect_interval"].get<int>();
        }
    }
    
    // 通用配置
    if (j.contains("enable_logging")) {
        config.enable_logging = j["enable_logging"].get<bool>();
    }
    if (j.contains("log_file_path")) {
        config.log_file_path = j["log_file_path"].get<std::string>();
    }
    if (j.contains("log_level")) {
        config.log_level = j["log_level"].get<int>();
    }
    
    return config;
}

nlohmann::json TransportConfig::to_json() const {
    nlohmann::json j;
    j["type"] = static_cast<int>(type);
    j["name"] = name;
    
    // IPC 配置
    nlohmann::json ipc_config;
    ipc_config["command"] = ipc.command;
    ipc_config["args"] = ipc.args;
    ipc_config["env_vars"] = ipc.env_vars;
    ipc_config["buffer_size"] = ipc.buffer_size;
    ipc_config["read_timeout_ms"] = ipc.read_timeout_ms;
    ipc_config["max_retry_count"] = ipc.max_retry_count;
    ipc_config["enable_logging"] = ipc.enable_logging;
    ipc_config["log_file_path"] = ipc.log_file_path;
    ipc_config["log_level"] = static_cast<int>(ipc.log_level);
    j["ipc"] = ipc_config;
    
    // SSE 配置
    nlohmann::json sse_config;
    sse_config["host"] = sse.host;
    sse_config["port"] = sse.port;
    sse_config["protocol"] = static_cast<int>(sse.protocol);
    sse_config["timeout_seconds"] = sse.timeout_seconds;
    sse_config["max_retries"] = sse.max_retries;
    sse_config["headers"] = sse.headers;
    sse_config["user_agent"] = sse.user_agent;
    sse_config["keep_alive"] = sse.keep_alive;
    sse_config["auto_reconnect"] = sse.auto_reconnect;
    sse_config["reconnect_interval"] = sse.reconnect_interval;
    j["sse"] = sse_config;
    
    // 通用配置
    j["enable_logging"] = enable_logging;
    j["log_file_path"] = log_file_path;
    j["log_level"] = log_level;
    
    return j;
}

bool TransportConfig::validate() const {
    if (type == TransportType::IPC) {
        return !ipc.command.empty() && ipc.buffer_size > 0 && ipc.read_timeout_ms > 0;
    } else if (type == TransportType::SSE) {
        return !sse.host.empty() && sse.port > 0 && sse.port <= 65535 && sse.timeout_seconds > 0;
    } else if (type == TransportType::AUTO) {
        return true; // 自动选择，配置验证在运行时进行
    }
    return false;
}

// Transport 内部实现类
class Transport::Impl {
public:
    // TransportMessageHandler 用于 IPC 消息处理
    class TransportMessageHandler : public mcp::MessageHandler {
    public:
        explicit TransportMessageHandler(Impl* impl) : impl_(impl) {}
        
        void on_message(const mcp::Message& message) override {
            if (impl_ && impl_->message_callback_) {
                TransportMessage transport_msg = TransportMessage::from_ipc_message(message);
                impl_->message_callback_(transport_msg);
            }
        }
        
        void on_error(const std::string& error) override {
            if (impl_ && impl_->error_callback_) {
                impl_->error_callback_(error);
            }
        }
        
    private:
        Impl* impl_;
    };

    Impl() : 
        status_(TransportStatus::DISCONNECTED),
        type_(TransportType::AUTO),
        config_(),
        connection_id_(generate_connection_id()),
        statistics_(),
        logging_enabled_(true),
        log_level_(1),
        log_file_(),
        log_mutex_() {
    }

    ~Impl() {
        stop();
    }

    bool start(const TransportConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            config_ = config;
            logging_enabled_ = config.enable_logging;
            log_level_ = config.log_level;
            
            // 初始化日志
            if (logging_enabled_ && !config.log_file_path.empty()) {
                log_file_.open(config.log_file_path, std::ios::app);
            }
            
            // 确定传输类型
            TransportType actual_type = config.type;
            if (actual_type == TransportType::AUTO) {
                // 自动选择：优先使用 IPC，如果 IPC 配置不完整则使用 HTTP
                if (!config.ipc.command.empty()) {
                    actual_type = TransportType::IPC;
                } else if (!config.sse.host.empty()) {
                    actual_type = TransportType::SSE;
                } else {
                    throw TransportError("Cannot determine transport type: both IPC and SSE configs are incomplete");
                }
            }
            
            type_ = actual_type;
            status_ = TransportStatus::CONNECTING;
            //TRANSPORT_LOG_INFO("选择的传输类型: " + log_level_to_string(static_cast<int>(actual_type)));
            //TRANSPORT_LOG_INFO("Starting transport with type: " + std::to_string(static_cast<int>(actual_type)));
            bool success = false;
            if (actual_type == TransportType::IPC) {
                TRANSPORT_LOG_INFO("开始启动 IPC 传输");
                success = start_ipc();
            } else if (actual_type == TransportType::SSE) {
                TRANSPORT_LOG_INFO("开始启动 SSE 传输");
                success = start_sse();
            }
            
            if (success) {
                status_ = TransportStatus::CONNECTED;
                connection_info_.connection_id = connection_id_;
                connection_info_.type = actual_type;
                connection_info_.status = status_;
                connection_info_.connected_at = std::chrono::system_clock::now();
                connection_info_.last_activity = connection_info_.connected_at;
                
                update_statistics("connections", 1);
                
                if (connection_callback_) {
                    connection_callback_(connection_info_);
                }
                
                TRANSPORT_LOG_INFO("Transport started successfully");
                return true;
            } else {
                status_ = TransportStatus::ERROR;
                TRANSPORT_LOG_ERROR("Failed to start transport");
                return false;
            }
        } catch (const std::exception& e) {
            status_ = TransportStatus::ERROR;
            TRANSPORT_LOG_ERROR("Transport start error: " + std::string(e.what()));
            if (error_callback_) {
                error_callback_("Transport start error: " + std::string(e.what()));
            }
            return false;
        }
    }

    bool stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (status_ == TransportStatus::DISCONNECTED) {
            return true;
        }
        
        try {
            if (type_ == TransportType::IPC && ipc_comm_) {
                ipc_comm_->stop();
                ipc_comm_.reset();
            } else if (type_ == TransportType::SSE && netio_client_) {
                netio_client_->disconnect();
                netio_client_.reset();
            }
            
            status_ = TransportStatus::DISCONNECTED;
            connection_info_.status = status_;
            
            if (log_file_.is_open()) {
                log_file_.close();
            }
            
            TRANSPORT_LOG_INFO("Transport stopped");
            return true;
        } catch (const std::exception& e) {
            TRANSPORT_LOG_ERROR("Error stopping transport: " + std::string(e.what()));
            return false;
        }
    }

    bool is_running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_ == TransportStatus::CONNECTED;
    }

    TransportStatus get_status() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_;
    }

    ConnectionInfo get_connection_info() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_info_;
    }

    std::future<TransportMessage> send_request(const TransportMessage& message) {
        return std::async(std::launch::async, [this, message]() {
            return send_request_impl(message);
        });
    }

    std::future<TransportMessage> send_request(const std::string& content, 
                                              const std::string& method,
                                              const std::string& path,
                                              const std::map<std::string, std::string>& headers) {
        TransportMessage msg = utils::create_request(content, method, path, headers);
        return send_request(msg);
    }

    TransportMessage send_request_sync(const TransportMessage& message) {
        return send_request_impl(message);
    }

    TransportMessage send_request_sync(const std::string& content, 
                                     const std::string& method,
                                     const std::string& path,
                                     const std::map<std::string, std::string>& headers) {
        TransportMessage msg = utils::create_request(content, method, path, headers);
        return send_request_sync(msg);
    }

    bool has_message() const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (type_ == TransportType::IPC && ipc_comm_) {
            return ipc_comm_->has_message();
        }
        return false; // SSE 模式不支持消息队列
    }

    TransportMessage get_message() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (type_ == TransportType::IPC && ipc_comm_) {
            mcp::Message ipc_msg = ipc_comm_->get_message();
            return TransportMessage::from_ipc_message(ipc_msg);
        }
        return TransportMessage();
    }

    std::vector<TransportMessage> get_all_messages() {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<TransportMessage> messages;
        if (type_ == TransportType::IPC && ipc_comm_) {
            std::vector<mcp::Message> ipc_messages = ipc_comm_->get_all_messages();
            for (const auto& ipc_msg : ipc_messages) {
                messages.push_back(TransportMessage::from_ipc_message(ipc_msg));
            }
        }
        return messages;
    }

    void set_message_callback(MessageCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        message_callback_ = callback;
    }

    void set_connection_callback(ConnectionCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_callback_ = callback;
    }

    void set_error_callback(ErrorCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_callback_ = callback;
    }

    void update_config(const TransportConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }

    TransportConfig get_config() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    nlohmann::json get_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return statistics_;
    }

    bool health_check() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (type_ == TransportType::IPC && ipc_comm_) {
            return ipc_comm_->is_running() && ipc_comm_->is_process_alive();
        } else if (type_ == TransportType::SSE && netio_client_) {
            return netio_client_->is_connected() && netio_client_->health_check();
        }
        return false;
    }

    // 旧的log函数已废弃，现在使用统一的log模块
    // 保留此函数以保持向后兼容性，但实际使用TRANSPORT_LOG_*宏
    void log(int level, const std::string& message) {
        // 将旧的日志级别映射到新的日志级别
        switch (level) {
            case 0: TRANSPORT_LOG_DEBUG(message); break;
            case 1: TRANSPORT_LOG_INFO(message); break;
            case 2: TRANSPORT_LOG_WARNING(message); break;
            case 3: TRANSPORT_LOG_ERROR(message); break;
            case 4: TRANSPORT_LOG_FATAL(message); break;
            default: TRANSPORT_LOG_INFO(message); break;
        }
    }

    void set_log_level(int level) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        log_level_ = level;
        TRANSPORT_LOG_INFO("Log level changed to " + log_level_to_string(level));
    }

    void set_log_file(const std::string& file_path) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        
        if (log_file_.is_open()) {
            log_file_.close();
        }
        
        if (!file_path.empty()) {
            log_file_.open(file_path, std::ios::app);
            if (log_file_.is_open()) {
                TRANSPORT_LOG_INFO("Log file opened: " + file_path);
            } else {
                TRANSPORT_LOG_ERROR("Failed to open log file: " + file_path);
            }
        }
    }

    void enable_logging(bool enable) {
        std::lock_guard<std::mutex> lock(log_mutex_);
        logging_enabled_ = enable;
        if (enable) {
            TRANSPORT_LOG_INFO("Logging enabled");
        }
    }

private:
    mutable std::mutex mutex_;
    TransportStatus status_;
    TransportType type_;
    TransportConfig config_;
    std::string connection_id_;
    ConnectionInfo connection_info_;
    nlohmann::json statistics_;
    
    // IPC 相关
    std::unique_ptr<mcp::PipeCommunication> ipc_comm_;
    
    // SSE 相关
    std::unique_ptr<netio::NetIO> netio_client_;
    
    // 回调函数
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;
    
    // 日志相关
    bool logging_enabled_;
    int log_level_;
    std::ofstream log_file_;
    mutable std::mutex log_mutex_;

    bool start_ipc() {
        try {
            ipc_comm_ = std::make_unique<mcp::PipeCommunication>();
            
            // 设置消息处理器
            if (message_callback_) {
                auto handler = std::make_shared<TransportMessageHandler>(this);
                ipc_comm_->set_message_handler(handler);
            }
            // 配置 IPC
            mcp::PipeConfig ipc_config;
            ipc_config.command = config_.ipc.command;
            ipc_config.args = config_.ipc.args;
            ipc_config.env_vars = config_.ipc.env_vars;
            ipc_config.buffer_size = config_.ipc.buffer_size;
            ipc_config.read_timeout_ms = config_.ipc.read_timeout_ms;
            ipc_config.max_retry_count = config_.ipc.max_retry_count;
            ipc_config.enable_logging = config_.ipc.enable_logging;
            ipc_config.log_file_path = config_.ipc.log_file_path;
            ipc_config.log_level = config_.ipc.log_level;
            
            TRANSPORT_LOG_INFO("IPC 配置: 命令=" + ipc_config.command + 
                ", 参数数量=" + std::to_string(ipc_config.args.size()) +
                ", 缓冲区大小=" + std::to_string(ipc_config.buffer_size) +
                ", 超时=" + std::to_string(ipc_config.read_timeout_ms) + "ms" +
                ", 重试次数=" + std::to_string(ipc_config.max_retry_count));
            
            TRANSPORT_LOG_INFO("IPC 配置详情:");
            TRANSPORT_LOG_INFO("  命令: " + ipc_config.command);
            std::string args_str = "  参数: ";
            for (const auto& arg : ipc_config.args) {
                args_str += arg + " ";
            }
            TRANSPORT_LOG_INFO(args_str);
            TRANSPORT_LOG_INFO("  缓冲区大小: " + std::to_string(ipc_config.buffer_size));
            TRANSPORT_LOG_INFO("  超时: " + std::to_string(ipc_config.read_timeout_ms) + "ms");
            TRANSPORT_LOG_INFO("  重试次数: " + std::to_string(ipc_config.max_retry_count));
            
            TRANSPORT_LOG_INFO("开始启动 IPC 通信...");
            if (!ipc_comm_->start(ipc_config)) {
                TRANSPORT_LOG_ERROR("Failed to start IPC communication");
                TRANSPORT_LOG_ERROR("IPC 启动失败！");
                return false;
            }
            TRANSPORT_LOG_INFO("IPC 启动成功！");
            
            connection_info_.remote_address = "localhost";
            connection_info_.remote_port = 0;
            connection_info_.local_address = "localhost";
            connection_info_.local_port = 0;
            
            TRANSPORT_LOG_INFO("IPC communication started successfully");
            return true;
        } catch (const std::exception& e) {
            TRANSPORT_LOG_ERROR("IPC start error: " + std::string(e.what()));
            return false;
        }
    }

    bool start_sse() {
        try {
            netio_client_ = std::make_unique<netio::NetIO>();
            
            // 设置回调
            if (message_callback_) {
                netio_client_->set_message_callback([this](const netio::Message& msg) {
                    TransportMessage transport_msg = TransportMessage::from_netio_message(msg);
                    message_callback_(transport_msg);
                });
            }
            
            if (connection_callback_) {
                netio_client_->set_connection_callback([this](const netio::ConnectionInfo& info) {
                    connection_info_.connection_id = info.connection_id;
                    connection_info_.remote_address = info.remote_address;
                    connection_info_.remote_port = info.remote_port;
                    connection_info_.local_address = info.local_address;
                    connection_info_.local_port = info.local_port;
                    connection_info_.status = static_cast<TransportStatus>(static_cast<int>(info.status));
                    connection_info_.last_activity = info.last_activity;
                    connection_callback_(connection_info_);
                });
            }
            
            if (error_callback_) {
                netio_client_->set_error_callback(error_callback_);
            }
            
            // 配置 SSE
            netio::NetConfig netio_config;
            netio_config.host = config_.sse.host;
            netio_config.port = config_.sse.port;
            netio_config.protocol = config_.sse.protocol;
            netio_config.timeout_seconds = config_.sse.timeout_seconds;
            netio_config.max_retries = config_.sse.max_retries;
            netio_config.headers = config_.sse.headers;
            netio_config.user_agent = config_.sse.user_agent;
            netio_config.keep_alive = config_.sse.keep_alive;
            netio_config.auto_reconnect = config_.sse.auto_reconnect;
            netio_config.reconnect_interval = config_.sse.reconnect_interval;
            
            if (!netio_client_->connect(netio_config)) {
                TRANSPORT_LOG_ERROR("Failed to connect to SSE server");
                return false;
            }
            
            auto conn_info = netio_client_->get_connection_info();
            connection_info_.remote_address = conn_info.remote_address;
            connection_info_.remote_port = conn_info.remote_port;
            connection_info_.local_address = conn_info.local_address;
            connection_info_.local_port = conn_info.local_port;
            
            TRANSPORT_LOG_INFO("SSE connection established successfully");
            return true;
        } catch (const std::exception& e) {
            TRANSPORT_LOG_ERROR("SSE start error: " + std::string(e.what()));
            return false;
        }
    }

    TransportMessage send_request_impl(const TransportMessage& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (status_ != TransportStatus::CONNECTED) {
            return utils::create_error("Transport not connected", 500);
        }
        
        try {
            if (type_ == TransportType::IPC && ipc_comm_) {
                TRANSPORT_LOG_INFO("发送 IPC 请求: " + message.content.substr(0, 100) + (message.content.length() > 100 ? "..." : ""));
                
                mcp::Message ipc_msg = message.to_ipc_message();
                auto res_future = ipc_comm_->send_request(ipc_msg.content);
                
                // 等待响应
                auto timeout = std::chrono::seconds(1);
                TRANSPORT_LOG_INFO("等待 IPC 响应，超时时间: " + std::to_string(config_.sse.timeout_seconds) + " 秒");
                auto status = res_future.wait_for(timeout);
                //TRANSPORT_LOG_DEBUG("response_future response_future" + res_future.get());
                if (status == std::future_status::ready) {
                    std::string response_content = res_future.get();
                    TRANSPORT_LOG_INFO("收到 IPC 响应: " + response_content.substr(0, 100) + (response_content.length() > 100 ? "..." : ""));
                    
                    TransportMessage response;
                    response.id = message.id;
                    response.content = response_content;
                    response.status_code = 200;
                    response.timestamp = std::chrono::system_clock::now();
                    
                    connection_info_.last_activity = response.timestamp;
                    update_statistics("requests_sent", 1);
                    update_statistics("responses_received", 1);
                    
                    return response;
                } else {
                    TRANSPORT_LOG_ERROR("IPC 请求超时");
                    return utils::create_error("Request timeout", 408);
                }
            } else if (type_ == TransportType::SSE && netio_client_) {
                netio::Message netio_msg = message.to_netio_message();
                netio::Message response = netio_client_->send_request_sync(netio_msg);
                
                TransportMessage transport_response = TransportMessage::from_netio_message(response);
                transport_response.id = message.id; // 保持原始 ID
                
                connection_info_.last_activity = transport_response.timestamp;
                update_statistics("requests_sent", 1);
                update_statistics("responses_received", 1);
                
                return transport_response;
            } else {
                return utils::create_error("Invalid transport type", 500);
            }
        } catch (const std::exception& e) {
            TRANSPORT_LOG_ERROR("Send request error: " + std::string(e.what()));
            return utils::create_error("Send request error: " + std::string(e.what()), 500);
        }
    }

    std::string generate_connection_id() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        return "transport_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    }

    void update_statistics(const std::string& key, int increment) {
        if (statistics_.contains(key)) {
            statistics_[key] = statistics_[key].get<int>() + increment;
        } else {
            statistics_[key] = increment;
        }
        statistics_["last_updated"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string get_timestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
        ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
        
        return ss.str();
    }

    std::string log_level_to_string(int level) const {
        switch (level) {
            case 0: return "DEBUG";
            case 1: return "INFO";
            case 2: return "WARNING";
            case 3: return "ERROR";
            default: return "UNKNOWN";
        }
    }
};


// Transport 公共接口实现
Transport::Transport() : impl_(std::make_unique<Impl>()) {}

Transport::~Transport() = default;

bool Transport::start(const TransportConfig& config) {
    return impl_->start(config);
}

bool Transport::stop() {
    return impl_->stop();
}

bool Transport::is_running() const {
    return impl_->is_running();
}

TransportStatus Transport::get_status() const {
    return impl_->get_status();
}

ConnectionInfo Transport::get_connection_info() const {
    return impl_->get_connection_info();
}

std::future<TransportMessage> Transport::send_request(const TransportMessage& message) {
    return impl_->send_request(message);
}

std::future<TransportMessage> Transport::send_request(const std::string& content, 
                                                     const std::string& method,
                                                     const std::string& path,
                                                     const std::map<std::string, std::string>& headers) {
    return impl_->send_request(content, method, path, headers);
}

TransportMessage Transport::send_request_sync(const TransportMessage& message) {
    return impl_->send_request_sync(message);
}

TransportMessage Transport::send_request_sync(const std::string& content, 
                                            const std::string& method,
                                            const std::string& path,
                                            const std::map<std::string, std::string>& headers) {
    return impl_->send_request_sync(content, method, path, headers);
}

bool Transport::has_message() const {
    return impl_->has_message();
}

TransportMessage Transport::get_message() {
    return impl_->get_message();
}

std::vector<TransportMessage> Transport::get_all_messages() {
    return impl_->get_all_messages();
}

void Transport::set_message_callback(MessageCallback callback) {
    impl_->set_message_callback(callback);
}

void Transport::set_connection_callback(ConnectionCallback callback) {
    impl_->set_connection_callback(callback);
}

void Transport::set_error_callback(ErrorCallback callback) {
    impl_->set_error_callback(callback);
}

void Transport::update_config(const TransportConfig& config) {
    impl_->update_config(config);
}

TransportConfig Transport::get_config() const {
    return impl_->get_config();
}

nlohmann::json Transport::get_statistics() const {
    return impl_->get_statistics();
}

bool Transport::health_check() {
    return impl_->health_check();
}

// Transport::log函数已废弃，现在使用TRANSPORT_LOG_*宏
void Transport::log(int level, const std::string& message) {
    impl_->log(level, message);
}

void Transport::set_log_level(int level) {
    impl_->set_log_level(level);
}

void Transport::set_log_file(const std::string& file_path) {
    impl_->set_log_file(file_path);
}

void Transport::enable_logging(bool enable) {
    impl_->enable_logging(enable);
}

// 工具函数实现
namespace utils {

TransportConfig create_ipc_config(const std::string& command, 
                                const std::vector<std::string>& args,
                                const std::map<std::string, std::string>& env_vars) {
    TransportConfig config;
    config.type = TransportType::IPC;
    config.ipc.command = command;
    config.ipc.args = args;
    config.ipc.env_vars = env_vars;
    return config;
}

TransportConfig create_sse_config(const std::string& host, int port, 
                                 netio::Protocol protocol) {
    TransportConfig config;
    config.type = TransportType::SSE;
    config.sse.host = host;
    config.sse.port = port;
    config.sse.protocol = protocol;
    return config;
}

TransportConfig create_sse_secure_config(const std::string& host, int port) {
    return create_sse_config(host, port, netio::Protocol::HTTPS);
}

TransportMessage create_request(const std::string& content, 
                              const std::string& method,
                              const std::string& path,
                              const std::map<std::string, std::string>& headers) {
    TransportMessage msg;
    msg.id = "req_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    msg.content = content;
    msg.method = method;
    msg.path = path;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

TransportMessage create_response(const std::string& content, 
                               int status_code,
                               const std::map<std::string, std::string>& headers) {
    TransportMessage msg;
    msg.id = "resp_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    msg.content = content;
    msg.status_code = status_code;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

TransportMessage create_error(const std::string& error_message, 
                            int status_code,
                            const std::map<std::string, std::string>& headers) {
    TransportMessage msg;
    msg.id = "err_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    msg.content = error_message;
    msg.status_code = status_code;
    msg.error_message = error_message;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

bool validate_config(const TransportConfig& config) {
    return config.validate();
}

bool validate_message(const TransportMessage& message) {
    return !message.id.empty() && !message.content.empty();
}

std::string json_to_string(const nlohmann::json& j) {
    return j.dump(2);
}

nlohmann::json string_to_json(const std::string& s) {
    try {
        return nlohmann::json::parse(s);
    } catch (const std::exception& e) {
        return nlohmann::json::object();
    }
}

} // namespace utils

} // namespace mcp_transport
