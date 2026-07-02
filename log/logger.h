#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <memory>
#include <map>
#include <functional>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <iostream>

namespace mcp_logger {

// 日志级别枚举
enum class LogLevel {
    DEBUG_LEVEL = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    FATAL = 4
};

// 日志输出目标
enum class LogTarget {
    CONSOLE = 0,    // 控制台输出
    FILE = 1,       // 文件输出
    BOTH = 2        // 同时输出到控制台和文件
};

// 日志配置结构
struct LogConfig {
    LogLevel level = LogLevel::INFO;
    LogTarget target = LogTarget::CONSOLE;
    std::string file_path = "";
    bool enable_timestamp = true;
    bool enable_thread_id = false;
    bool enable_file_location = false;
    bool auto_flush = true;
    size_t max_file_size = 10 * 1024 * 1024; // 10MB
    int max_files = 5; // 最多保留5个日志文件
    std::string format = "[{timestamp}] [{level}] {message}";
};

// 全局日志管理器
class Logger {
public:
    // 单例模式
    static Logger& getInstance();
    
    // 初始化日志系统
    void initialize(const LogConfig& config);
    
    // 设置全局配置
    void setLevel(LogLevel level);
    void setTarget(LogTarget target);
    void setLogFile(const std::string& file_path);
    void enableTimestamp(bool enable);
    void enableThreadId(bool enable);
    void enableFileLocation(bool enable);
    void setFormat(const std::string& format);
    
    // 日志记录方法
    void log(LogLevel level, const std::string& message, 
             const std::string& file = "", int line = 0, const std::string& function = "");
    
    // 便捷方法
    void debug(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
    void info(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
    void warning(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
    void error(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
    void fatal(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
    
    // 模块化日志记录器
    class ModuleLogger {
    public:
        ModuleLogger(const std::string& module_name, Logger* parent);
        
        void debug(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void info(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void warning(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void error(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        void fatal(const std::string& message, const std::string& file = "", int line = 0, const std::string& function = "");
        
    private:
        std::string module_name_;
        Logger* parent_;
    };
    
    // 获取模块日志记录器
    std::shared_ptr<ModuleLogger> getModuleLogger(const std::string& module_name);
    
    // 清理资源
    void shutdown();
    
    // 获取当前配置
    LogConfig getConfig() const { return config_; }
    
private:
    Logger() = default;
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    // 内部方法
    std::string formatMessage(LogLevel level, const std::string& message, 
                             const std::string& file, int line, const std::string& function) const;
    std::string getTimestamp() const;
    std::string getThreadId() const;
    std::string levelToString(LogLevel level) const;
    std::string getFileLocation(const std::string& file, int line, const std::string& function) const;
    
    // 文件管理
    void rotateLogFile();
    void openLogFile();
    void closeLogFile();
    
    // 成员变量
    mutable std::mutex mutex_;
    LogConfig config_;
    std::ofstream log_file_;
    std::map<std::string, std::shared_ptr<ModuleLogger>> module_loggers_;
    bool initialized_ = false;
};

// 全局便捷宏定义
#define MCP_LOG_DEBUG(msg) mcp_logger::Logger::getInstance().debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_INFO(msg) mcp_logger::Logger::getInstance().info(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_WARNING(msg) mcp_logger::Logger::getInstance().warning(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_ERROR(msg) mcp_logger::Logger::getInstance().error(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_LOG_FATAL(msg) mcp_logger::Logger::getInstance().fatal(msg, __FILE__, __LINE__, __FUNCTION__)

// 模块化日志宏定义
#define MCP_MODULE_LOG_DEBUG(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->debug(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_MODULE_LOG_INFO(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->info(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_MODULE_LOG_WARNING(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->warning(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_MODULE_LOG_ERROR(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->error(msg, __FILE__, __LINE__, __FUNCTION__)
#define MCP_MODULE_LOG_FATAL(module, msg) mcp_logger::Logger::getInstance().getModuleLogger(module)->fatal(msg, __FILE__, __LINE__, __FUNCTION__)

} // namespace mcp_logger
