#include "netio.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <random>
#include <chrono>

namespace netio {

// Message实现
nlohmann::json Message::to_json() const {
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

Message Message::from_json(const nlohmann::json& j) {
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
nlohmann::json ConnectionInfo::to_json() const {
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
        disconnect();
        stop_server();
    }

    bool connect(const NetConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
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
                
                // 设置默认头部
                httplib::Headers default_headers;
                default_headers.insert({"User-Agent", config.user_agent});
                for (const auto& [key, value] : config.headers) {
                    default_headers.insert({key, value});
                }
                ssl_client_->set_default_headers(default_headers);
                
                // 测试连接
                result = ssl_client_->Get("/");
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
                
                // 测试连接
                result = client_->Get("/");
            }
            
            if (result) {
                status_ = ConnectionStatus::CONNECTED;
                connection_info_.connection_id = connection_id_;
                connection_info_.remote_address = config.host;
                connection_info_.remote_port = config.port;
                connection_info_.status = status_;
                connection_info_.connected_at = std::chrono::system_clock::now();
                connection_info_.last_activity = connection_info_.connected_at;
                
                update_statistics("connections", 1);
                
                if (connection_callback_) {
                    connection_callback_(connection_info_);
                }
                
                return true;
            } else {
                status_ = ConnectionStatus::ERROR;
                if (error_callback_) {
                    error_callback_("Failed to connect to " + config.host + ":" + std::to_string(config.port));
                }
                return false;
            }
        } catch (const std::exception& e) {
            status_ = ConnectionStatus::ERROR;
            if (error_callback_) {
                error_callback_("Connection error: " + std::string(e.what()));
            }
            return false;
        }
    }

    bool disconnect() {
        std::lock_guard<std::mutex> lock(mutex_);
        
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
        
        return true;
    }

    bool is_connected() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return status_ == ConnectionStatus::CONNECTED && (client_ != nullptr || ssl_client_ != nullptr);
    }

    ConnectionInfo get_connection_info() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return connection_info_;
    }

    std::future<Message> send_request(const Message& message) {
        return std::async(std::launch::async, [this, message]() {
            return send_request_impl(message);
        });
    }

    std::future<Message> send_request(const std::string& method, 
                                    const std::string& path, 
                                    const std::string& body,
                                    const std::map<std::string, std::string>& headers) {
        Message msg = utils::create_request(method, path, body, headers);
        return send_request(msg);
    }

    Message send_request_sync(const Message& message) {
        return send_request_impl(message);
    }

    Message send_request_sync(const std::string& method, 
                            const std::string& path, 
                            const std::string& body,
                            const std::map<std::string, std::string>& headers) {
        Message msg = utils::create_request(method, path, body, headers);
        return send_request_sync(msg);
    }

    bool start_server(const NetConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        try {
            config_ = config;
            server_ = std::make_unique<httplib::Server>();
            
            // 设置默认处理器
            server_->Get(".*", [this](const httplib::Request& req, httplib::Response& res) {
                handle_request(req, res);
            });
            
            server_->Post(".*", [this](const httplib::Request& req, httplib::Response& res) {
                handle_request(req, res);
            });
            
            server_->Put(".*", [this](const httplib::Request& req, httplib::Response& res) {
                handle_request(req, res);
            });
            
            server_->Delete(".*", [this](const httplib::Request& req, httplib::Response& res) {
                handle_request(req, res);
            });
            
            // 启动服务器
            server_running_ = server_->listen(config.host, config.port);
            
            if (server_running_) {
                update_statistics("server_starts", 1);
            }
            
            return server_running_;
        } catch (const std::exception& e) {
            if (error_callback_) {
                error_callback_("Server start error: " + std::string(e.what()));
            }
            return false;
        }
    }

    bool stop_server() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (server_) {
            server_->stop();
            server_.reset();
        }
        
        server_running_ = false;
        return true;
    }

    bool is_server_running() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return server_running_;
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

    void update_config(const NetConfig& config) {
        std::lock_guard<std::mutex> lock(mutex_);
        config_ = config;
    }

    NetConfig get_config() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return config_;
    }

    nlohmann::json get_statistics() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return statistics_;
    }

    bool health_check() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!client_ && !ssl_client_) {
            return false;
        }
        
        try {
            httplib::Result result;
            if (ssl_client_) {
                result = ssl_client_->Get("/health");
            } else {
                result = client_->Get("/health");
            }
            return result != nullptr && result->status == 200;
        } catch (const std::exception& e) {
            return false;
        }
    }

private:
    mutable std::mutex mutex_;
    std::unique_ptr<httplib::Client> client_;
    std::unique_ptr<httplib::SSLClient> ssl_client_;
    std::unique_ptr<httplib::Server> server_;
    ConnectionStatus status_;
    bool server_running_;
    NetConfig config_;
    std::string connection_id_;
    ConnectionInfo connection_info_;
    nlohmann::json statistics_;
    
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
    ErrorCallback error_callback_;

    std::string generate_connection_id() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        return "conn_" + std::to_string(timestamp) + "_" + std::to_string(dis(gen));
    }

    Message send_request_impl(const Message& message) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (!client_ && !ssl_client_) {
            Message error_msg = utils::create_error(500, "Client not connected");
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
                Message error_msg = utils::create_error(405, "Method not allowed: " + message.method);
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
                
                for (const auto& [key, value] : result->headers) {
                    response.headers[key] = value;
                }
                
                connection_info_.last_activity = response.timestamp;
                update_statistics("requests_sent", 1);
                update_statistics("responses_received", 1);

                if (message_callback_) {
                    std::cout<<"Sending message_callback response: " + response.to_json().dump()<<std::endl;
                    message_callback_(response);
                }
                
                return response;
            } else {
                Message error_msg = utils::create_error(500, "Request failed");
                return error_msg;
            }
        } catch (const std::exception& e) {
            Message error_msg = utils::create_error(500, "Request error: " + std::string(e.what()));
            return error_msg;
        }
    }

    void handle_request(const httplib::Request& req, httplib::Response& res) {
        try {
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
            
            update_statistics("requests_received", 1);
            
            if (message_callback_) {
                message_callback_(request);
            }
            
            // 默认响应
            res.status = 200;
            res.set_content("OK", "text/plain");
            
        } catch (const std::exception& e) {
            res.status = 500;
            res.set_content("Internal Server Error", "text/plain");
        }
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

bool NetIO::is_connected() const {
    return impl_->is_connected();
}

ConnectionInfo NetIO::get_connection_info() const {
    return impl_->get_connection_info();
}

std::future<Message> NetIO::send_request(const Message& message) {
    return impl_->send_request(message);
}

std::future<Message> NetIO::send_request(const std::string& method, 
                                        const std::string& path, 
                                        const std::string& body,
                                        const std::map<std::string, std::string>& headers) {
    return impl_->send_request(method, path, body, headers);
}

Message NetIO::send_request_sync(const Message& message) {
    return impl_->send_request_sync(message);
}

Message NetIO::send_request_sync(const std::string& method, 
                                const std::string& path, 
                                const std::string& body,
                                const std::map<std::string, std::string>& headers) {
    return impl_->send_request_sync(method, path, body, headers);
}

bool NetIO::start_server(const NetConfig& config) {
    return impl_->start_server(config);
}

bool NetIO::stop_server() {
    return impl_->stop_server();
}

bool NetIO::is_server_running() const {
    return impl_->is_server_running();
}

void NetIO::set_message_callback(MessageCallback callback) {
    impl_->set_message_callback(callback);
}

void NetIO::set_connection_callback(ConnectionCallback callback) {
    impl_->set_connection_callback(callback);
}

void NetIO::set_error_callback(ErrorCallback callback) {
    impl_->set_error_callback(callback);
}

void NetIO::update_config(const NetConfig& config) {
    impl_->update_config(config);
}

NetConfig NetIO::get_config() const {
    return impl_->get_config();
}

nlohmann::json NetIO::get_statistics() const {
    return impl_->get_statistics();
}

bool NetIO::health_check() {
    return impl_->health_check();
}

// 工具函数实现
namespace utils {

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

Message create_request(const std::string& method, 
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

Message create_response(int status_code, 
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

Message create_error(int status_code, 
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

NetConfig create_http_config(const std::string& host, int port) {
    NetConfig config;
    config.host = host;
    config.port = port;
    config.protocol = Protocol::HTTP;
    return config;
}

NetConfig create_https_config(const std::string& host, int port) {
    NetConfig config;
    config.host = host;
    config.port = port;
    config.protocol = Protocol::HTTPS;
    return config;
}

bool validate_config(const NetConfig& config) {
    return !config.host.empty() && 
           config.port > 0 && 
           config.port <= 65535 &&
           config.timeout_seconds > 0;
}

bool validate_message(const Message& message) {
    return !message.id.empty() && 
           !message.method.empty() && 
           !message.path.empty();
}

} // namespace utils

// NetConfig实现
nlohmann::json NetConfig::to_json() const {
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
    return j;
}

} // namespace netio
