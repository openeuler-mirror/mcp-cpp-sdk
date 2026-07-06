#include "netio.h"
#include "../../log/include/logger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>
#include <utility>
#include <optional>

namespace netio {

// Message实现
nlohmann::json Message::toJson() const {
    nlohmann::json j;
    j["id"] = id;
    j["type"] = static_cast<int>(type);
    j["method"] = method;
    j["path"] = path;
    j["body"] = body;
    j["headers"] = headers;
    j["status_code"] = status_code;
    j["error_message"] = error_message;
    j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        timestamp.time_since_epoch()).count();
    return j;
}

Message Message::fromJson(const nlohmann::json& j) {
    Message msg;
    msg.id = j.value("id", "");
    msg.type = static_cast<MessageType>(j.value("type", 0));
    msg.method = j.value("method", "");
    msg.path = j.value("path", "");
    msg.body = j.value("body", "");
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

// ConnectionInfo实现
nlohmann::json ConnectionInfo::toJson() const {
    nlohmann::json j;
    j["connection_id"] = connection_id;
    j["remote_address"] = remote_address;
    j["remote_port"] = remote_port;
    j["local_address"] = local_address;
    j["local_port"] = local_port;
    j["status"] = static_cast<int>(status);
    j["connected_at"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        connected_at.time_since_epoch()).count();
    j["last_activity"] = std::chrono::duration_cast<std::chrono::milliseconds>(
        last_activity.time_since_epoch()).count();
    j["session_id"] = session_id;
    return j;
}

// NetIO内部实现类
class NetIO::Impl {
public:
    Impl() : 
        client_(nullptr),
        server_(nullptr),
        status_(ConnectionStatus::DISCONNECTED),
        server_running_(false),
        config_(),
        connection_id_(generate_connection_id()),
        statistics_() {
    }

    ~Impl() {
        try {
            disconnect();
            
            // 确保服务器线程被正确清理
            if (server_thread_.joinable()) {
                if (server_) {
                    server_->stop();
                }
                server_thread_.join();
            }
            
            server_.reset();
            server_running_ = false;
        } catch (const std::exception& e) {
            // 析构函数中不能抛出异常，只能记录日志
            mcp_logger::getModuleLogger("netio")->error("Error in destructor: " + std::string(e.what()));
        }
    }

    bool connect(const NetConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        mcp_logger::getModuleLogger("netio")->info("Connecting to " + config.host + ":" + std::to_string(config.port) + " using " + (config.protocol == Protocol::HTTPS ? "HTTPS" : "HTTP"));
        
        try {
            config_ = config;
            
            // 根据协议创建客户端
            httplib::Result result;
            if (config.protocol == Protocol::HTTPS) {
                ssl_client_ = std::make_unique<httplib::SSLClient>(config.host, config.port);
                // 设置超时
                ssl_client_->set_connection_timeout(config.timeout_seconds);
                ssl_client_->set_read_timeout(config.timeout_seconds);
                ssl_client_->set_write_timeout(config.timeout_seconds);
                
                // 配置SSL证书验证
                if (!config.ca_file.empty()) {
                    // 如果提供了CA证书文件，使用它来验证服务器证书
                    ssl_client_->set_ca_cert_path(config.ca_file);
                    ssl_client_->enable_server_certificate_verification(true);
                    mcp_logger::getModuleLogger("netio")->info("Using CA certificate file for SSL verification: " + config.ca_file);
                } else {
                    // 如果没有提供CA证书文件，禁用证书验证（用于自签名证书测试）
                    ssl_client_->enable_server_certificate_verification(false);
                    mcp_logger::getModuleLogger("netio")->warning("SSL certificate verification disabled (no CA file provided). This is suitable for testing with self-signed certificates only.");
                }
                
                // 设置默认头部
                httplib::Headers default_headers;
                default_headers.insert({"User-Agent", config.user_agent});
                for (const auto& [key, value] : config.headers) {
                    default_headers.insert({key, value});
                }
                ssl_client_->set_default_headers(default_headers);
                
                mcp_logger::getModuleLogger("netio")->info("Sending GET request to / for connection test...");
                result = ssl_client_->Get("/mcp");
            } else {
                client_ = std::make_unique<httplib::Client>(config.host, config.port);
                // 设置超时
                client_->set_connection_timeout(config.timeout_seconds);
                client_->set_read_timeout(config.timeout_seconds);
                client_->set_write_timeout(config.timeout_seconds);
                
                // 设置默认头部
                httplib::Headers default_headers;
                default_headers.insert({"User-Agent", config.user_agent});
                for (const auto& [key, value] : config.headers) {
                    default_headers.insert({key, value});
                }
                client_->set_default_headers(default_headers);
                
                mcp_logger::getModuleLogger("netio")->info("Sending GET request to /mcp for connection test...");
                result = client_->Get("/mcp");
            }
            
            if (result) {
                mcp_logger::getModuleLogger("netio")->info("Connection result: " + result->body);

                for (const auto& [key, value] : result->headers) {
                    mcp_logger::getModuleLogger("netio")->info("Connection header - " + key + ": " + value);
                }
                
                status_ = ConnectionStatus::CONNECTED;
                connection_info_.connection_id = generate_connection_id();
                connection_info_.remote_address = config.host;
                connection_info_.remote_port = config.port;
                connection_info_.status = status_;
                connection_info_.connected_at = std::chrono::system_clock::now();
                connection_info_.last_activity = connection_info_.connected_at;
                if (result->has_header("X-Session-Id")) {
                    connection_info_.session_id = result->get_header_value("X-Session-Id");
                } else {
                    mcp_logger::getModuleLogger("netio")->info("No session id found in connection headers");
                    connection_info_.session_id = "";
                }
                
                updateStatistics("connections", 1);   
                mcp_logger::getModuleLogger("netio")->info("Successfully connected to " + config.host + ":" + std::to_string(config.port) + " with connection ID: " + connection_id_);
                
                if (connection_callback_) {
                    connection_callback_(connection_info_);
                }else{
                    mcp_logger::getModuleLogger("netio")->error("No connection callback found");
                }
                
                return true;
            } else {
                status_ = ConnectionStatus::ERROR;
                mcp_logger::getModuleLogger("netio")->error("Failed to connect to " + config.host + ":" + std::to_string(config.port));
                if (error_callback_) {
                    error_callback_("Failed to connect to " + config.host + ":" + std::to_string(config.port));
                }
                return false;
            }
        } catch (const std::exception& e) {
            status_ = ConnectionStatus::ERROR;
            mcp_logger::getModuleLogger("netio")->error("Connection error: " + std::string(e.what()));
            if (error_callback_) {
                error_callback_("Connection error: " + std::string(e.what()));
            }
            return false;
        }
    }

    bool disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        mcp_logger::getModuleLogger("netio")->info("Disconnecting from " + connection_info_.remote_address + ":" + std::to_string(connection_info_.remote_port));
        
        if (client_) {
            client_->stop();
            client_.reset();
        }
        
        if (ssl_client_) {
            ssl_client_->stop();
            ssl_client_.reset();
        }
        
        status_ = ConnectionStatus::DISCONNECTED;
        connection_info_.status = status_;
        
        mcp_logger::getModuleLogger("netio")->info("Successfully disconnected");
        
        return true;
    }

    bool isConnected() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_ == ConnectionStatus::CONNECTED && (client_ != nullptr || ssl_client_ != nullptr);
    }

    ConnectionInfo get_connection_info() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_info_;
    }

    std::future<Message> sendRequest(const Message& message) {
        return std::async(std::launch::async, [this, message]() {
            return sendRequest_impl(message);
        });
    }

    std::future<Message> sendRequest(const std::string& method, 
                                    const std::string& path, 
                                    const std::string& body,
                                    const std::map<std::string, std::string>& headers) {
        Message msg = utils::createRequest(method, path, body, headers);
        return sendRequest(msg);
    }

    Message sendRequest_sync(const Message& message) {
        return sendRequest_impl(message);
    }

    Message sendRequest_sync(const std::string& method, 
                            const std::string& path, 
                            const std::string& body,
                            const std::map<std::string, std::string>& headers) {
        Message msg = utils::createRequest(method, path, body, headers);
        return sendRequest_sync(msg);
    }

    bool startServer(const NetConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // 如果服务器已经在运行，先停止它
        if (server_running_ || server_thread_.joinable()) {
            mcp_logger::getModuleLogger("netio")->warning("Server is already running, stopping it first");
            stopServerUnsafe(); // 不获取锁，因为我们已经在锁中
        }
        
        std::string protocol_str = (config.protocol == Protocol::HTTPS) ? "HTTPS" : "HTTP";
        mcp_logger::getModuleLogger("netio")->info("Starting " + protocol_str + " server on " + config.host + ":" + std::to_string(config.port));
        
        try {
            config_ = config;
            
            // 根据协议类型创建相应的服务器
#ifdef CPPHTTPLIB_OPENSSL_SUPPORT
            if (config.protocol == Protocol::HTTPS) {
                // 检查证书和私钥文件是否提供
                if (config.cert_file.empty() || config.key_file.empty()) {
                    mcp_logger::getModuleLogger("netio")->error("HTTPS server requires cert_file and key_file to be set");
                    if (error_callback_) {
                        error_callback_("HTTPS server requires cert_file and key_file to be set");
                    }
                    return false;
                }
                
                // 创建 SSL 服务器
                const char* ca_file_ptr = config.ca_file.empty() ? nullptr : config.ca_file.c_str();
                const char* key_password_ptr = config.private_key_password.empty() ? nullptr : config.private_key_password.c_str();
                
                auto ssl_server = std::make_unique<httplib::SSLServer>(
                    config.cert_file.c_str(),
                    config.key_file.c_str(),
                    ca_file_ptr,
                    nullptr, // client_ca_cert_dir_path
                    key_password_ptr
                );
                
                // 验证 SSL 服务器是否有效
                if (!ssl_server->is_valid()) {
                    mcp_logger::getModuleLogger("netio")->error("Failed to create SSL server: invalid certificate or key file");
                    mcp_logger::getModuleLogger("netio")->error("Please check that cert_file and key_file are valid and accessible");
                    if (error_callback_) {
                        error_callback_("Failed to create SSL server: invalid certificate or key file");
                    }
                    return false;
                }
                
                // 将 SSL 服务器赋值给基类指针
                server_ = std::move(ssl_server);
                mcp_logger::getModuleLogger("netio")->info("SSL server created successfully with certificate: " + config.cert_file);
            } else {
                // 创建普通 HTTP 服务器
                server_ = std::make_unique<httplib::Server>();
            }
#else
            if (config.protocol == Protocol::HTTPS) {
                mcp_logger::getModuleLogger("netio")->error("HTTPS support is not available: OpenSSL support is not compiled");
                if (error_callback_) {
                    error_callback_("HTTPS support is not available: OpenSSL support is not compiled");
                }
                return false;
            }
            // 创建普通 HTTP 服务器
            server_ = std::make_unique<httplib::Server>();
#endif
            
            // 设置特定endpoint的处理器
            std::string endpoint = config.endpoint.empty() ? "/" : config.endpoint;
            mcp_logger::getModuleLogger("netio")->info("Setting up server endpoint: " + endpoint);
            
            // 这个接口只作为connect的接口使用，其他接口需要单独处理
            server_->Get(endpoint, [this](const httplib::Request& req, httplib::Response& res) {
                mcp_logger::getModuleLogger("netio")->info("Server received GET request: " + req.path);
                handleGet(req, res);
            });
            
            server_->Post(endpoint, [this](const httplib::Request& req, httplib::Response& res) {
                mcp_logger::getModuleLogger("netio")->info("Server received POST request: " + req.path);
                handleRequest(req, res);
            });
            
            server_->Put(endpoint, [this](const httplib::Request& req, httplib::Response& res) {
                mcp_logger::getModuleLogger("netio")->info("Server received PUT request: " + req.path);
                handleRequest(req, res);
            });
            
            server_->Delete(endpoint, [this](const httplib::Request& req, httplib::Response& res) {
                mcp_logger::getModuleLogger("netio")->info("Server received DELETE request: " + req.path);
                handleRequest(req, res);
            });
            
            // 添加OPTIONS方法处理器以支持CORS预检请求
            // 注意：NetIO 专门用于 HTTP/HTTPS 传输，因此总是需要处理 CORS 预检请求
            // IPC 传输使用 PipeCommunication，完全不经过 NetIO，所以不需要 CORS 处理
            server_->Options(endpoint, [this](const httplib::Request& req, httplib::Response& res) {
                mcp_logger::getModuleLogger("netio")->info("Server received OPTIONS request: " + req.path);
                handleOptions(req, res);
            });
            
            // 在单独线程中启动服务器以避免阻塞
            server_thread_ = std::thread([this, config, protocol_str]() {
                try {
                    mcp_logger::getModuleLogger("netio")->info("Server thread started, listening on " + protocol_str + "://" + config.host + ":" + std::to_string(config.port));
                    bool listen_result = server_->listen(config.host, config.port);
                    
                    // 更新服务器运行状态（使用 httplib 的 is_running 状态）
                    server_running_ = server_->is_running();
                    
                    if (listen_result && server_running_) {
                        updateStatistics("server_starts", 1);
                        mcp_logger::getModuleLogger("netio")->info(protocol_str + " server successfully started on " + config.host + ":" + std::to_string(config.port));
                    } else {
                        server_running_ = false;
                        mcp_logger::getModuleLogger("netio")->error(protocol_str + " server failed to listen on " + config.host + ":" + std::to_string(config.port));
                        mcp_logger::getModuleLogger("netio")->error("Possible reasons: port already in use, permission denied, or network error");
                    }
                } catch (const std::exception& e) {
                    server_running_ = false;
                    mcp_logger::getModuleLogger("netio")->error("Server thread error: " + std::string(e.what()));
                    if (error_callback_) {
                        error_callback_("Server thread error: " + std::string(e.what()));
                    }
                } catch (...) {
                    server_running_ = false;
                    mcp_logger::getModuleLogger("netio")->error("Server thread unknown error");
                    if (error_callback_) {
                        error_callback_("Server thread unknown error");
                    }
                }
            });
            
            // 等待服务器真正启动（使用 httplib 的 wait_until_ready）
            try {
                server_->wait_until_ready();
                // 额外等待一小段时间确保服务器完全启动
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                // 再次检查服务器状态
                server_running_ = server_->is_running();
            } catch (const std::exception& e) {
                mcp_logger::getModuleLogger("netio")->error("Error waiting for server to be ready: " + std::string(e.what()));
                server_running_ = false;
            }
            
            return true;
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("netio")->error("Server start error: " + std::string(e.what()));
            
            // 清理资源
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            server_.reset();
            server_running_ = false;
            
            if (error_callback_) {
                error_callback_("Server start error: " + std::string(e.what()));
            }
            return false;
        }
    }

    bool stopServer() {
        std::lock_guard<std::mutex> lock(mutex_);
        return stopServerUnsafe();
    }
    
    bool stopServerUnsafe() {
        mcp_logger::getModuleLogger("netio")->info("Stopping server");
        
        if (server_) {
            // 停止服务器
            server_->stop();
            
            // 等待服务器线程结束
            if (server_thread_.joinable()) {
                server_thread_.join();
            }
            
            server_.reset();
        }
        
        server_running_ = false;
        
        mcp_logger::getModuleLogger("netio")->info("Server stopped");
        
        return true;
    }

    bool is_server_running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        // 如果服务器对象存在，使用 httplib 的 is_running 方法
        if (server_) {
            return server_->is_running();
        }
        return server_running_;
    }

    void set_message_callback(MessageCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        message_callback_ = callback;
    }

    void set_server_request_handler(std::shared_ptr<ServerRequestHandler> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        server_request_handler_ = std::move(handler);
    }

    void set_connection_request_handler(std::shared_ptr<ConnectionRequestHandler> handler) {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_request_handler_ = std::move(handler);
    }

    void set_connection_callback(ConnectionCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        connection_callback_ = callback;
    }

    void set_error_callback(ErrorCallback callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        error_callback_ = callback;
    }

    void updateConfig(const NetConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }

    NetConfig getConfig() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    nlohmann::json getStatistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return statistics_;
    }

    bool healthCheck() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!client_ && !ssl_client_) {
            mcp_logger::getModuleLogger("netio")->warning("Health check failed: client not connected");
            return false;
        }
        
        try {
            httplib::Result result;
            if (ssl_client_) {
                result = ssl_client_->Get("/health");
            } else {
                result = client_->Get("/health");
            }
            bool healthy = result != nullptr && result->status == 200;
            mcp_logger::getModuleLogger("netio")->debug("Health check result: " + std::string(healthy ? "healthy" : "unhealthy"));
            return healthy;
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("netio")->error("Health check error: " + std::string(e.what()));
            return false;
        }
    }

private:
    mutable std::mutex mutex_;
    std::unique_ptr<httplib::Client> client_;
    std::unique_ptr<httplib::SSLClient> ssl_client_;
    std::unique_ptr<httplib::Server> server_;
    std::thread server_thread_;
    ConnectionStatus status_;
    bool server_running_;
    NetConfig config_;
    std::string connection_id_;
    ConnectionInfo connection_info_;
    nlohmann::json statistics_;
    
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;
    std::shared_ptr<ServerRequestHandler> server_request_handler_;
    std::shared_ptr<ConnectionRequestHandler> connection_request_handler_;

    std::shared_ptr<ServerRequestHandler> get_server_request_handler() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return server_request_handler_;
    }

    std::shared_ptr<ConnectionRequestHandler> get_connection_request_handler() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_request_handler_;
    }

    std::string generate_connection_id() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        return "conn_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    }

    Message sendRequest_impl(const Message& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        mcp_logger::getModuleLogger("netio")->debug("Sending request: " + message.method + " " + message.path + " (ID: " + message.id + ")");
        
        if (!client_ && !ssl_client_) {
            Message error_msg = utils::createError(500, "Client not connected");
            mcp_logger::getModuleLogger("netio")->error("Client not connected for request: " + message.id);
            return error_msg;
        }
        
        try {
            httplib::Headers headers;
            for (const auto& [key, value] : message.headers) {
                headers.insert({key, value});
            }
            
            httplib::Result result;
            
            if (message.method == "GET") {
                if (ssl_client_) {
                    result = ssl_client_->Get(message.path, headers);
                } else {
                    result = client_->Get(message.path, headers);
                }
            } else if (message.method == "POST") {
                if (ssl_client_) {
                    result = ssl_client_->Post(message.path, headers, message.body, "application/json");
                } else {
                    result = client_->Post(message.path, headers, message.body, "application/json");
                }
            } else if (message.method == "PUT") {
                if (ssl_client_) {
                    result = ssl_client_->Put(message.path, headers, message.body, "application/json");
                } else {
                    result = client_->Put(message.path, headers, message.body, "application/json");
                }
            } else if (message.method == "DELETE") {
                if (ssl_client_) {
                    result = ssl_client_->Delete(message.path, headers);
                } else {
                    result = client_->Delete(message.path, headers);
                }
            } else {
                Message error_msg = utils::createError(405, "Method not allowed: " + message.method);
                return error_msg;
            }
            
            if (result) {
                Message response;
                response.id = message.id;
                response.type = MessageType::RESPONSE;
                response.method = message.method;
                response.path = message.path;
                response.body = result->body;
                response.status_code = result->status;
                response.timestamp = std::chrono::system_clock::now();
                
                mcp_logger::getModuleLogger("netio")->info("sendRequest_impl Received headers:");
                for (const auto& [key, value] : result->headers) {
                    response.headers[key] = value;
                    mcp_logger::getModuleLogger("netio")->info("  " + key + ": " + value);
                }

                std::optional<std::string> content_type;
                if (auto it = result->headers.find("Content-Type"); it != result->headers.end()) {
                    content_type = it->second;
                    mcp_logger::getModuleLogger("netio")->debug("Response header Content-Type: " + *content_type);
                }
                
                connection_info_.last_activity = response.timestamp;
                updateStatistics("requests_sent", 1);
                updateStatistics("responses_received", 1);

                mcp_logger::getModuleLogger("netio")->debug("Received response: " + std::to_string(result->status) + " for request " + message.id);

                if (message_callback_) {
                    mcp_logger::getModuleLogger("netio")->debug("Calling message callback for response: " + message.id);
                    message_callback_(response);
                }
                
                return response;
            } else {
                Message error_msg = utils::createError(500, "Request failed");
                mcp_logger::getModuleLogger("netio")->error("Request failed for " + message.method + " " + message.path + " (ID: " + message.id + ")");
                return error_msg;
            }
        } catch (const std::exception& e) {
            Message error_msg = utils::createError(500, "Request error: " + std::string(e.what()));
            mcp_logger::getModuleLogger("netio")->error("Request error for " + message.method + " " + message.path + " (ID: " + message.id + "): " + std::string(e.what()));
            return error_msg;
        }
    }
    
    // 添加CORS头到响应
    // 注意：NetIO 专门用于 HTTP/HTTPS 传输，所有响应都需要添加 CORS 头
    // IPC 传输使用 PipeCommunication，不经过 NetIO，所以不需要调用此方法
    void addCorsHeaders(httplib::Response& res) {
        // 从config中读取CORS头并添加到响应
        for (const auto& [key, value] : config_.headers) {
            if (!key.empty() && !value.empty()) {
                // 确保CORS相关的头都被添加
                res.set_header(key.c_str(), value.c_str());
            }
        }
    }
    
    // 处理OPTIONS预检请求（CORS preflight）
    // 注意：此方法仅用于 HTTP/HTTPS 传输，IPC 传输不需要 CORS 预检
    void handleOptions(const httplib::Request& req, httplib::Response& res) {
        // 检查请求路径或查询参数中是否包含 stdio/ipc 传输类型
        // 如果客户端错误地尝试通过 HTTP 连接 stdio 传输，返回清晰的错误
        std::string path = req.path;
        std::string query_string = req.has_param("transportType") ? req.get_param_value("transportType") : "";
        
        // 检查查询参数或路径中是否包含 stdio/ipc
        bool is_stdio_request = false;
        if (!query_string.empty() && (query_string == "stdio" || query_string == "ipc" || query_string == "IPC")) {
            is_stdio_request = true;
        } else if (path.find("stdio") != std::string::npos || path.find("ipc") != std::string::npos) {
            is_stdio_request = true;
        }
        
        if (is_stdio_request) {
            mcp_logger::getModuleLogger("netio")->warning("⚠️ Client attempted CORS preflight for stdio/IPC transport via HTTP: " + path);
            res.status = 400;
            addCorsHeaders(res);
            nlohmann::json error_response;
            error_response["jsonrpc"] = "2.0";
            error_response["error"] = {
                {"code", -32600},
                {"message", "Invalid transport type: stdio/IPC transport cannot be accessed via HTTP. Use process communication instead."}
            };
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        mcp_logger::getModuleLogger("netio")->info("Handling CORS preflight request for: " + req.path);
        
        // 设置状态码
        res.status = 200;
        
        // 添加CORS头
        addCorsHeaders(res);
        
        // 确保OPTIONS响应包含必要的CORS头
        // 如果配置中没有，使用默认值
        if (!res.has_header("Access-Control-Allow-Origin")) {
            res.set_header("Access-Control-Allow-Origin", "*");
        }
        if (!res.has_header("Access-Control-Allow-Methods")) {
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        }
        if (!res.has_header("Access-Control-Allow-Headers")) {
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization, mcp-protocol-version, X-Session-Id");
        }
        if (!res.has_header("Access-Control-Expose-Headers")) {
            res.set_header("Access-Control-Expose-Headers", "X-Session-Id");
        }
        if (!res.has_header("Access-Control-Max-Age")) {
            res.set_header("Access-Control-Max-Age", "86400"); // 24小时
        }
        
        // OPTIONS请求通常不需要响应体
        res.set_content("", "text/plain");
        
        mcp_logger::getModuleLogger("netio")->debug("CORS preflight request handled successfully");
    }
    
    void handleGet(const httplib::Request& req, httplib::Response& res) {
        try {
            // 检查请求路径或查询参数中是否包含 stdio/ipc 传输类型
            // 如果客户端错误地尝试通过 HTTP 连接 stdio 传输，返回清晰的错误
            std::string path = req.path;
            std::string query_string = req.has_param("transportType") ? req.get_param_value("transportType") : "";
            
            // 检查查询参数或路径中是否包含 stdio/ipc
            bool is_stdio_request = false;
            if (!query_string.empty() && (query_string == "stdio" || query_string == "ipc" || query_string == "IPC")) {
                is_stdio_request = true;
            } else if (path.find("stdio") != std::string::npos || path.find("ipc") != std::string::npos) {
                is_stdio_request = true;
            }
            
            if (is_stdio_request) {
                mcp_logger::getModuleLogger("netio")->warning("⚠️ Client attempted to connect stdio/IPC transport via HTTP: " + path);
                res.status = 400;
                addCorsHeaders(res);
                nlohmann::json error_response;
                error_response["jsonrpc"] = "2.0";
                error_response["error"] = {
                    {"code", -32600},
                    {"message", "Invalid transport type: stdio/IPC transport cannot be accessed via HTTP. Use process communication instead."}
                };
                res.set_content(error_response.dump(), "application/json");
                return;
            }
            
            Message request;
            request.id = generate_connection_id();
            request.type = MessageType::REQUEST;
            request.method = req.method;
            request.path = req.path;
            request.body = req.body;
            request.timestamp = std::chrono::system_clock::now();
            
            for (const auto& [key, value] : req.headers) {
                request.headers[key] = value;
            }
            
            updateStatistics("requests_received", 1);
            
            auto handler = get_connection_request_handler();
            if (!handler) {
                mcp_logger::getModuleLogger("netio")->error("No connection request handler found");
                res.status = 404;
                // 即使错误响应也要添加CORS头
                addCorsHeaders(res);
                res.set_content("Not Found", "text/plain");
                return;
            }

            {
                Message response;
                response.type = MessageType::RESPONSE;
                response.id = request.id;
                response.timestamp = std::chrono::system_clock::now();
                
                bool need_response = false;
                try {
                    //  外层核心处理逻辑 请求转发
                    need_response = handler->handleRequest(request, response);
                } catch (const std::exception& e) {
                    nlohmann::json error_json;
                    error_json["jsonrpc"] = "2.0";
                    error_json["error"] = {
                        {"code", -32603},
                        {"message", std::string("Internal error: ") + e.what()}
                    };
                    
                    response.status_code = 500;
                    response.headers["Content-Type"] = "application/json; charset=utf-8";
                    response.body = error_json.dump();
                    need_response = true;  
                    mcp_logger::getModuleLogger("netio")->error("Server request handler exception: " + std::string(e.what()));
                } catch (...) {
                    nlohmann::json error_json;
                    error_json["jsonrpc"] = "2.0";
                    error_json["error"] = {
                        {"code", -32603},
                        {"message", "Internal error: Unknown exception"}
                    };
                    
                    response.status_code = 500;
                    response.headers["Content-Type"] = "application/json; charset=utf-8";
                    response.body = error_json.dump();
                    need_response = true;
                    mcp_logger::getModuleLogger("netio")->error("Server request handler unknown exception");
                }
                
                // 确保响应状态码有效
                int status_code = response.status_code != 0 ? response.status_code : (need_response ? 200 : 204);
                if (status_code < 100 || status_code >= 600) {
                    status_code = 500; // 无效状态码，使用500
                }
                res.status = status_code;
                
                std::string content_type = "application/json; charset=utf-8";
                auto content_type_it = response.headers.find("Content-Type");
                if (content_type_it != response.headers.end()) {
                    content_type = content_type_it->second;
                }
                
                // 设置header（先设置业务响应头）
                // 注意：不要设置Content-Length，httplib的set_content会自动设置
                for (const auto& [header_key, header_value] : response.headers) {
                    if (!header_key.empty() && !header_value.empty()) {
                        // 跳过Content-Length，让httplib自动设置
                        if (header_key != "Content-Length" && header_key != "content-length") {
                            res.set_header(header_key.c_str(), header_value.c_str());
                        }
                    }
                }
                
                // 添加CORS头（确保CORS头在所有响应中都存在）
                addCorsHeaders(res);
                
                // 设置body - 确保总是有内容，即使为空
                if (!response.body.empty()) {
                    res.set_content(response.body, content_type.c_str());
                } else if (need_response) {
                    // 如果需要响应但没有body，至少返回一个空JSON对象
                    res.set_content("{}", content_type.c_str());
                }

                // 打印出header包含的内容
                mcp_logger::getModuleLogger("netio")->debug("handleGet Response headers:");
                for (const auto& [header_key, header_value] : response.headers) {
                    mcp_logger::getModuleLogger("netio")->debug("  " + header_key + ": " + header_value);
                }
                
                mcp_logger::getModuleLogger("netio")->debug("Server responding with status " + std::to_string(res.status) + " for request: " + request.id);
                return;
            }
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("netio")->error("Server request handling error: " + std::string(e.what()));
            res.status = 500;
            // 即使错误响应也要添加CORS头
            addCorsHeaders(res);
            res.set_content("Internal Server Error", "text/plain");
        }
    }
    void handleRequest(const httplib::Request& req, httplib::Response& res) {
        try {
            // 检查请求路径或查询参数中是否包含 stdio/ipc 传输类型
            // 如果客户端错误地尝试通过 HTTP 连接 stdio 传输，返回清晰的错误
            std::string path = req.path;
            std::string query_string = req.has_param("transportType") ? req.get_param_value("transportType") : "";
            
            // 检查查询参数或路径中是否包含 stdio/ipc
            bool is_stdio_request = false;
            if (!query_string.empty() && (query_string == "stdio" || query_string == "ipc" || query_string == "IPC")) {
                is_stdio_request = true;
            } else if (path.find("stdio") != std::string::npos || path.find("ipc") != std::string::npos) {
                is_stdio_request = true;
            }
            
            if (is_stdio_request) {
                mcp_logger::getModuleLogger("netio")->warning("⚠️ Client attempted to connect stdio/IPC transport via HTTP: " + path);
                res.status = 400;
                addCorsHeaders(res);
                nlohmann::json error_response;
                error_response["jsonrpc"] = "2.0";
                error_response["error"] = {
                    {"code", -32600},
                    {"message", "Invalid transport type: stdio/IPC transport cannot be accessed via HTTP. Use process communication instead."}
                };
                res.set_content(error_response.dump(), "application/json");
                return;
            }
            
            Message request;
            request.id = generate_connection_id();
            request.type = MessageType::REQUEST;
            request.method = req.method;
            request.path = req.path;
            request.body = req.body;
            request.timestamp = std::chrono::system_clock::now();
            
            for (const auto& [key, value] : req.headers) {
                request.headers[key] = value;
            }
            
            // 检查请求头中是否有 sessionId（用于 SSE 连接）
            std::string session_id;
            auto session_id_it = req.headers.find("sessionId");
            if (session_id_it == req.headers.end()) {
                session_id_it = req.headers.find("SessionId");
            }
            if (session_id_it == req.headers.end()) {
                session_id_it = req.headers.find("X-Session-Id");
            }
            if (session_id_it != req.headers.end()) {
                session_id = session_id_it->second;
                mcp_logger::getModuleLogger("netio")->info("Received POST message for sessionId " + session_id);
            }
            
            mcp_logger::getModuleLogger("netio")->info("Server received request: " + req.method + " " + req.path + " (ID: " + request.id + ")");
            
            updateStatistics("requests_received", 1);
            
            auto handler = get_server_request_handler();
            if (handler) {
                Message response;
                response.type = MessageType::RESPONSE;
                response.id = request.id;
                response.timestamp = std::chrono::system_clock::now();
                
                bool need_response = false;
                try {
                    need_response = handler->handleRequest(request, response);
                } catch (const std::exception& e) {
                    nlohmann::json error_json;
                    error_json["jsonrpc"] = "2.0";
                    error_json["error"] = {
                        {"code", -32603},
                        {"message", std::string("Internal error: ") + e.what()}
                    };
                    
                    response.status_code = 500;
                    response.headers["Content-Type"] = "application/json";
                    response.body = error_json.dump();
                    need_response = true;
                    
                    mcp_logger::getModuleLogger("netio")->error("Server request handler exception: " + std::string(e.what()));
                }
                
                int status_code = response.status_code != 0 ? response.status_code : (need_response ? 200 : 204);
                res.status = status_code;
                
                std::string content_type = "application/json";
                auto content_type_it = response.headers.find("Content-Type");
                if (content_type_it != response.headers.end()) {
                    content_type = content_type_it->second;
                }

                // 设置header（先设置业务响应头）
                // 注意：不要设置Content-Length，httplib的set_content会自动设置
                for (const auto& [header_key, header_value] : response.headers) {
                    if (!header_key.empty() && !header_value.empty()) {
                        // 跳过Content-Length，让httplib自动设置
                        if (header_key != "Content-Length" && header_key != "content-length") {
                            res.set_header(header_key.c_str(), header_value.c_str());
                        }
                    }
                }
                
                // 如果有 sessionId，确保响应头中包含它（用于 SSE 连接管理）
                if (!session_id.empty() && !response.headers.count("X-Session-Id")) {
                    res.set_header("X-Session-Id", session_id.c_str());
                }
                
                // 添加CORS头（确保CORS头在所有响应中都存在）
                addCorsHeaders(res);
                
                // 确保响应体不为空（如果 need_response 为 true）
                if (!response.body.empty()) {
                    res.set_content(response.body, content_type.c_str());
                } else if (need_response) {
                    // 如果需要响应但没有body，至少返回一个空JSON对象
                    res.set_content("{}", content_type.c_str());
                }
                // 打印出header包含的内容
                mcp_logger::getModuleLogger("netio")->debug("handleRequest Response headers:");
                for (const auto& [header_key, header_value] : response.headers) {
                    mcp_logger::getModuleLogger("netio")->debug("  " + header_key + ": " + header_value);
                }
                mcp_logger::getModuleLogger("netio")->debug("Server responding with status " + std::to_string(res.status) + " for request: " + request.id);
                return;
            }
            
            if (message_callback_) {
                mcp_logger::getModuleLogger("netio")->debug("Calling message callback for server request: " + request.id);
                message_callback_(request);
            }
            
            // 默认响应
            res.status = 200;
            // 添加CORS头
            addCorsHeaders(res);
            res.set_content("OK", "text/plain");
            
            mcp_logger::getModuleLogger("netio")->debug("Server responding with status 200 for request: " + request.id);
            
        } catch (const std::exception& e) {
            mcp_logger::getModuleLogger("netio")->error("Server request handling error: " + std::string(e.what()));
            res.status = 500;
            // 即使错误响应也要添加CORS头
            addCorsHeaders(res);
            res.set_content("Internal Server Error", "text/plain");
        }
    }

    void updateStatistics(const std::string& key, int increment) {
        if (statistics_.contains(key)) {
            statistics_[key] = statistics_[key].get<int>() + increment;
        } else {
            statistics_[key] = increment;
        }
        statistics_["last_updated"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
};

// NetIO公共接口实现
NetIO::NetIO() : impl_(std::make_unique<Impl>()) {}

NetIO::~NetIO() = default;

bool NetIO::connect(const NetConfig& config) {
    return impl_->connect(config);
}

bool NetIO::disconnect() {
    return impl_->disconnect();
}

bool NetIO::isConnected() const {
    return impl_->isConnected();
}

ConnectionInfo NetIO::getConnectionInfo() const {
    return impl_->get_connection_info();
}

std::future<Message> NetIO::sendRequest(const Message& message) {
    return impl_->sendRequest(message);
}

std::future<Message> NetIO::sendRequest(const std::string& method, 
                                        const std::string& path, 
                                        const std::string& body,
                                        const std::map<std::string, std::string>& headers) {
    return impl_->sendRequest(method, path, body, headers);
}

Message NetIO::sendRequestSync(const Message& message) {
    return impl_->sendRequest_sync(message);
}

Message NetIO::sendRequestSync(const std::string& method, 
                                const std::string& path, 
                                const std::string& body,
                                const std::map<std::string, std::string>& headers) {
    return impl_->sendRequest_sync(method, path, body, headers);
}

bool NetIO::startServer(const NetConfig& config) {
    return impl_->startServer(config);
}

bool NetIO::stopServer() {
    return impl_->stopServer();
}

bool NetIO::isServerRunning() const {
    return impl_->is_server_running();
}

void NetIO::setMessageCallback(MessageCallback callback) {
    impl_->set_message_callback(callback);
}

void NetIO::setServerRequestHandler(std::shared_ptr<ServerRequestHandler> handler) {
    impl_->set_server_request_handler(std::move(handler));
}

void NetIO::setConnectionRequestHandler(std::shared_ptr<ConnectionRequestHandler> handler) {
    impl_->set_connection_request_handler(std::move(handler));
}

void NetIO::setConnectionCallback(ConnectionCallback callback) {
    impl_->set_connection_callback(callback);
}

void NetIO::setErrorCallback(ErrorCallback callback) {
    impl_->set_error_callback(callback);
}

void NetIO::updateConfig(const NetConfig& config) {
    impl_->updateConfig(config);
}

NetConfig NetIO::getConfig() const {
    return impl_->getConfig();
}

nlohmann::json NetIO::getStatistics() const {
    return impl_->getStatistics();
}

bool NetIO::healthCheck() {
    return impl_->healthCheck();
}

// 工具函数实现
namespace utils {

std::string jsonToString(const nlohmann::json& j) {
    return j.dump(2);
}

nlohmann::json stringToJson(const std::string& s) {
    try {
        return nlohmann::json::parse(s);
    } catch (const std::exception& e) {
        return nlohmann::json::object();
    }
}

Message createRequest(const std::string& method, 
                      const std::string& path, 
                      const std::string& body,
                      const std::map<std::string, std::string>& headers) {
    Message msg;
    msg.id = "req_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    msg.type = MessageType::REQUEST;
    msg.method = method;
    msg.path = path;
    msg.body = body;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

Message createResponse(int status_code, 
                      const std::string& body,
                      const std::map<std::string, std::string>& headers) {
    Message msg;
    msg.id = "resp_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    msg.type = MessageType::RESPONSE;
    msg.status_code = status_code;
    msg.body = body;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

Message createError(int status_code, 
                    const std::string& error_message,
                    const std::map<std::string, std::string>& headers) {
    Message msg;
    msg.id = "err_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    msg.type = MessageType::ERROR;
    msg.status_code = status_code;
    msg.error_message = error_message;
    msg.headers = headers;
    msg.timestamp = std::chrono::system_clock::now();
    return msg;
}

NetConfig createHttpConfig(const std::string& host, int port) {
    NetConfig config;
    config.host = host;
    config.port = port;
    config.protocol = Protocol::HTTP;
    return config;
}

NetConfig createHttpsConfig(const std::string& host, int port) {
    NetConfig config;
    config.host = host;
    config.port = port;
    config.protocol = Protocol::HTTPS;
    return config;
}

bool validateConfig(const NetConfig& config) {
    return !config.host.empty() && 
           config.port > 0 && 
           config.port <= 65535 &&
           config.timeout_seconds > 0;
}

bool validateMessage(const Message& message) {
    return !message.id.empty() && 
           !message.method.empty() && 
           !message.path.empty();
}

} // namespace utils

// NetConfig实现
nlohmann::json NetConfig::toJson() const {
    nlohmann::json j;
    j["host"] = host;
    j["port"] = port;
    j["protocol"] = static_cast<int>(protocol);
    j["timeout_seconds"] = timeout_seconds;
    j["max_retries"] = max_retries;
    j["headers"] = headers;
    j["user_agent"] = user_agent;
    j["keep_alive"] = keep_alive;
    j["auto_reconnect"] = auto_reconnect;
    j["reconnect_interval"] = reconnect_interval;
    j["endpoint"] = endpoint;
    j["cert_file"] = cert_file;
    j["key_file"] = key_file;
    j["ca_file"] = ca_file;
    return j;
}

} // namespace netio
