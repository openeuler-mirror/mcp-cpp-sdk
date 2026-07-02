#include "pipe_communication.h"
#include <iostream>
#include <sstream>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#endif

namespace mcp {

PipeCommunication::PipeCommunication() 
    : running_(false), process_id_(-1), request_id_counter_(0),
      current_log_level_(LogLevel::INFO), logging_enabled_(true), log_file_path_("") {
#ifdef _WIN32
    process_handle_ = nullptr;
#endif
    stdin_pipe_[0] = stdin_pipe_[1] = -1;
    stdout_pipe_[0] = stdout_pipe_[1] = -1;
}

PipeCommunication::~PipeCommunication() {
    stop();
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

bool PipeCommunication::start(const PipeConfig& config) {
    if (running_) {
        log(LogLevel::WARNING, "PipeCommunication is already running");
        return true;
    }
    
    config_ = config;
    current_log_level_ = config.log_level;
    logging_enabled_ = config.enable_logging;
    log_file_path_ = config.log_file_path;
    
    // 初始化日志文件
    if (logging_enabled_ && !log_file_path_.empty()) {
        log_file_.open(log_file_path_, std::ios::app);
        if (!log_file_.is_open()) {
            log(LogLevel::ERROR, "Failed to open log file: " + log_file_path_);
        }
    }
    
    log(LogLevel::INFO, "Starting PipeCommunication with command: " + config.command);
    
    if (!create_pipes()) {
        log(LogLevel::ERROR, "Failed to create pipes");
        return false;
    }
    
    if (!start_process()) {
        log(LogLevel::ERROR, "Failed to start process");
        close_pipes();
        return false;
    }
    
    running_ = true;
    read_thread_ = std::make_unique<std::thread>(&PipeCommunication::read_thread_func, this);
    
    log(LogLevel::INFO, "PipeCommunication started successfully, PID: " + std::to_string(process_id_));
    return true;
}

void PipeCommunication::stop() {
    if (!running_) {
        return;
    }
    
    log(LogLevel::INFO, "Stopping PipeCommunication");
    running_ = false;
    
    if (read_thread_ && read_thread_->joinable()) {
        read_thread_->join();
    }
    
    stop_process();
    close_pipes();
    log(LogLevel::INFO, "PipeCommunication stopped");
}

bool PipeCommunication::is_running() const {
    return running_;
}

bool PipeCommunication::send_message(const std::string& message) {
    if (!running_) {
        log(LogLevel::ERROR, "Cannot send message: PipeCommunication is not running");
        return false;
    }
    
    log(LogLevel::DEBUG, "Sending message to subprocess: " + message);
    
    std::string msg_with_newline = message + "\n";
    ssize_t bytes_written = write_impl(msg_with_newline.c_str(), msg_with_newline.size());
    
    bool success = bytes_written == static_cast<ssize_t>(msg_with_newline.size());
    if (success) {
        log(LogLevel::DEBUG, "Message sent successfully (" + std::to_string(bytes_written) + " bytes)");
    } else {
        log(LogLevel::ERROR, "Failed to send message, written " + std::to_string(bytes_written) + " bytes");
    }
    
    return success;
}

std::future<std::string> PipeCommunication::send_request(const std::string& request, int timeout_seconds) {
    std::string request_id = "";
    std::string full_request = request;
    
    // 添加请求ID到JSON中
    try {
        nlohmann::json req_json = nlohmann::json::parse(request);
        //req_json["id"] = request_id;
        std::cout << "req_json: " << req_json.dump() << std::endl;
        full_request = req_json.dump();
        if (req_json["id"].is_null() || req_json["id"].empty()) {
            request_id = generate_request_id();
        } else {
            // 处理id字段为int类型的情况
            if (req_json["id"].is_number_integer()) {
                request_id = std::to_string(req_json["id"].get<int>());
            } else if (req_json["id"].is_string()) {
                request_id = req_json["id"].get<std::string>();
            } else {
                // 如果既不是int也不是string，生成新的ID
                request_id = generate_request_id();
            }
        }
    } catch (...) {
        // 如果解析失败，直接使用原始请求
    }
    std::cout << "send_request request_id: " << request_id << std::endl;
    
    // 创建Promise和Future
    std::promise<std::string> response_promise;
    std::future<std::string> response_future = response_promise.get_future();
    
    // 发送请求
    if (!send_message(full_request)) {
        response_promise.set_value("Failed to send request");
        return response_future;
    }
    
    {
        std::lock_guard<std::mutex> lock(response_mutex_);
        pending_requests_[request_id] = std::move(response_promise);
    }
    
    // 设置超时
    if (timeout_seconds > 0) {
        std::thread timeout_thread([this, request_id, timeout_seconds]() {
            std::this_thread::sleep_for(std::chrono::seconds(timeout_seconds));
        
            std::lock_guard<std::mutex> lock(response_mutex_);
            auto it = pending_requests_.find(request_id);
            if (it != pending_requests_.end()) {
                it->second.set_value("Request timeout");
                pending_requests_.erase(it);
            }
        });
        timeout_thread.detach();
    }
    
    return response_future;
}

void PipeCommunication::set_message_handler(std::shared_ptr<MessageHandler> handler) {
    message_handler_ = handler;
}

int PipeCommunication::get_process_id() const {
    return process_id_;
}

bool PipeCommunication::is_process_alive() const {
    if (process_id_ <= 0) {
        return false;
    }
    
#if defined(_WIN32)
    DWORD exit_code;
    if (GetExitCodeProcess(static_cast<HANDLE>(process_handle_), &exit_code)) {
        return exit_code == STILL_ACTIVE;
    }
    return false;
#else
    int status;
    pid_t result = waitpid(process_id_, &status, WNOHANG);
    return result == 0; // 0 means process is still running
#endif
}

bool PipeCommunication::create_pipes() {
    return create_pipes_impl();
}

void PipeCommunication::close_pipes() {
    close_pipes_impl();
}

bool PipeCommunication::start_process() {
    return start_process_impl();
}

void PipeCommunication::stop_process() {
    stop_process_impl();
}

// 平台相关实现
bool PipeCommunication::create_pipes_impl() {
#if defined(_WIN32)
    // Windows实现
    log(LogLevel::DEBUG, "Creating pipes on Windows");
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE child_stdin_read, child_stdin_write;
    HANDLE child_stdout_read, child_stdout_write;
    
    if (!CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0) ||
        !CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0)) {
        log(LogLevel::ERROR, "Failed to create pipes on Windows");
        return false;
    }
    
    if (!SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        log(LogLevel::ERROR, "Failed to set handle information on Windows");
        CloseHandle(child_stdin_read);
        CloseHandle(child_stdin_write);
        CloseHandle(child_stdout_read);
        CloseHandle(child_stdout_write);
        return false;
    }
    
    stdin_pipe_[0] = reinterpret_cast<intptr_t>(child_stdin_read);
    stdin_pipe_[1] = reinterpret_cast<intptr_t>(child_stdin_write);
    stdout_pipe_[0] = reinterpret_cast<intptr_t>(child_stdout_read);
    stdout_pipe_[1] = reinterpret_cast<intptr_t>(child_stdout_write);
    
    log(LogLevel::DEBUG, "Pipes created successfully on Windows");
    return true;
#else
    // POSIX实现
    log(LogLevel::DEBUG, "Creating pipes on POSIX");
    
    if (pipe(stdin_pipe_) == -1 || pipe(stdout_pipe_) == -1) {
        log(LogLevel::ERROR, "Failed to create pipes on POSIX");
        return false;
    }
    
    // 设置非阻塞模式
    int flags = fcntl(stdout_pipe_[0], F_GETFL, 0);
    if (fcntl(stdout_pipe_[0], F_SETFL, flags | O_NONBLOCK) == -1) {
        log(LogLevel::WARNING, "Failed to set non-blocking mode for stdout pipe");
    }
    
    log(LogLevel::DEBUG, "Pipes created successfully on POSIX");
    return true;
#endif
}

void PipeCommunication::close_pipes_impl() {
    log(LogLevel::DEBUG, "Closing pipes");
    
#if defined(_WIN32)
    if (stdin_pipe_[1] != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(stdin_pipe_[1]));
        stdin_pipe_[1] = -1;
    }
    if (stdout_pipe_[0] != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(stdout_pipe_[0]));
        stdout_pipe_[0] = -1;
    }
#else
    if (stdin_pipe_[1] != -1) {
        close(stdin_pipe_[1]);
        stdin_pipe_[1] = -1;
    }
    if (stdout_pipe_[0] != -1) {
        close(stdout_pipe_[0]);
        stdout_pipe_[0] = -1;
    }
#endif
    
    log(LogLevel::DEBUG, "Pipes closed");
}

bool PipeCommunication::start_process_impl() {
#if defined(_WIN32)
    // Windows实现
    log(LogLevel::DEBUG, "Starting process on Windows");
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    
    ZeroMemory(&si, sizeof(STARTUPINFOA));
    si.cb = sizeof(STARTUPINFOA);
    si.hStdInput = reinterpret_cast<HANDLE>(stdin_pipe_[0]);
    si.hStdOutput = reinterpret_cast<HANDLE>(stdout_pipe_[1]);
    si.hStdError = reinterpret_cast<HANDLE>(stdout_pipe_[1]);
    si.dwFlags |= STARTF_USESTDHANDLES;
    
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    
    // 设置环境变量
    for (const auto& [key, value] : config_.env_vars) {
        SetEnvironmentVariableA(key.c_str(), value.c_str());
        log(LogLevel::DEBUG, "Set environment variable: " + key + "=" + value);
    }
    
    std::string cmd_line = "cmd.exe /c " + config_.command;
    for (const auto& arg : config_.args) {
        cmd_line += " " + arg;
    }
    
    log(LogLevel::DEBUG, "Command line: " + cmd_line);
    
    char* cmd_line_ptr = _strdup(cmd_line.c_str());
    BOOL success = CreateProcessA(NULL, cmd_line_ptr, NULL, NULL, TRUE, 
                                 CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    free(cmd_line_ptr);
    
    if (success) {
        process_id_ = pi.dwProcessId;
        process_handle_ = pi.hProcess;
        CloseHandle(pi.hThread);
        CloseHandle(reinterpret_cast<HANDLE>(stdin_pipe_[0]));
        CloseHandle(reinterpret_cast<HANDLE>(stdout_pipe_[1]));
        log(LogLevel::INFO, "Process started successfully on Windows, PID: " + std::to_string(process_id_));
        return true;
    } else {
        DWORD error = GetLastError();
        log(LogLevel::ERROR, "Failed to start process on Windows, error code: " + std::to_string(error));
    }
    
    return false;
#else
    // POSIX实现
    log(LogLevel::DEBUG, "Starting process on POSIX");
    
    process_id_ = fork();
    
    if (process_id_ == -1) {
        log(LogLevel::ERROR, "Failed to fork process on POSIX");
        return false;
    }
    
    if (process_id_ == 0) {
        // 子进程
        log(LogLevel::DEBUG, "Child process executing");
        
        // 设置环境变量
        for (const auto& [key, value] : config_.env_vars) {
            setenv(key.c_str(), value.c_str(), 1);
            log(LogLevel::DEBUG, "Set environment variable: " + key + "=" + value);
        }
        
        // 重定向标准输入输出
        close(stdin_pipe_[1]);
        close(stdout_pipe_[0]);
        
        if (dup2(stdin_pipe_[0], STDIN_FILENO) == -1 ||
            dup2(stdout_pipe_[1], STDOUT_FILENO) == -1) {
            log(LogLevel::ERROR, "Failed to redirect stdio in child process");
            exit(EXIT_FAILURE);
        }
        
        close(stdin_pipe_[0]);
        close(stdout_pipe_[1]);
        
        // 执行命令
        std::vector<char*> c_args;
        c_args.push_back(const_cast<char*>(config_.command.c_str()));
        for (const auto& arg : config_.args) {
            c_args.push_back(const_cast<char*>(arg.c_str()));
        }
        c_args.push_back(nullptr);
        
        std::string cmd_str = config_.command;
        for (const auto& arg : config_.args) {
            cmd_str += " " + arg;
        }
        log(LogLevel::DEBUG, "Executing command: " + cmd_str);
        
        execvp(c_args[0], c_args.data());
        log(LogLevel::ERROR, "Failed to execute command: " + cmd_str);
        exit(EXIT_FAILURE);
    } else {
        // 父进程
        close(stdin_pipe_[0]);
        close(stdout_pipe_[1]);
        log(LogLevel::INFO, "Process started successfully on POSIX, PID: " + std::to_string(process_id_));
        return true;
    }
#endif
}

void PipeCommunication::stop_process_impl() {
    log(LogLevel::DEBUG, "Stopping process, PID: " + std::to_string(process_id_));
    
#if defined(_WIN32)
    if (process_handle_) {
        log(LogLevel::DEBUG, "Terminating process on Windows");
        TerminateProcess(static_cast<HANDLE>(process_handle_), 0);
        DWORD wait_result = WaitForSingleObject(static_cast<HANDLE>(process_handle_), 2000);
        if (wait_result == WAIT_TIMEOUT) {
            log(LogLevel::WARNING, "Process termination timeout on Windows");
        } else {
            log(LogLevel::DEBUG, "Process terminated successfully on Windows");
        }
        CloseHandle(static_cast<HANDLE>(process_handle_));
        process_handle_ = nullptr;
    }
#else
    if (process_id_ > 0) {
        log(LogLevel::DEBUG, "Sending SIGTERM to process on POSIX");
        if (kill(process_id_, SIGTERM) == 0) {
            int status;
            pid_t result = waitpid(process_id_, &status, 0);
            if (result == process_id_) {
                if (WIFEXITED(status)) {
                    log(LogLevel::DEBUG, "Process exited normally with code: " + std::to_string(WEXITSTATUS(status)));
                } else if (WIFSIGNALED(status)) {
                    log(LogLevel::DEBUG, "Process terminated by signal: " + std::to_string(WTERMSIG(status)));
                }
            } else {
                log(LogLevel::WARNING, "Failed to wait for process termination on POSIX");
            }
        } else {
            log(LogLevel::ERROR, "Failed to send SIGTERM to process on POSIX");
        }
    }
#endif
    process_id_ = -1;
    log(LogLevel::DEBUG, "Process stopped");
}

ssize_t PipeCommunication::read_impl(char* buffer, size_t size) {
#if defined(_WIN32)
    DWORD bytes_read;
    BOOL success = ReadFile(reinterpret_cast<HANDLE>(stdout_pipe_[0]), 
                           buffer, size - 1, &bytes_read, NULL);
    return success ? bytes_read : -1;
#else
    return read(stdout_pipe_[0], buffer, size - 1);
#endif
}

ssize_t PipeCommunication::write_impl(const char* buffer, size_t size) {
#if defined(_WIN32)
    DWORD bytes_written;
    BOOL success = WriteFile(reinterpret_cast<HANDLE>(stdin_pipe_[1]), 
                            buffer, size, &bytes_written, NULL);
    return success ? bytes_written : -1;
#else
    return write(stdin_pipe_[1], buffer, size);
#endif
}

void PipeCommunication::read_thread_func() {
    std::vector<char> buffer(config_.buffer_size);
    std::string data_buffer;
    int consecutive_empty_reads = 0;
    const int max_empty_reads = 100; // 最大连续空读取次数
    
    log(LogLevel::INFO, "Read thread started");
    
    while (running_) {
        ssize_t bytes_read = read_impl(buffer.data(), config_.buffer_size);
        
        if (bytes_read > 0) {
            consecutive_empty_reads = 0; // 重置空读取计数
            buffer[bytes_read] = '\0';
            data_buffer.append(buffer.data(), bytes_read);
            
            // 记录原始输出
            //log_subprocess_output(std::string(buffer, bytes_read), "raw_output");
            
            // 处理完整的消息
            size_t pos = 0;
            while ((pos = data_buffer.find('\n')) != std::string::npos) {
                std::string line = data_buffer.substr(0, pos);
                data_buffer.erase(0, pos + 1);
                
                if (!line.empty()) {
                    // 记录处理的消息
                    log_subprocess_output(line, "processed_message");
                    process_message(line);
                }
            }
        } else if (bytes_read == 0) {
            // 管道关闭
            log(LogLevel::WARNING, "Subprocess pipe closed");
            break;
        } else {
            // 错误或暂时无数据
            consecutive_empty_reads++;
            if (consecutive_empty_reads > max_empty_reads) {
                log(LogLevel::WARNING, "Too many consecutive empty reads, stopping read thread");
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                //break;

            }
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.read_timeout_ms));
        }
    }
    
    log(LogLevel::INFO, "Read thread finished");
}

void PipeCommunication::process_message(const std::string& line) {
    try {
        nlohmann::json message = nlohmann::json::parse(line);
        
        if (message.contains("jsonrpc") && message["jsonrpc"] == "2.0") {
            if (message.contains("id") && !message["id"].is_null()) {
                // 这是一个响应
                std::string id;
                if (message["id"].is_string()) {
                    id = message["id"].get<std::string>();
                } else if (message["id"].is_number()) {
                    id = std::to_string(message["id"].get<int>());
                } else {
                    id = message["id"].dump();
                }
                
                if (message.contains("result")) {
                    std::cout << "pip_communication 收到响应: " << "id: " << id << ", result: " << message["result"].dump() << std::endl;
                    handle_response(id, message["result"].dump());
                } else if (message.contains("error")) {
                    std::cout << "pip_communication 收到错误: " << message["error"].dump() << std::endl;
                    handle_error(id, message["error"].dump());
                }
            } else if (message.contains("method")) {
                // 这是一个请求或通知
                Message msg;
                msg.type = MessageType::NOTIFICATION;
                msg.content = line;
                
                // 添加到消息队列
                {
                    std::lock_guard<std::mutex> lock(message_mutex_);
                    message_queue_.push(msg);
                }
                
                if (message_handler_) {
                    message_handler_->on_message(msg);
                }
            }
        } else {
            // 不是JSON-RPC格式，作为普通消息处理
            Message msg;
            msg.type = MessageType::NOTIFICATION;
            msg.content = line;
            
            // 添加到消息队列
            {
                std::lock_guard<std::mutex> lock(message_mutex_);
                message_queue_.push(msg);
            }
            
            if (message_handler_) {
                message_handler_->on_message(msg);
            }
        }
    } catch (const nlohmann::json::exception& e) {
        // JSON解析失败，作为普通消息处理
        Message msg;
        msg.type = MessageType::NOTIFICATION;
        msg.content = line;
        
        // 添加到消息队列
        {
            std::lock_guard<std::mutex> lock(message_mutex_);
            message_queue_.push(msg);
        }
        
        if (message_handler_) {
            message_handler_->on_message(msg);
        }
    }
}

std::string PipeCommunication::generate_request_id() {
    return std::to_string(++request_id_counter_);
}

void PipeCommunication::handle_response(const std::string& id, const std::string& content) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    auto it = pending_requests_.find(id);
    if (it != pending_requests_.end()) {
        it->second.set_value(content);
        pending_requests_.erase(it);
    }
}

void PipeCommunication::handle_error(const std::string& id, const std::string& error) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    auto it = pending_requests_.find(id);
    if (it != pending_requests_.end()) {
        it->second.set_value("ERROR: " + error);
        pending_requests_.erase(it);
    }
}

bool PipeCommunication::has_result() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return !result_queue_.empty();
}

std::string PipeCommunication::get_result() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    if (result_queue_.empty()) {
        return "";
    }
    std::string result = result_queue_.front();
    result_queue_.pop();
    return result;
}

std::vector<std::string> PipeCommunication::get_all_results() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    std::vector<std::string> results;
    while (!result_queue_.empty()) {
        results.push_back(result_queue_.front());
        result_queue_.pop();
    }
    return results;
}

bool PipeCommunication::has_message() const {
    std::lock_guard<std::mutex> lock(message_mutex_);
    return !message_queue_.empty();
}

Message PipeCommunication::get_message() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    if (message_queue_.empty()) {
        return Message{MessageType::NOTIFICATION, "", ""};
    }
    
    Message msg = message_queue_.front();
    message_queue_.pop();
    return msg;
}

std::vector<Message> PipeCommunication::get_all_messages() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    std::vector<Message> messages;
    
    while (!message_queue_.empty()) {
        messages.push_back(message_queue_.front());
        message_queue_.pop();
    }
    
    return messages;
}

// 日志功能实现
void PipeCommunication::log(LogLevel level, const std::string& message) {
    if (!logging_enabled_ || level < current_log_level_) {
        return;
    }
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    std::string timestamp = get_timestamp();
    std::string level_str = log_level_to_string(level);
    std::string log_entry = "[" + timestamp + "] [" + level_str + "] " + message;
    
    if (log_file_.is_open()) {
        log_file_ << log_entry << std::endl;
        log_file_.flush();
    } else {
        std::cout << log_entry << std::endl;
    }
}

void PipeCommunication::set_log_level(LogLevel level) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    current_log_level_ = level;
    log(LogLevel::INFO, "Log level changed to " + log_level_to_string(level));
}

void PipeCommunication::set_log_file(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    log_file_path_ = file_path;
    
    if (!file_path.empty()) {
        log_file_.open(file_path, std::ios::app);
        if (log_file_.is_open()) {
            log(LogLevel::INFO, "Log file opened: " + file_path);
        } else {
            log(LogLevel::ERROR, "Failed to open log file: " + file_path);
        }
    }
}

void PipeCommunication::enable_logging(bool enable) {
    std::lock_guard<std::mutex> lock(log_mutex_);
    logging_enabled_ = enable;
    if (enable) {
        log(LogLevel::INFO, "Logging enabled");
    }
}

void PipeCommunication::log_subprocess_output(const std::string& output, const std::string& source) {
    if (!logging_enabled_) {
        return;
    }
    
    std::string timestamp = get_timestamp();
    std::string log_entry = "[" + timestamp + "] [SUBPROCESS-" + source + "] " + output;
    
    std::lock_guard<std::mutex> lock(log_mutex_);
    
    if (log_file_.is_open()) {
        log_file_ << log_entry << std::endl;
        log_file_.flush();
    } else {
        std::cout << log_entry << std::endl;
    }
}

std::string PipeCommunication::get_timestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string PipeCommunication::log_level_to_string(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

} // namespace mcp
