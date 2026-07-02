#include "../log/logger.h"
#include <iostream>

using namespace mcp_logger;

int main() {
    // 初始化日志系统
    Logger& logger = Logger::getInstance();
    LogConfig config;
    config.level = LogLevel::INFO;
    config.target = LogTarget::BOTH;
    config.file_path = "test_log_integration.log";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    logger.initialize(config);
    
    // 测试基本日志功能
    MCP_LOG_INFO("Transport模块日志集成测试开始");
    MCP_LOG_DEBUG("这是一条调试信息");
    MCP_LOG_INFO("这是一条信息日志");
    MCP_LOG_WARNING("这是一条警告信息");
    MCP_LOG_ERROR("这是一条错误信息");
    MCP_LOG_FATAL("这是一条致命错误信息");
    
    // 测试模块化日志
    auto transportLogger = logger.getModuleLogger("Transport");
    transportLogger->info("Transport模块日志记录器测试");
    
    MCP_LOG_INFO("Transport模块日志集成测试完成");
    
    return 0;
}
