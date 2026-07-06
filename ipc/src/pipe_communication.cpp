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
#include <errno.h>
#include <cstring>
#endif

namespace mcp {

PipeCommunication::PipeCommunication() 
    : running_(false), process_id_(-1), request_id_counter_(0),
      server_request_handler_(nullptr), server_mode_(false) {
#ifdef _WIN32
    process_handle_ = nullptr;
#endif
    stdin_pipe_[0] = stdin_pipe_[1] = -1;
    stdout_pipe_[0] = stdout_pipe_[1] = -1;
}

PipeCommunication::~PipeCommunication() {
    stop();
}

bool PipeCommunication::start(const PipeConfig& config) {
    if (running_) {
        mcp_logger::getModuleLogger("ipc")->warning("PipeCommunication is already running");
        return false; // 重复启动应该返回false
    }
    
    config_ = config;
    server_mode_ = (config.mode == CommunicationMode::SERVER);
    
    if (server_mode_) {
        mcp_logger::getModuleLogger("ipc")->info("Starting PipeCommunication in SERVER mode");
        return initializeServerMode();
    } else {
        mcp_logger::getModuleLogger("ipc")->info("Starting PipeCommunication in CLIENT mode with command: " + config.command);
        
        if (!createPipes()) {
            mcp_logger::getModuleLogger("ipc")->error("Failed to create pipes");
            return false;
        }
        
        if (!startProcess()) {
            mcp_logger::getModuleLogger("ipc")->error("Failed to start process");
            closePipes();
            return false;
        }
        
        running_ = true;
        read_thread_ = std::make_unique<std::thread>(&PipeCommunication::readThreadFunc, this);
        
        mcp_logger::getModuleLogger("ipc")->info("PipeCommunication started successfully, PID: " + std::to_string(process_id_));
        return true;
    }
}

void PipeCommunication::stop() {
    if (!running_) {
        return;
    }
    
    mcp_logger::getModuleLogger("ipc")->info("Stopping PipeCommunication");
    running_ = false;
    
    // 如果不是服务器模式（即客户端模式），先关闭stdin写入端
    // 这样服务器子进程可以检测到EOF并正常退出
    if (!server_mode_) {
        mcp_logger::getModuleLogger("ipc")->debug("Closing stdin pipe to signal subprocess to exit");
        closeStdinWriteEnd();
        
        // 给子进程一些时间检测到EOF并正常退出（最多等待500ms）
        // 这样可以避免发送SIGTERM
        if (isProcessAlive()) {
            auto start_time = std::chrono::steady_clock::now();
            while (std::chrono::steady_clock::now() - start_time < std::chrono::milliseconds(500)) {
                if (!isProcessAlive()) {
                    mcp_logger::getModuleLogger("ipc")->debug("Subprocess exited gracefully after stdin close");
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    }
    
    // 等待读取线程结束
    if (read_thread_ && read_thread_->joinable()) {
        mcp_logger::getModuleLogger("ipc")->debug("Waiting for read thread to finish...");
        read_thread_->join();
        mcp_logger::getModuleLogger("ipc")->debug("Read thread joined successfully");
    }
    
    // 停止子进程（如果还在运行）
    stopProcess();
    
    // 关闭管道
    closePipes();
    
    mcp_logger::getModuleLogger("ipc")->info("PipeCommunication stopped");
}

bool PipeCommunication::isRunning() const {
    return running_;
}

bool PipeCommunication::sendMessage(const std::string& message) {
    if (!running_) {
        mcp_logger::getModuleLogger("ipc")->error("Cannot send message: PipeCommunication is not running");
        return false;
    }
    
    mcp_logger::getModuleLogger("ipc")->debug("Sending message to subprocess: " + message);
    
    std::string msg_with_newline = message + "\n";
    ssize_t bytes_written = writeImpl(msg_with_newline.c_str(), msg_with_newline.size());
    
    bool success = bytes_written == static_cast<ssize_t>(msg_with_newline.size());
    if (success) {
        mcp_logger::getModuleLogger("ipc")->debug("Message sent successfully (" + std::to_string(bytes_written) + " bytes)");
    } else {
        mcp_logger::getModuleLogger("ipc")->error("Failed to send message, written " + std::to_string(bytes_written) + " bytes");
    }
    
    return success;
}

std::future<std::string> PipeCommunication::sendRequest(const std::string& request, int timeout_seconds) {
    std::string request_id = "";
    std::string full_request = request;
    
    // 添加请求ID到JSON中
    try {
        nlohmann::json req_json = nlohmann::json::parse(request);
        //req_json["id"] = request_id;
        mcp_logger::getModuleLogger("ipc")->debug("req_json: " + req_json.dump());
        full_request = req_json.dump();
        if (req_json["id"].is_null() || req_json["id"].empty()) {
            request_id = generateRequestId();
        } else {
            // 处理id字段为int类型的情况
            if (req_json["id"].is_number_integer()) {
                request_id = std::to_string(req_json["id"].get<int>());
            } else if (req_json["id"].is_string()) {
                request_id = req_json["id"].get<std::string>();
            } else {
                // 如果既不是int也不是string，生成新的ID
                request_id = generateRequestId();
            }
        }
    } catch (...) {
        // 如果解析失败，直接使用原始请求
    }
    mcp_logger::getModuleLogger("ipc")->debug("send_request request_id: " + request_id);
    
    // 创建Promise和Future
    std::promise<std::string> response_promise;
    std::future<std::string> response_future = response_promise.get_future();
    
    // 发送请求
        if (!sendMessage(full_request)) {
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

void PipeCommunication::setMessageHandler(std::shared_ptr<MessageHandler> handler) {
    message_handler_ = handler;
}

int PipeCommunication::getProcessId() const {
    return process_id_;
}

bool PipeCommunication::isProcessAlive() const {
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

bool PipeCommunication::createPipes() {
    return createPipesImpl();
}

void PipeCommunication::closePipes() {
    closePipesImpl();
}

bool PipeCommunication::startProcess() {
    return startProcessImpl();
}

void PipeCommunication::stopProcess() {
    stopProcessImpl();
}

// 平台相关实现
bool PipeCommunication::createPipesImpl() {
#if defined(_WIN32)
    // Windows实现
    mcp_logger::getModuleLogger("ipc")->debug("Creating pipes on Windows");
    
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = NULL;
    
    HANDLE child_stdin_read, child_stdin_write;
    HANDLE child_stdout_read, child_stdout_write;
    
    if (!CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0) ||
        !CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0)) {
        mcp_logger::getModuleLogger("ipc")->error("Failed to create pipes on Windows");
        return false;
    }
    
    if (!SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0) ||
        !SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0)) {
        mcp_logger::getModuleLogger("ipc")->error("Failed to set handle information on Windows");
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
    
    mcp_logger::getModuleLogger("ipc")->debug("Pipes created successfully on Windows");
    return true;
#else
    // POSIX实现
    mcp_logger::getModuleLogger("ipc")->debug("Creating pipes on POSIX");
    
    if (pipe(stdin_pipe_) == -1 || pipe(stdout_pipe_) == -1) {
        mcp_logger::getModuleLogger("ipc")->error("Failed to create pipes on POSIX");
        return false;
    }
    
    // 设置非阻塞模式
    int flags = fcntl(stdout_pipe_[0], F_GETFL, 0);
    if (fcntl(stdout_pipe_[0], F_SETFL, flags | O_NONBLOCK) == -1) {
        mcp_logger::getModuleLogger("ipc")->warning("Failed to set non-blocking mode for stdout pipe");
    }
    
    mcp_logger::getModuleLogger("ipc")->debug("Pipes created successfully on POSIX");
    return true;
#endif
}

void PipeCommunication::closeStdinWriteEnd() {
    mcp_logger::getModuleLogger("ipc")->debug("Closing stdin write end to signal EOF to subprocess");
    
#if defined(_WIN32)
    // Close stdin write end (parent's write handle to child's stdin)
    if (stdin_pipe_[1] != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(stdin_pipe_[1]));
        stdin_pipe_[1] = -1;
    }
#else
    // Close stdin write end (parent's write file descriptor to child's stdin)
    if (stdin_pipe_[1] != -1) {
        close(stdin_pipe_[1]);
        stdin_pipe_[1] = -1;
    }
#endif
}

void PipeCommunication::closePipesImpl() {
    mcp_logger::getModuleLogger("ipc")->debug("Closing pipes");
    
#if defined(_WIN32)
    // Close all pipe handles
    if (stdin_pipe_[0] != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(stdin_pipe_[0]));
        stdin_pipe_[0] = -1;
    }
    if (stdin_pipe_[1] != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(stdin_pipe_[1]));
        stdin_pipe_[1] = -1;
    }
    if (stdout_pipe_[0] != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(stdout_pipe_[0]));
        stdout_pipe_[0] = -1;
    }
    if (stdout_pipe_[1] != -1) {
        CloseHandle(reinterpret_cast<HANDLE>(stdout_pipe_[1]));
        stdout_pipe_[1] = -1;
    }
#else
    // Close all pipe file descriptors
    if (stdin_pipe_[0] != -1) {
        close(stdin_pipe_[0]);
        stdin_pipe_[0] = -1;
    }
    if (stdin_pipe_[1] != -1) {
        close(stdin_pipe_[1]);
        stdin_pipe_[1] = -1;
    }
    if (stdout_pipe_[0] != -1) {
        close(stdout_pipe_[0]);
        stdout_pipe_[0] = -1;
    }
    if (stdout_pipe_[1] != -1) {
        close(stdout_pipe_[1]);
        stdout_pipe_[1] = -1;
    }
#endif
    
    mcp_logger::getModuleLogger("ipc")->debug("Pipes closed");
}

bool PipeCommunication::startProcessImpl() {
#if defined(_WIN32)
    // Windows实现
    mcp_logger::getModuleLogger("ipc")->debug("Starting process on Windows");
    
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
        mcp_logger::getModuleLogger("ipc")->debug("Set environment variable: " + key + "=" + value);
    }
    
    std::string cmd_line = "cmd.exe /c " + config_.command;
    for (const auto& arg : config_.args) {
        cmd_line += " " + arg;
    }
    
    mcp_logger::getModuleLogger("ipc")->debug("Command line: " + cmd_line);
    
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
        mcp_logger::getModuleLogger("ipc")->info("Process started successfully on Windows, PID: " + std::to_string(process_id_));
        return true;
    } else {
        DWORD error = GetLastError();
        mcp_logger::getModuleLogger("ipc")->error("Failed to start process on Windows, error code: " + std::to_string(error));
    }
    
    return false;
#else
    // POSIX实现
    mcp_logger::getModuleLogger("ipc")->debug("Starting process on POSIX");
    
    process_id_ = fork();
    
    if (process_id_ == -1) {
        mcp_logger::getModuleLogger("ipc")->error("Failed to fork process on POSIX");
        return false;
    }
    
    if (process_id_ == 0) {
        // 子进程
        mcp_logger::getModuleLogger("ipc")->debug("Child process executing");
        
        // 设置环境变量
        for (const auto& [key, value] : config_.env_vars) {
            setenv(key.c_str(), value.c_str(), 1);
            mcp_logger::getModuleLogger("ipc")->debug("Set environment variable: " + key + "=" + value);
        }
        
        // 关闭父进程不需要的文件描述符
        // 子进程只需要stdin_pipe_[0]用于读取，stdout_pipe_[1]用于写入
        close(stdin_pipe_[1]);  // 关闭父进程的写端
        close(stdout_pipe_[0]); // 关闭父进程的读端
        
        // 重定向标准输入输出
        if (dup2(stdin_pipe_[0], STDIN_FILENO) == -1 ||
            dup2(stdout_pipe_[1], STDOUT_FILENO) == -1) {
            mcp_logger::getModuleLogger("ipc")->error("Failed to redirect stdio in child process");
            exit(EXIT_FAILURE);
        }
        
        // 关闭原始管道文件描述符（已经重定向到标准输入输出）
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
        mcp_logger::getModuleLogger("ipc")->debug("subprocecss Executing command: " + cmd_str);
        
        execvp(c_args[0], c_args.data());
        // execvp 失败时会设置 errno
        int err = errno;
        mcp_logger::getModuleLogger("ipc")->error("subproccess Failed to execute command: " + cmd_str + ", error: " + std::strerror(err) + " (errno: " + std::to_string(err) + ")");
        exit(EXIT_FAILURE);
    } else {
        // 父进程
        // 关闭子进程不需要的文件描述符
        // 父进程只需要stdin_pipe_[1]用于写入，stdout_pipe_[0]用于读取
        close(stdin_pipe_[0]);  // 关闭子进程的读端
        close(stdout_pipe_[1]); // 关闭子进程的写端
        
        // 等待一小段时间，检查子进程是否成功启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 检查子进程是否还活着（如果execvp失败，子进程会立即退出）
        int status;
        pid_t result = waitpid(process_id_, &status, WNOHANG);
        if (result == process_id_) {
            // 子进程已经退出，说明execvp失败
            std::string cmd_str = config_.command;
            for (const auto& arg : config_.args) {
                cmd_str += " " + arg;
            }
            std::string error_msg = "Child process exited immediately, command execution failed: " + cmd_str;
            if (WIFEXITED(status)) {
                error_msg += ", exit code: " + std::to_string(WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                error_msg += ", terminated by signal: " + std::to_string(WTERMSIG(status));
            }
            error_msg += ". Please check if the executable exists and is accessible.";
            mcp_logger::getModuleLogger("ipc")->error(error_msg);
            process_id_ = -1;
            return false;
        }
        
        mcp_logger::getModuleLogger("ipc")->info("Process started successfully on POSIX, PID: " + std::to_string(process_id_));
        return true;
    }
#endif
}

void PipeCommunication::stopProcessImpl() {
    mcp_logger::getModuleLogger("ipc")->debug("Stopping process, PID: " + std::to_string(process_id_));
    
#if defined(_WIN32)
    if (process_handle_) {
        mcp_logger::getModuleLogger("ipc")->debug("Terminating process on Windows");
        TerminateProcess(static_cast<HANDLE>(process_handle_), 0);
        DWORD wait_result = WaitForSingleObject(static_cast<HANDLE>(process_handle_), 2000);
        if (wait_result == WAIT_TIMEOUT) {
            mcp_logger::getModuleLogger("ipc")->warning("Process termination timeout on Windows");
        } else {
            mcp_logger::getModuleLogger("ipc")->debug("Process terminated successfully on Windows");
        }
        CloseHandle(static_cast<HANDLE>(process_handle_));
        process_handle_ = nullptr;
    }
#else
    if (process_id_ > 0) {
        mcp_logger::getModuleLogger("ipc")->debug("Sending SIGTERM to process on POSIX");
        if (kill(process_id_, SIGTERM) == 0) {
            // 等待进程退出，但设置超时
            int status;
            pid_t result = waitpid(process_id_, &status, WNOHANG);
            if (result == 0) {
                // 进程还在运行，等待最多2秒
                auto start_time = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() - start_time < std::chrono::seconds(2)) {
                    result = waitpid(process_id_, &status, WNOHANG);
                    if (result == process_id_) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                
                if (result == process_id_) {
                    if (WIFEXITED(status)) {
                        mcp_logger::getModuleLogger("ipc")->debug("Process exited normally with code: " + std::to_string(WEXITSTATUS(status)));
                    } else if (WIFSIGNALED(status)) {
                        mcp_logger::getModuleLogger("ipc")->debug("Process terminated by signal: " + std::to_string(WTERMSIG(status)));
                    }
                } else {
                    // 如果进程还没退出，发送SIGKILL强制终止
                    mcp_logger::getModuleLogger("ipc")->warning("Process did not exit gracefully, sending SIGKILL");
                    kill(process_id_, SIGKILL);
                    waitpid(process_id_, &status, 0);
                }
            } else if (result == process_id_) {
                if (WIFEXITED(status)) {
                    mcp_logger::getModuleLogger("ipc")->debug("Process exited normally with code: " + std::to_string(WEXITSTATUS(status)));
                } else if (WIFSIGNALED(status)) {
                    mcp_logger::getModuleLogger("ipc")->debug("Process terminated by signal: " + std::to_string(WTERMSIG(status)));
                }
            } else {
                mcp_logger::getModuleLogger("ipc")->warning("Failed to wait for process termination on POSIX");
            }
        } else {
            mcp_logger::getModuleLogger("ipc")->error("Failed to send SIGTERM to process on POSIX");
        }
    }
#endif
    process_id_ = -1;
    mcp_logger::getModuleLogger("ipc")->debug("Process stopped");
}

ssize_t PipeCommunication::readImpl(char* buffer, size_t size) {
    // 在服务器模式下，从 stdin_pipe_[0] (STDIN) 读取
    // 在客户端模式下，从 stdout_pipe_[0] (子进程的 stdout) 读取
    int fd = server_mode_ ? stdin_pipe_[0] : stdout_pipe_[0];
#if defined(_WIN32)
    DWORD bytes_read;
    BOOL success = ReadFile(reinterpret_cast<HANDLE>(fd), 
                           buffer, size - 1, &bytes_read, NULL);
    return success ? bytes_read : -1;
#else
    return read(fd, buffer, size - 1);
#endif
}

ssize_t PipeCommunication::writeImpl(const char* buffer, size_t size) {
    // 在服务器模式下，写入到 stdout_pipe_[1] (STDOUT)
    // 在客户端模式下，写入到 stdin_pipe_[1] (子进程的 stdin)
    int fd = server_mode_ ? stdout_pipe_[1] : stdin_pipe_[1];
#if defined(_WIN32)
    DWORD bytes_written;
    BOOL success = WriteFile(reinterpret_cast<HANDLE>(fd), 
                            buffer, size, &bytes_written, NULL);
    return success ? bytes_written : -1;
#else
    ssize_t result = write(fd, buffer, size);
    if (result == -1) {
        // 检查是否是管道断开错误
        if (errno == EPIPE) {
            mcp_logger::getModuleLogger("ipc")->warning("Pipe broken (EPIPE) - subprocess may have terminated");
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            mcp_logger::getModuleLogger("ipc")->debug("Write would block (EAGAIN/EWOULDBLOCK)");
        } else {
            mcp_logger::getModuleLogger("ipc")->error("Write error: " + std::string(strerror(errno)));
        }
    }
    return result;
#endif
}

void PipeCommunication::readThreadFunc() {
    std::vector<char> buffer(config_.buffer_size);
    std::string data_buffer;
    int consecutive_empty_reads = 0;
    const int max_empty_reads = 100; // 最大连续空读取次数
    const size_t max_buffer_size = 1024 * 1024; // 1MB 最大缓冲区大小，防止内存泄漏
    
    mcp_logger::getModuleLogger("ipc")->info("Read thread started");
    
    while (running_) {
        ssize_t bytes_read = readImpl(buffer.data(), config_.buffer_size);
        
        if (bytes_read > 0) {
            consecutive_empty_reads = 0; // 重置空读取计数
            buffer[bytes_read] = '\0';
            
            // 检查缓冲区大小限制，防止内存泄漏
            if (data_buffer.size() + bytes_read > max_buffer_size) {
                mcp_logger::getModuleLogger("ipc")->error("Data buffer size exceeded limit, clearing buffer");
                data_buffer.clear();
            }
            
            data_buffer.append(buffer.data(), bytes_read);
            
            // 处理完整的消息
            size_t pos = 0;
            while ((pos = data_buffer.find('\n')) != std::string::npos) {
                std::string line = data_buffer.substr(0, pos);
                data_buffer.erase(0, pos + 1);
                
                if (!line.empty()) {
                    // 记录处理的消息
                    printSubprocessOutput(line, "read_thread_func");
                    processMessage(line);
                }
            }
        } else if (bytes_read == 0) {
            // 管道关闭 - 这是正常的EOF条件
            // 在服务器模式下，这意味着stdin关闭，应该退出
            // 在客户端模式下，这意味着子进程的stdout关闭
            if (server_mode_) {
                mcp_logger::getModuleLogger("ipc")->info("Stdin closed (EOF received), server shutting down");
                running_ = false;  // 通知主循环退出
            } else {
                mcp_logger::getModuleLogger("ipc")->info("Subprocess pipe closed (EOF received)");
            }
            
            // 处理剩余的数据（如果有的话）
            if (!data_buffer.empty()) {
                printSubprocessOutput(data_buffer, "read_thread_func_final");
                processMessage(data_buffer);
                data_buffer.clear();
            }
            break;
        } else {
            // 错误或暂时无数据 (EAGAIN/EWOULDBLOCK in non-blocking mode)
            consecutive_empty_reads++;
            
            if (consecutive_empty_reads > max_empty_reads) {
                if (consecutive_empty_reads % 2 == 0) {
                    mcp_logger::getModuleLogger("ipc")->warning("Too many consecutive empty reads, checking process status");
                }
                // 检查子进程是否还活着
                if (!isProcessAlive()) {
                    mcp_logger::getModuleLogger("ipc")->warning("Subprocess is no longer alive, stopping read thread");
                    break;
                }
                
                consecutive_empty_reads = 0; // 重置计数，继续尝试
            }
            
            // 使用配置的超时时间，但不要无限等待
            std::this_thread::sleep_for(std::chrono::milliseconds(config_.read_timeout_ms));
        }
    }
    
    mcp_logger::getModuleLogger("ipc")->info("Read thread finished");
}

void PipeCommunication::processMessage(const std::string& line) {
    // 检查是否是有效的 JSON 格式（以 { 开头）
    // 服务器的日志输出可能被误读，先检查是否是 JSON 格式
    std::string trimmed_line = line;
    
    // 去除前后空白字符
    size_t start = trimmed_line.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        // 空消息，忽略
        return;
    }
    size_t end = trimmed_line.find_last_not_of(" \t\r\n");
    trimmed_line = trimmed_line.substr(start, end - start + 1);
    
    // 检查是否看起来像 JSON（以 { 开头）
    if (trimmed_line.empty() || trimmed_line[0] != '{') {
        // 不是 JSON 格式，可能是日志输出，静默忽略
        return;
    }
    
    try {
        nlohmann::json message = nlohmann::json::parse(trimmed_line);
        
        if (message.contains("jsonrpc") && message["jsonrpc"] == "2.0") {
            if (server_mode_) {
                // 服务端模式：处理来自客户端的请求和通知
                if (message.contains("id") && !message["id"].is_null()) {
                    // 这是一个请求
                    handleServerRequest(message);
                } else if (message.contains("method")) {
                    // 这是一个通知
                    handleServerNotification(message);
                }
            } else {
                // 客户端模式：处理来自服务端的响应
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
                        mcp_logger::getModuleLogger("ipc")->debug("pip_communication 收到响应: id: " + id + ", result: " + message["result"].dump());
                        handleResponse(id, message["result"].dump());
                    } else if (message.contains("error")) {
                        mcp_logger::getModuleLogger("ipc")->warning("Received error response: id=" + id + ", error=" + message["error"].dump());
                        handleError(id, message["error"].dump());
                    }
                } else if (message.contains("method")) {
                    // 这是一个通知
                    Message msg;
                    msg.type = MessageType::NOTIFICATION;
                    msg.content = trimmed_line;
                    
                    // 添加到消息队列
                    {
                        std::lock_guard<std::mutex> lock(message_mutex_);
                        message_queue_.push(msg);
                    }
                    
                    if (message_handler_) {
                        message_handler_->onMessage(msg);
                    }
                }
            }
        } else {
            // 不是JSON-RPC格式，可能是其他 JSON 消息，静默忽略（可能是日志输出）
            // 只有在调试模式下才记录
            mcp_logger::getModuleLogger("ipc")->debug("Received non-JSON-RPC message (likely log output): " + trimmed_line.substr(0, 100));
        }
    } catch (const nlohmann::json::exception& e) {
        // JSON解析失败，可能是日志输出或格式错误，静默忽略
        // 只有在调试模式下才记录
        mcp_logger::getModuleLogger("ipc")->debug("Failed to parse message as JSON (likely log output): " + trimmed_line.substr(0, 100));
    }
}

std::string PipeCommunication::generateRequestId() {
    return std::to_string(++request_id_counter_);
}

void PipeCommunication::handleResponse(const std::string& id, const std::string& content) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    auto it = pending_requests_.find(id);
    if (it != pending_requests_.end()) {
        it->second.set_value(content);
        pending_requests_.erase(it);
    }
}

void PipeCommunication::handleError(const std::string& id, const std::string& error) {
    std::lock_guard<std::mutex> lock(response_mutex_);
    auto it = pending_requests_.find(id);
    if (it != pending_requests_.end()) {
        it->second.set_value(error);
        pending_requests_.erase(it);
    }
}

bool PipeCommunication::hasResult() const {
    std::lock_guard<std::mutex> lock(result_mutex_);
    return !result_queue_.empty();
}

std::string PipeCommunication::getResult() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    if (result_queue_.empty()) {
        return "";
    }
    std::string result = result_queue_.front();
    result_queue_.pop();
    return result;
}

std::vector<std::string> PipeCommunication::getAllResults() {
    std::lock_guard<std::mutex> lock(result_mutex_);
    std::vector<std::string> results;
    while (!result_queue_.empty()) {
        results.push_back(result_queue_.front());
        result_queue_.pop();
    }
    return results;
}

bool PipeCommunication::hasMessage() const {
    std::lock_guard<std::mutex> lock(message_mutex_);
    return !message_queue_.empty();
}

Message PipeCommunication::getMessage() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    if (message_queue_.empty()) {
        return Message{MessageType::NOTIFICATION, "", ""};
    }
    
    Message msg = message_queue_.front();
    message_queue_.pop();
    return msg;
}

std::vector<Message> PipeCommunication::getAllMessages() {
    std::lock_guard<std::mutex> lock(message_mutex_);
    std::vector<Message> messages;
    
    while (!message_queue_.empty()) {
        messages.push_back(message_queue_.front());
        message_queue_.pop();
    }
    
    return messages;
}

void PipeCommunication::printSubprocessOutput(const std::string& output, const std::string& source) {
    // 使用统一的logger模块输出子进程日志
    std::string subprocess_msg = "[subprocess " + source + "] " + output;
    mcp_logger::getModuleLogger("ipc")->info(subprocess_msg);
}

// 服务端模式相关方法实现
void PipeCommunication::setServerRequestHandler(std::shared_ptr<ServerRequestHandler> handler) {
    server_request_handler_ = handler;
}

bool PipeCommunication::sendResponse(const std::string& request_id, const std::string& result) {
    if (!running_ || !server_mode_) {
        mcp_logger::getModuleLogger("ipc")->error("Cannot send response: not in server mode or not running");
        return false;
    }
    
    try {
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = request_id;
        response["result"] = nlohmann::json::parse(result);
        
        std::string response_str = response.dump() + "\n";
        ssize_t bytes_written = writeImpl(response_str.c_str(), response_str.size());
        
        bool success = bytes_written == static_cast<ssize_t>(response_str.size());
        if (success) {
            mcp_logger::getModuleLogger("ipc")->debug("Response sent successfully for request ID: " + request_id);
        } else {
            mcp_logger::getModuleLogger("ipc")->error("Failed to send response for request ID: " + request_id);
        }
        
        return success;
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc")->error("Error creating response JSON: " + std::string(e.what()));
        return false;
    }
}

bool PipeCommunication::sendError(const std::string& request_id, int error_code, const std::string& error_message) {
    if (!running_ || !server_mode_) {
        mcp_logger::getModuleLogger("ipc")->error("Cannot send error: not in server mode or not running");
        return false;
    }
    
    try {
        nlohmann::json error_response;
        error_response["jsonrpc"] = "2.0";
        error_response["id"] = request_id;
        
        nlohmann::json error_obj;
        error_obj["code"] = error_code;
        error_obj["message"] = error_message;
        error_response["error"] = error_obj;
        
        std::string error_str = error_response.dump() + "\n";
        ssize_t bytes_written = writeImpl(error_str.c_str(), error_str.size());
        
        bool success = bytes_written == static_cast<ssize_t>(error_str.size());
        if (success) {
            mcp_logger::getModuleLogger("ipc")->debug("Error response sent successfully for request ID: " + request_id);
        } else {
            mcp_logger::getModuleLogger("ipc")->error("Failed to send error response for request ID: " + request_id);
        }
        
        return success;
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc")->error("Error creating error response JSON: " + std::string(e.what()));
        return false;
    }
}

bool PipeCommunication::sendNotification(const std::string& method, const std::string& params) {
    if (!running_ || !server_mode_) {
        mcp_logger::getModuleLogger("ipc")->error("Cannot send notification: not in server mode or not running");
        return false;
    }
    
    try {
        nlohmann::json notification;
        notification["jsonrpc"] = "2.0";
        notification["method"] = method;
        if (!params.empty()) {
            notification["params"] = nlohmann::json::parse(params);
        }
        
        std::string notification_str = notification.dump() + "\n";
        ssize_t bytes_written = writeImpl(notification_str.c_str(), notification_str.size());
        
        bool success = bytes_written == static_cast<ssize_t>(notification_str.size());
        if (success) {
            mcp_logger::getModuleLogger("ipc")->debug("Notification sent successfully: " + method);
        } else {
            mcp_logger::getModuleLogger("ipc")->error("Failed to send notification: " + method);
        }
        
        return success;
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc")->error("Error creating notification JSON: " + std::string(e.what()));
        return false;
    }
}

bool PipeCommunication::initializeServerMode() {
    mcp_logger::getModuleLogger("ipc")->info("Initializing server mode");
    
    // 在服务端模式下，我们直接使用标准输入输出
    // stdin_pipe_[0] 用于读取（从 STDIN 读取客户端请求）
    // stdout_pipe_[1] 用于写入（写入到 STDOUT 发送响应给客户端）
    stdin_pipe_[0] = STDIN_FILENO;
    stdin_pipe_[1] = -1;  // 服务器模式不需要写入 stdin
    stdout_pipe_[0] = -1;  // 服务器模式不需要读取 stdout
    stdout_pipe_[1] = STDOUT_FILENO;
    
    running_ = true;
    read_thread_ = std::make_unique<std::thread>(&PipeCommunication::readThreadFunc, this);
    
    mcp_logger::getModuleLogger("ipc")->info("Server mode initialized successfully");
    return true;
}

void PipeCommunication::handleServerRequest(const nlohmann::json& request) {
    if (!server_request_handler_) {
        mcp_logger::getModuleLogger("ipc")->warning("No server request handler set, ignoring request");
        return;
    }
    
    try {
        std::string method = request["method"].get<std::string>();
        std::string request_id = "";
        
        if (request.contains("id") && !request["id"].is_null()) {
            if (request["id"].is_string()) {
                request_id = request["id"].get<std::string>();
            } else if (request["id"].is_number()) {
                request_id = std::to_string(request["id"].get<int>());
            } else {
                request_id = request["id"].dump();
            }
        }
        
        std::string params = "";
        if (request.contains("params")) {
            params = request["params"].dump();
        }
        
        mcp_logger::getModuleLogger("ipc")->debug("Handling server request: " + method + " (ID: " + request_id + ")");
        
        std::string result = server_request_handler_->handleRequest(method, params, request_id);
        
        if (!request_id.empty()) {
            sendResponse(request_id, result);
        }
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc")->error("Error handling server request: " + std::string(e.what()));
        
        if (request.contains("id") && !request["id"].is_null()) {
            std::string request_id = request["id"].dump();
            sendError(request_id, -32603, "Internal error: " + std::string(e.what()));
        }
    }
}

void PipeCommunication::handleServerNotification(const nlohmann::json& notification) {
    if (!server_request_handler_) {
        mcp_logger::getModuleLogger("ipc")->warning("No server request handler set, ignoring notification");
        return;
    }
    
    try {
        std::string method = notification["method"].get<std::string>();
        std::string params = "";
        
        if (notification.contains("params")) {
            params = notification["params"].dump();
        }
        
        mcp_logger::getModuleLogger("ipc")->debug("Handling server notification: " + method);
        server_request_handler_->handleNotification(method, params);
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("ipc")->error("Error handling server notification: " + std::string(e.what()));
    }
}

} // namespace mcp
