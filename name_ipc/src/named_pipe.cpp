#include "named_pipe.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

namespace mcp {

NamedPipe::NamedPipe()
    : running_(false), connected_(false), message_handler_(nullptr), pipe_fd_(-1) {
}

NamedPipe::~NamedPipe() {
    stop();
}

bool NamedPipe::start(const NamedPipeConfig& config) {
    if (running_) {
        mcp_logger::getModuleLogger("named_pipe")->warning("NamedPipe is already running");
        return false;
    }
    
    config_ = config;
    
    mcp_logger::getModuleLogger("named_pipe")->info("Starting NamedPipe with path: " + config.pipe_path);
    
    // 创建命名管道
    if (!createNamedPipe()) {
        mcp_logger::getModuleLogger("named_pipe")->error("Failed to create named pipe");
        return false;
    }
    
    // 打开命名管道
    if (!openNamedPipe()) {
        mcp_logger::getModuleLogger("named_pipe")->error("Failed to open named pipe");
        closeNamedPipe();
        return false;
    }
    
    running_ = true;
    
    // 如果是读模式或双工模式，启动读取线程
    if (config.mode == NamedPipeMode::READ || config.mode == NamedPipeMode::DUPLEX) {
        read_thread_ = std::make_unique<std::thread>(&NamedPipe::readThreadFunc, this);
    }
    
    mcp_logger::getModuleLogger("named_pipe")->info("NamedPipe started successfully");
    return true;
}

void NamedPipe::stop() {
    if (!running_) {
        return;
    }
    
    mcp_logger::getModuleLogger("named_pipe")->info("Stopping NamedPipe");
    running_ = false;
    
    // 等待读取线程结束
    if (read_thread_ && read_thread_->joinable()) {
        mcp_logger::getModuleLogger("named_pipe")->debug("Waiting for read thread to finish...");
        read_thread_->join();
        mcp_logger::getModuleLogger("named_pipe")->debug("Read thread joined successfully");
    }
    
    // 关闭连接
    closeConnection();
    
    // 关闭命名管道
    closeNamedPipe();
    
    mcp_logger::getModuleLogger("named_pipe")->info("NamedPipe stopped");
}

bool NamedPipe::isRunning() const {
    return running_;
}

bool NamedPipe::isConnected() const {
    return connected_;
}

bool NamedPipe::sendMessage(const std::string& message) {
    NamedPipeMessage msg;
    msg.content = message;
    return sendMessage(msg);
}

bool NamedPipe::sendMessage(const NamedPipeMessage& message) {
    if (!running_ || !connected_) {
        mcp_logger::getModuleLogger("named_pipe")->error("Cannot send message: NamedPipe is not running or not connected");
        return false;
    }
    
    if (config_.mode == NamedPipeMode::READ) {
        mcp_logger::getModuleLogger("named_pipe")->error("Cannot send message: NamedPipe is in READ mode");
        return false;
    }
    
    std::lock_guard<std::mutex> lock(write_mutex_);
    
    // 添加换行符作为消息分隔符
    std::string data = message.content;
    if (!data.empty() && data.back() != '\n') {
        data += '\n';
    }
    
    ssize_t written = writeImpl(data.c_str(), data.size());
    if (written < 0) {
        mcp_logger::getModuleLogger("named_pipe")->error("Failed to write message to named pipe");
        return false;
    }
    
    if (static_cast<size_t>(written) != data.size()) {
        mcp_logger::getModuleLogger("named_pipe")->warning("Partial write to named pipe: " + 
            std::to_string(written) + " of " + std::to_string(data.size()) + " bytes");
    }
    
    mcp_logger::getModuleLogger("named_pipe")->debug("Message sent: " + message.content.substr(0, 100));
    return true;
}

bool NamedPipe::hasMessage() const {
    std::lock_guard<std::mutex> lock(message_mutex_);
    return !message_queue_.empty();
}

NamedPipeMessage NamedPipe::getMessage() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    if (message_queue_.empty()) {
        return NamedPipeMessage();
    }
    
    NamedPipeMessage msg = message_queue_.front();
    message_queue_.pop();
    return msg;
}

std::vector<NamedPipeMessage> NamedPipe::getAllMessages() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    std::vector<NamedPipeMessage> messages;
    
    while (!message_queue_.empty()) {
        messages.push_back(message_queue_.front());
        message_queue_.pop();
    }
    
    return messages;
}

void NamedPipe::setMessageHandler(std::shared_ptr<NamedPipeMessageHandler> handler) {
    message_handler_ = handler;
}

std::string NamedPipe::getPipePath() const {
    return config_.pipe_path;
}

NamedPipeMode NamedPipe::getMode() const {
    return config_.mode;
}

// POSIX实现
bool NamedPipe::createNamedPipe() {
    return createNamedPipeImpl();
}

void NamedPipe::closeNamedPipe() {
    closeNamedPipeImpl();
}

bool NamedPipe::openNamedPipe() {
    return openNamedPipeImpl();
}

void NamedPipe::closeConnection() {
    closeConnectionImpl();
}

void NamedPipe::readThreadFunc() {
    mcp_logger::getModuleLogger("named_pipe")->debug("Read thread started");
    
    char* buffer = new char[config_.buffer_size];
    std::string line_buffer;
    
    while (running_) {
        ssize_t bytes_read = readImpl(buffer, config_.buffer_size - 1);
        
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            line_buffer += std::string(buffer, bytes_read);
            
            // 处理完整的行
            size_t pos;
            while ((pos = line_buffer.find('\n')) != std::string::npos) {
                std::string line = line_buffer.substr(0, pos);
                line_buffer = line_buffer.substr(pos + 1);
                
                if (!line.empty()) {
                    processMessage(line);
                }
            }
        } else if (bytes_read == 0) {
            // EOF - 连接关闭
            mcp_logger::getModuleLogger("named_pipe")->info("Connection closed by peer");
            if (message_handler_) {
                message_handler_->onDisconnected();
            }
            connected_ = false;
            break;
        } else {
            // 错误
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 非阻塞模式，没有数据可读
                std::this_thread::sleep_for(std::chrono::milliseconds(config_.read_timeout_ms));
                continue;
            } else if (errno == EPIPE) {
                mcp_logger::getModuleLogger("named_pipe")->info("Connection broken");
                if (message_handler_) {
                    message_handler_->onDisconnected();
                }
                connected_ = false;
                break;
            } else {
                mcp_logger::getModuleLogger("named_pipe")->error("Read error: " + std::string(strerror(errno)));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
    
    delete[] buffer;
    mcp_logger::getModuleLogger("named_pipe")->debug("Read thread finished");
}

void NamedPipe::processMessage(const std::string& line) {
    NamedPipeMessage message;
    message.content = line;
    
    // 尝试解析JSON以提取ID
    try {
        nlohmann::json json_msg = nlohmann::json::parse(line);
        if (json_msg.contains("id")) {
            if (json_msg["id"].is_string()) {
                message.id = json_msg["id"].get<std::string>();
            } else if (json_msg["id"].is_number()) {
                message.id = std::to_string(json_msg["id"].get<int>());
            }
        }
    } catch (...) {
        // 不是JSON格式，忽略
    }
    
    // 添加到消息队列
    {
        std::lock_guard<std::mutex> lock(message_mutex_);
        message_queue_.push(message);
    }
    
    // 调用消息处理器
    if (message_handler_) {
        message_handler_->onMessage(message);
    }
    
    mcp_logger::getModuleLogger("named_pipe")->debug("Message processed: " + line.substr(0, 100));
}

// POSIX实现
bool NamedPipe::createNamedPipeImpl() {
    if (config_.auto_create) {
        mcp_logger::getModuleLogger("named_pipe")->debug("Creating named pipe (FIFO): " + config_.pipe_path);
        
        // 如果管道已存在，先删除
        struct stat st;
        if (stat(config_.pipe_path.c_str(), &st) == 0) {
            if (S_ISFIFO(st.st_mode)) {
                mcp_logger::getModuleLogger("named_pipe")->debug("FIFO already exists, removing it");
                unlink(config_.pipe_path.c_str());
            } else {
                // 路径存在但不是FIFO，尝试删除它（可能是普通文件或目录）
                mcp_logger::getModuleLogger("named_pipe")->warning("Path exists but is not a FIFO, attempting to remove: " + config_.pipe_path);
                if (unlink(config_.pipe_path.c_str()) == -1) {
                    // 如果是目录，尝试使用rmdir
                    if (S_ISDIR(st.st_mode)) {
                        if (rmdir(config_.pipe_path.c_str()) == -1) {
                            mcp_logger::getModuleLogger("named_pipe")->error("Failed to remove existing path (not a FIFO): " + config_.pipe_path + " - " + std::string(strerror(errno)));
                            return false;
                        }
                    } else {
                        mcp_logger::getModuleLogger("named_pipe")->error("Failed to remove existing path (not a FIFO): " + config_.pipe_path + " - " + std::string(strerror(errno)));
                        return false;
                    }
                }
                mcp_logger::getModuleLogger("named_pipe")->info("Removed existing non-FIFO path: " + config_.pipe_path);
            }
        }
        
        // 创建FIFO
        if (mkfifo(config_.pipe_path.c_str(), 0666) == -1) {
            mcp_logger::getModuleLogger("named_pipe")->error("Failed to create FIFO: " + std::string(strerror(errno)));
            return false;
        }
        
        mcp_logger::getModuleLogger("named_pipe")->debug("FIFO created successfully");
    } else {
        // 客户端模式，检查FIFO是否存在
        mcp_logger::getModuleLogger("named_pipe")->debug("Client mode, checking if FIFO exists: " + config_.pipe_path);
        struct stat st;
        if (stat(config_.pipe_path.c_str(), &st) != 0 || !S_ISFIFO(st.st_mode)) {
            mcp_logger::getModuleLogger("named_pipe")->error("FIFO does not exist: " + config_.pipe_path);
            return false;
        }
        mcp_logger::getModuleLogger("named_pipe")->debug("FIFO exists, ready to connect");
    }
    
    return true;
}

void NamedPipe::closeNamedPipeImpl() {
    mcp_logger::getModuleLogger("named_pipe")->debug("Closing named pipe");
    
    // 关闭文件描述符
    if (pipe_fd_ != -1) {
        close(pipe_fd_);
        pipe_fd_ = -1;
    }
    
    // 删除FIFO文件（可选）
    if (!config_.pipe_path.empty()) {
        unlink(config_.pipe_path.c_str());
        mcp_logger::getModuleLogger("named_pipe")->debug("FIFO file removed: " + config_.pipe_path);
    }
    
    mcp_logger::getModuleLogger("named_pipe")->debug("Named pipe closed");
}

bool NamedPipe::openNamedPipeImpl() {
    // POSIX: 打开FIFO
    mcp_logger::getModuleLogger("named_pipe")->debug("Opening FIFO");
    
    int flags = 0;
    if (config_.mode == NamedPipeMode::READ) {
        flags = O_RDONLY;
    } else if (config_.mode == NamedPipeMode::WRITE) {
        flags = O_WRONLY;
    } else {
        flags = O_RDWR;
    }
    
    if (config_.non_blocking) {
        flags |= O_NONBLOCK;
    }
    
    pipe_fd_ = open(config_.pipe_path.c_str(), flags);
    if (pipe_fd_ == -1) {
        mcp_logger::getModuleLogger("named_pipe")->error("Failed to open FIFO: " + std::string(strerror(errno)));
        return false;
    }
    
    connected_ = true;
    
    if (message_handler_) {
        message_handler_->onConnected();
    }
    
    mcp_logger::getModuleLogger("named_pipe")->debug("FIFO opened successfully");
    return true;
}

void NamedPipe::closeConnectionImpl() {
    connected_ = false;
    // POSIX不需要特殊处理，关闭文件描述符即可
}

ssize_t NamedPipe::readImpl(char* buffer, size_t size) {
    return read(pipe_fd_, buffer, size);
}

ssize_t NamedPipe::writeImpl(const char* buffer, size_t size) {
    return write(pipe_fd_, buffer, size);
}

bool NamedPipe::waitForConnection() {
    // POSIX不需要等待连接，open()会自动阻塞直到另一端打开
    return true;
}

} // namespace mcp

