#include "logger.h"
#include <thread>
#include <sstream>
#include <filesystem>
#include <algorithm>

namespace mcp_logger {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::~Logger() {
    shutdown();
}

void Logger::initialize(const LogConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return;
    }
    
    config_ = config;
    
    if (config_.target == LogTarget::FILE || config_.target == LogTarget::BOTH) {
        openLogFile();
    }
    
    initialized_ = true;
    
    // 直接输出初始化成功消息，不通过日志系统
    std::cout << "Logger initialized successfully" << std::endl;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.level = level;
}

void Logger::setTarget(LogTarget target) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_.target == LogTarget::FILE || config_.target == LogTarget::BOTH) {
        closeLogFile();
    }
    
    config_.target = target;
    
    if (config_.target == LogTarget::FILE || config_.target == LogTarget::BOTH) {
        openLogFile();
    }
}

void Logger::setLogFile(const std::string& file_path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_.target == LogTarget::FILE || config_.target == LogTarget::BOTH) {
        closeLogFile();
    }
    
    config_.file_path = file_path;
    
    if (config_.target == LogTarget::FILE || config_.target == LogTarget::BOTH) {
        openLogFile();
    }
}

void Logger::enableTimestamp(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.enable_timestamp = enable;
}

void Logger::enableThreadId(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.enable_thread_id = enable;
}

void Logger::enableFileLocation(bool enable) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.enable_file_location = enable;
}

void Logger::setFormat(const std::string& format) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_.format = format;
}

void Logger::log(LogLevel level, const std::string& message, 
                 const std::string& file, int line, const std::string& function) {
    if (level < config_.level) {
        return;
    }
    
    std::string formatted_message = formatMessage(level, message, file, line, function);
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (config_.target == LogTarget::CONSOLE || config_.target == LogTarget::BOTH) {
        std::cout << formatted_message << std::endl;
    }
    
    if ((config_.target == LogTarget::FILE || config_.target == LogTarget::BOTH) && log_file_.is_open()) {
        log_file_ << formatted_message << std::endl;
        if (config_.auto_flush) {
            log_file_.flush();
        }
    }
}

void Logger::debug(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::DEBUG_LEVEL, message, file, line, function);
}

void Logger::info(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::INFO, message, file, line, function);
}

void Logger::warning(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::WARNING, message, file, line, function);
}

void Logger::error(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::ERROR, message, file, line, function);
}

void Logger::fatal(const std::string& message, const std::string& file, int line, const std::string& function) {
    log(LogLevel::FATAL, message, file, line, function);
}

std::shared_ptr<Logger::ModuleLogger> Logger::getModuleLogger(const std::string& module_name) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = module_loggers_.find(module_name);
    if (it != module_loggers_.end()) {
        return it->second;
    }
    
    auto module_logger = std::make_shared<ModuleLogger>(module_name, this);
    module_loggers_[module_name] = module_logger;
    return module_logger;
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (log_file_.is_open()) {
        log_file_.close();
    }
    
    module_loggers_.clear();
    initialized_ = false;
}

std::string Logger::formatMessage(LogLevel level, const std::string& message, 
                                 const std::string& file, int line, const std::string& function) const {
    std::string result = config_.format;
    
    // 替换时间戳
    if (config_.enable_timestamp) {
        std::string timestamp = getTimestamp();
        size_t pos = result.find("{timestamp}");
        if (pos != std::string::npos) {
            result.replace(pos, 11, timestamp);
        }
    }
    
    // 替换日志级别
    std::string level_str = levelToString(level);
    size_t pos = result.find("{level}");
    if (pos != std::string::npos) {
        result.replace(pos, 7, level_str);
    }
    
    // 替换消息
    pos = result.find("{message}");
    if (pos != std::string::npos) {
        result.replace(pos, 9, message);
    }
    
    // 替换线程ID
    if (config_.enable_thread_id) {
        std::string thread_id = getThreadId();
        pos = result.find("{thread_id}");
        if (pos != std::string::npos) {
            result.replace(pos, 11, thread_id);
        }
    }
    
    // 替换文件位置
    if (config_.enable_file_location) {
        std::string file_location = getFileLocation(file, line, function);
        pos = result.find("{file_location}");
        if (pos != std::string::npos) {
            result.replace(pos, 15, file_location);
        }
    }
    
    return result;
}

std::string Logger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

std::string Logger::getThreadId() const {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG_LEVEL:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:                return "UNKNOWN";
    }
}

std::string Logger::getFileLocation(const std::string& file, int line, const std::string& function) const {
    if (file.empty()) {
        return "";
    }
    
    std::string filename = std::filesystem::path(file).filename().string();
    return filename + ":" + std::to_string(line) + ":" + function;
}

void Logger::rotateLogFile() {
    if (config_.file_path.empty()) {
        return;
    }
    
    // 检查文件大小
    std::ifstream file(config_.file_path, std::ios::binary | std::ios::ate);
    if (file.is_open()) {
        std::streamsize size = file.tellg();
        file.close();
        
        if (size >= static_cast<std::streamsize>(config_.max_file_size)) {
            // 重命名现有文件
            for (int i = config_.max_files - 1; i > 0; --i) {
                std::string old_name = config_.file_path + "." + std::to_string(i);
                std::string new_name = config_.file_path + "." + std::to_string(i + 1);
                
                if (std::filesystem::exists(old_name)) {
                    std::filesystem::rename(old_name, new_name);
                }
            }
            
            std::string backup_name = config_.file_path + ".1";
            std::filesystem::rename(config_.file_path, backup_name);
            
            // 重新打开文件
            openLogFile();
        }
    }
}

void Logger::openLogFile() {
    if (config_.file_path.empty()) {
        return;
    }
    
    // 确保目录存在
    std::filesystem::path file_path(config_.file_path);
    std::filesystem::path dir = file_path.parent_path();
    if (!dir.empty() && !std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }
    
    log_file_.open(config_.file_path, std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file: " << config_.file_path << std::endl;
    }
}

void Logger::closeLogFile() {
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

// ModuleLogger 实现
Logger::ModuleLogger::ModuleLogger(const std::string& module_name, Logger* parent)
    : module_name_(module_name), parent_(parent) {
}

void Logger::ModuleLogger::debug(const std::string& message, const std::string& file, int line, const std::string& function) {
    parent_->log(LogLevel::DEBUG_LEVEL, "[" + module_name_ + "] " + message, file, line, function);
}

void Logger::ModuleLogger::info(const std::string& message, const std::string& file, int line, const std::string& function) {
    parent_->log(LogLevel::INFO, "[" + module_name_ + "] " + message, file, line, function);
}

void Logger::ModuleLogger::warning(const std::string& message, const std::string& file, int line, const std::string& function) {
    parent_->log(LogLevel::WARNING, "[" + module_name_ + "] " + message, file, line, function);
}

void Logger::ModuleLogger::error(const std::string& message, const std::string& file, int line, const std::string& function) {
    parent_->log(LogLevel::ERROR, "[" + module_name_ + "] " + message, file, line, function);
}

void Logger::ModuleLogger::fatal(const std::string& message, const std::string& file, int line, const std::string& function) {
    parent_->log(LogLevel::FATAL, "[" + module_name_ + "] " + message, file, line, function);
}

} // namespace mcp_logger
