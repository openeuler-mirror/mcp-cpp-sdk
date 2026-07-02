#include "logger.h"
#include <thread>
#include <chrono>
#include <vector>

using namespace mcp_logger;

// 测试函数，用于演示日志功能
void testBasicLogging() {
    std::cout << "\n=== 测试基本日志功能 ===" << std::endl;
    
    // 获取日志实例并初始化
    Logger& logger = Logger::getInstance();
    
    // 配置日志系统
    LogConfig config;
    config.level = LogLevel::DEBUG_LEVEL;
    config.target = LogTarget::BOTH;  // 同时输出到控制台和文件
    config.file_path = "test_log.txt";
    config.enable_timestamp = true;
    config.enable_thread_id = true;
    config.enable_file_location = true;
    config.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    
    logger.initialize(config);
    
    // 测试各种日志级别
    MCP_LOG_DEBUG("这是一条调试信息");
    MCP_LOG_INFO("这是一条信息日志");
    MCP_LOG_WARNING("这是一条警告信息");
    MCP_LOG_ERROR("这是一条错误信息");
    MCP_LOG_FATAL("这是一条致命错误信息");
    
    std::cout << "基本日志功能测试完成" << std::endl;
}

void testModuleLogging() {
    std::cout << "\n=== 测试模块化日志功能 ===" << std::endl;
    
    Logger& logger = Logger::getInstance();
    
    // 获取不同模块的日志记录器
    auto networkLogger = logger.getModuleLogger("Network");
    auto databaseLogger = logger.getModuleLogger("Database");
    auto authLogger = logger.getModuleLogger("Auth");
    
    // 使用模块日志记录器
    networkLogger->info("网络连接已建立");
    networkLogger->warning("网络延迟较高");
    
    databaseLogger->info("数据库连接成功");
    databaseLogger->error("数据库查询失败");
    
    authLogger->info("用户登录成功");
    authLogger->warning("密码强度不足");
    
    std::cout << "模块化日志功能测试完成" << std::endl;
}

void testThreadSafety() {
    std::cout << "\n=== 测试线程安全性 ===" << std::endl;
    
    Logger& logger = Logger::getInstance();
    
    // 创建多个线程同时写入日志
    std::vector<std::thread> threads;
    
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([i, &logger]() {
            for (int j = 0; j < 10; ++j) {
                MCP_LOG_INFO("线程 " + std::to_string(i) + " 第 " + std::to_string(j) + " 条消息");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    std::cout << "线程安全性测试完成" << std::endl;
}

void testLogRotation() {
    std::cout << "\n=== 测试日志轮转功能 ===" << std::endl;
    
    Logger& logger = Logger::getInstance();
    
    // 设置较小的文件大小限制以触发轮转
    LogConfig config = logger.getConfig();
    config.max_file_size = 1024;  // 1KB
    config.max_files = 3;
    logger.initialize(config);
    
    // 写入大量日志以触发轮转
    for (int i = 0; i < 100; ++i) {
        MCP_LOG_INFO("这是一条用于测试日志轮转的消息，编号: " + std::to_string(i));
    }
    
    std::cout << "日志轮转功能测试完成" << std::endl;
}

void testDifferentFormats() {
    std::cout << "\n=== 测试不同日志格式 ===" << std::endl;
    
    Logger& logger = Logger::getInstance();
    
    // 测试简单格式
    logger.setFormat("[{level}] {message}");
    logger.enableTimestamp(false);
    logger.enableThreadId(false);
    logger.enableFileLocation(false);
    MCP_LOG_INFO("简单格式日志");
    
    // 测试详细格式
    logger.setFormat("[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}");
    logger.enableTimestamp(true);
    logger.enableThreadId(true);
    logger.enableFileLocation(true);
    MCP_LOG_INFO("详细格式日志");
    
    // 测试自定义格式
    logger.setFormat("*** {level} *** {message} ***");
    MCP_LOG_WARNING("自定义格式日志");
    
    std::cout << "不同日志格式测试完成" << std::endl;
}

void testLogLevels() {
    std::cout << "\n=== 测试日志级别过滤 ===" << std::endl;
    
    Logger& logger = Logger::getInstance();
    
    // 设置不同的日志级别
    std::vector<LogLevel> levels = {
        LogLevel::DEBUG_LEVEL,
        LogLevel::INFO,
        LogLevel::WARNING,
        LogLevel::ERROR,
        LogLevel::FATAL
    };
    
    for (auto level : levels) {
        std::cout << "\n设置日志级别为: " << static_cast<int>(level) << std::endl;
        logger.setLevel(level);
        
        MCP_LOG_DEBUG("调试信息");
        MCP_LOG_INFO("信息日志");
        MCP_LOG_WARNING("警告信息");
        MCP_LOG_ERROR("错误信息");
        MCP_LOG_FATAL("致命错误");
    }
    
    std::cout << "日志级别过滤测试完成" << std::endl;
}

int main() {
    std::cout << "MCP Logger 测试程序开始" << std::endl;
    std::cout << "=========================" << std::endl;
    
    try {
        // 运行各种测试
        testBasicLogging();
        testModuleLogging();
        testThreadSafety();
        testLogRotation();
        testDifferentFormats();
        testLogLevels();
        
        std::cout << "\n=========================" << std::endl;
        std::cout << "所有测试完成！" << std::endl;
        std::cout << "请检查 test_log.txt 文件查看文件输出结果" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    // 清理资源
    Logger::getInstance().shutdown();
    
    return 0;
}
