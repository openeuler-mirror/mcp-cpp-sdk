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

// 取消可能存在的DEBUG宏定义，避免与LogLevel::DEBUG冲突
#ifdef DEBUG
#undef DEBUG
#endif
#ifdef INFO
#undef INFO
#endif

namespace mcp_logger {

// 日志级别枚举
enum class LogLevel {
    DEBUG = 0,
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
    bool enable_thread_id = true;
    bool enable_file_location = true;
    bool auto_flush = true;
    size_t max_file_size = 10 * 1024 * 1024; // 10MB
    int max_files = 5; // 最多保留5个日志文件
    std::string format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
};

// 全局日志管理器
class Logger {
public:
    // 单例模式
    static Logger& getInstance();
    
    // 获取默认标准日志格式
    static std::string getDefaultStandardFormat();
    
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
    // 统一日志格式设置（全局统一格式）
    void setUnifiedFormat(const std::string& format);
    
    // 应用默认标准格式
    void applyDefaultStandardFormat();
    
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
    // 若设置，则在创建 ModuleLogger 时统一应用该格式
    std::string unified_format_;
};

// 注意：已移除所有宏定义，请使用 module_logger_ 方式进行日志记录
// 示例：
// auto module_logger_ = mcp_logger::Logger::getInstance().getModuleLogger("your_module");
// module_logger_->debug("debug message");
// module_logger_->info("info message");
// module_logger_->warning("warning message");
// module_logger_->error("error message");
// module_logger_->fatal("fatal message");

// 全局便捷函数
// 获取模块日志记录器（支持参数传入模块名称）
std::shared_ptr<Logger::ModuleLogger> getModuleLogger(const std::string& module_name);

// 初始化日志系统（便捷函数）
void initializeLogger(const LogConfig& config);

} // namespace mcp_logger
