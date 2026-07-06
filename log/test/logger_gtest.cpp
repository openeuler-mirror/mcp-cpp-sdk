/**
 * @file logger_gtest.cpp
 * @brief Logger Google Test 测试文件
 */

#include <gtest/gtest.h>
#include "logger.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>

using namespace mcp_logger;

class LoggerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 清理之前的日志文件
        cleanupTestFiles();
        
        // 重置Logger状态
        Logger::getInstance().shutdown();
        
        // 设置测试配置
        test_config_.level = LogLevel::DEBUG;
        test_config_.target = LogTarget::CONSOLE;
        test_config_.file_path = "test_logger_gtest.log";
        test_config_.enable_timestamp = true;
        test_config_.enable_thread_id = true;
        test_config_.enable_file_location = true;
        test_config_.auto_flush = true;
        test_config_.max_file_size = 1024; // 1KB for testing
        test_config_.max_files = 3;
        test_config_.format = "[{timestamp}] [{level}] [{thread_id}] {file_location} - {message}";
    }

    void TearDown() override {
        // 清理测试文件
        cleanupTestFiles();
        
        // 关闭Logger
        Logger::getInstance().shutdown();
    }

    void cleanupTestFiles() {
        std::vector<std::string> files_to_remove = {
            "test_logger_gtest.log",
            "test_logger_gtest.log.1",
            "test_logger_gtest.log.2",
            "test_logger_gtest.log.3",
            "test_console.log",
            "test_file.log",
            "test_both.log"
        };
        
        for (const auto& file : files_to_remove) {
            if (std::filesystem::exists(file)) {
                std::filesystem::remove(file);
            }
        }
    }

    LogConfig test_config_;
};

// 测试Logger单例模式
TEST_F(LoggerTest, SingletonPattern) {
    Logger& logger1 = Logger::getInstance();
    Logger& logger2 = Logger::getInstance();
    
    EXPECT_EQ(&logger1, &logger2);
}

// 测试Logger初始化
TEST_F(LoggerTest, Initialize) {
    Logger& logger = Logger::getInstance();
    
    // 测试初始化
    logger.initialize(test_config_);
    EXPECT_TRUE(logger.getConfig().enable_timestamp);
    EXPECT_TRUE(logger.getConfig().enable_thread_id);
    EXPECT_TRUE(logger.getConfig().enable_file_location);
    EXPECT_EQ(logger.getConfig().level, LogLevel::DEBUG);
    EXPECT_EQ(logger.getConfig().target, LogTarget::CONSOLE);
}

// 测试重复初始化
TEST_F(LoggerTest, DuplicateInitialize) {
    Logger& logger = Logger::getInstance();
    
    // 第一次初始化
    logger.initialize(test_config_);
    LogConfig config1 = logger.getConfig();
    
    // 第二次初始化应该被忽略
    LogConfig new_config = test_config_;
    new_config.level = LogLevel::ERROR;
    logger.initialize(new_config);
    LogConfig config2 = logger.getConfig();
    
    // 配置应该没有改变
    EXPECT_EQ(config1.level, config2.level);
}

// 测试日志级别设置
TEST_F(LoggerTest, SetLevel) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    // 测试设置不同级别
    logger.setLevel(LogLevel::INFO);
    EXPECT_EQ(logger.getConfig().level, LogLevel::INFO);
    
    logger.setLevel(LogLevel::ERROR);
    EXPECT_EQ(logger.getConfig().level, LogLevel::ERROR);
    
    logger.setLevel(LogLevel::DEBUG);
    EXPECT_EQ(logger.getConfig().level, LogLevel::DEBUG);
}

// 测试日志目标设置
TEST_F(LoggerTest, SetTarget) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    // 测试设置不同目标
    logger.setTarget(LogTarget::FILE);
    EXPECT_EQ(logger.getConfig().target, LogTarget::FILE);
    
    logger.setTarget(LogTarget::BOTH);
    EXPECT_EQ(logger.getConfig().target, LogTarget::BOTH);
    
    logger.setTarget(LogTarget::CONSOLE);
    EXPECT_EQ(logger.getConfig().target, LogTarget::CONSOLE);
}

// 测试日志文件设置
TEST_F(LoggerTest, SetLogFile) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    // 测试设置日志文件
    std::string new_file = "test_new_file.log";
    logger.setLogFile(new_file);
    EXPECT_EQ(logger.getConfig().file_path, new_file);
    
    // 清理
    if (std::filesystem::exists(new_file)) {
        std::filesystem::remove(new_file);
    }
}

// 测试时间戳开关
TEST_F(LoggerTest, EnableTimestamp) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    logger.enableTimestamp(false);
    EXPECT_FALSE(logger.getConfig().enable_timestamp);
    
    logger.enableTimestamp(true);
    EXPECT_TRUE(logger.getConfig().enable_timestamp);
}

// 测试线程ID开关
TEST_F(LoggerTest, EnableThreadId) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    logger.enableThreadId(false);
    EXPECT_FALSE(logger.getConfig().enable_thread_id);
    
    logger.enableThreadId(true);
    EXPECT_TRUE(logger.getConfig().enable_thread_id);
}

// 测试文件位置开关
TEST_F(LoggerTest, EnableFileLocation) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    logger.enableFileLocation(false);
    EXPECT_FALSE(logger.getConfig().enable_file_location);
    
    logger.enableFileLocation(true);
    EXPECT_TRUE(logger.getConfig().enable_file_location);
}

// 测试格式设置
TEST_F(LoggerTest, SetFormat) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    std::string new_format = "{level} - {message}";
    logger.setFormat(new_format);
    EXPECT_EQ(logger.getConfig().format, new_format);
}

// 测试统一格式设置
TEST_F(LoggerTest, SetUnifiedFormat) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    std::string unified_format = "{level} [{module}] {message}";
    logger.setUnifiedFormat(unified_format);
    EXPECT_EQ(logger.getConfig().format, unified_format);
}

// 测试应用默认标准格式
TEST_F(LoggerTest, ApplyDefaultStandardFormat) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    logger.applyDefaultStandardFormat();
    EXPECT_EQ(logger.getConfig().format, Logger::getDefaultStandardFormat());
    EXPECT_TRUE(logger.getConfig().enable_timestamp);
    EXPECT_TRUE(logger.getConfig().enable_thread_id);
    EXPECT_TRUE(logger.getConfig().enable_file_location);
}

// 测试获取默认标准格式
TEST_F(LoggerTest, GetDefaultStandardFormat) {
    std::string format = Logger::getDefaultStandardFormat();
    EXPECT_FALSE(format.empty());
    EXPECT_TRUE(format.find("{timestamp}") != std::string::npos);
    EXPECT_TRUE(format.find("{level}") != std::string::npos);
    EXPECT_TRUE(format.find("{thread_id}") != std::string::npos);
    EXPECT_TRUE(format.find("{file_location}") != std::string::npos);
    EXPECT_TRUE(format.find("{message}") != std::string::npos);
}

// 测试控制台日志输出
TEST_F(LoggerTest, ConsoleLogging) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::CONSOLE;
    logger.initialize(test_config_);
    
    // 重定向cout到stringstream进行测试
    std::stringstream buffer;
    std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
    
    logger.info("Test info message", __FILE__, __LINE__, __FUNCTION__);
    
    // 恢复cout
    std::cout.rdbuf(old_cout);
    
    std::string output = buffer.str();
    EXPECT_TRUE(output.find("Test info message") != std::string::npos);
    EXPECT_TRUE(output.find("INFO") != std::string::npos);
}

// 测试文件日志输出
TEST_F(LoggerTest, FileLogging) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_file.log";
    logger.initialize(test_config_);
    
    logger.info("Test file message", __FILE__, __LINE__, __FUNCTION__);
    logger.warning("Test warning message", __FILE__, __LINE__, __FUNCTION__);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 检查文件是否存在
    EXPECT_TRUE(std::filesystem::exists("test_file.log"));
    
    // 读取文件内容
    std::ifstream file("test_file.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_TRUE(content.find("Test file message") != std::string::npos);
    EXPECT_TRUE(content.find("Test warning message") != std::string::npos);
    EXPECT_TRUE(content.find("INFO") != std::string::npos);
    EXPECT_TRUE(content.find("WARNING") != std::string::npos);
}

// 测试同时输出到控制台和文件
TEST_F(LoggerTest, BothLogging) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::BOTH;
    test_config_.file_path = "test_both.log";
    logger.initialize(test_config_);
    
    // 重定向cout到stringstream
    std::stringstream buffer;
    std::streambuf* old_cout = std::cout.rdbuf(buffer.rdbuf());
    
    logger.info("Test both message", __FILE__, __LINE__, __FUNCTION__);
    
    // 恢复cout
    std::cout.rdbuf(old_cout);
    
    // 检查控制台输出
    std::string console_output = buffer.str();
    EXPECT_TRUE(console_output.find("Test both message") != std::string::npos);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 检查文件输出
    EXPECT_TRUE(std::filesystem::exists("test_both.log"));
    std::ifstream file("test_both.log");
    ASSERT_TRUE(file.is_open());
    
    std::string file_content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_TRUE(file_content.find("Test both message") != std::string::npos);
}

// 测试不同日志级别
TEST_F(LoggerTest, LogLevels) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_levels.log";
    logger.initialize(test_config_);
    
    logger.debug("Debug message", __FILE__, __LINE__, __FUNCTION__);
    logger.info("Info message", __FILE__, __LINE__, __FUNCTION__);
    logger.warning("Warning message", __FILE__, __LINE__, __FUNCTION__);
    logger.error("Error message", __FILE__, __LINE__, __FUNCTION__);
    logger.fatal("Fatal message", __FILE__, __LINE__, __FUNCTION__);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 读取文件内容
    std::ifstream file("test_levels.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_TRUE(content.find("DEBUG") != std::string::npos);
    EXPECT_TRUE(content.find("INFO") != std::string::npos);
    EXPECT_TRUE(content.find("WARNING") != std::string::npos);
    EXPECT_TRUE(content.find("ERROR") != std::string::npos);
    EXPECT_TRUE(content.find("FATAL") != std::string::npos);
}

// 测试日志级别过滤
TEST_F(LoggerTest, LogLevelFiltering) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_filtering.log";
    test_config_.level = LogLevel::WARNING; // 只记录WARNING及以上级别
    logger.initialize(test_config_);
    
    logger.debug("Debug message", __FILE__, __LINE__, __FUNCTION__);
    logger.info("Info message", __FILE__, __LINE__, __FUNCTION__);
    logger.warning("Warning message", __FILE__, __LINE__, __FUNCTION__);
    logger.error("Error message", __FILE__, __LINE__, __FUNCTION__);
    logger.fatal("Fatal message", __FILE__, __LINE__, __FUNCTION__);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 读取文件内容
    std::ifstream file("test_filtering.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // DEBUG和INFO应该被过滤掉
    EXPECT_TRUE(content.find("Debug message") == std::string::npos);
    EXPECT_TRUE(content.find("Info message") == std::string::npos);
    
    // WARNING、ERROR、FATAL应该被记录
    EXPECT_TRUE(content.find("Warning message") != std::string::npos);
    EXPECT_TRUE(content.find("Error message") != std::string::npos);
    EXPECT_TRUE(content.find("Fatal message") != std::string::npos);
}

// 测试模块日志记录器
TEST_F(LoggerTest, ModuleLogger) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_module.log";
    logger.initialize(test_config_);
    
    // 创建模块日志记录器
    auto module_logger = logger.getModuleLogger("test_module");
    ASSERT_NE(module_logger, nullptr);
    
    // 测试模块日志记录
    module_logger->info("Module info message", __FILE__, __LINE__, __FUNCTION__);
    module_logger->error("Module error message", __FILE__, __LINE__, __FUNCTION__);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 读取文件内容
    std::ifstream file("test_module.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // 检查模块名称是否包含在日志中
    EXPECT_TRUE(content.find("[test_module]") != std::string::npos);
    EXPECT_TRUE(content.find("Module info message") != std::string::npos);
    EXPECT_TRUE(content.find("Module error message") != std::string::npos);
}

// 测试重复获取模块日志记录器
TEST_F(LoggerTest, DuplicateModuleLogger) {
    Logger& logger = Logger::getInstance();
    logger.initialize(test_config_);
    
    // 第一次获取
    auto logger1 = logger.getModuleLogger("test_module");
    ASSERT_NE(logger1, nullptr);
    
    // 第二次获取相同模块
    auto logger2 = logger.getModuleLogger("test_module");
    ASSERT_NE(logger2, nullptr);
    
    // 应该是同一个实例
    EXPECT_EQ(logger1, logger2);
}

// 测试全局便捷函数
TEST_F(LoggerTest, GlobalConvenienceFunctions) {
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_global.log";
    
    // 测试全局初始化函数
    initializeLogger(test_config_);
    
    // 测试全局模块日志记录器函数
    auto module_logger = getModuleLogger("global_test");
    ASSERT_NE(module_logger, nullptr);
    
    module_logger->info("Global test message", __FILE__, __LINE__, __FUNCTION__);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 检查文件内容
    EXPECT_TRUE(std::filesystem::exists("test_global.log"));
    std::ifstream file("test_global.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_TRUE(content.find("Global test message") != std::string::npos);
}

// 测试日志文件轮转
TEST_F(LoggerTest, LogFileRotation) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_rotation.log";
    test_config_.max_file_size = 500; // 500字节触发轮转
    test_config_.max_files = 2;
    logger.initialize(test_config_);
    
    // 写入大量数据触发文件轮转
    for (int i = 0; i < 50; ++i) {
        logger.info("This is a long message that will help us exceed the file size limit: " + std::to_string(i), 
                   __FILE__, __LINE__, __FUNCTION__);
    }
    
    // 等待文件写入和轮转
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 检查主文件是否存在
    EXPECT_TRUE(std::filesystem::exists("test_rotation.log"));
    
    // 检查文件大小是否超过限制
    if (std::filesystem::exists("test_rotation.log")) {
        auto file_size = std::filesystem::file_size("test_rotation.log");
        std::cout << "Current log file size: " << file_size << " bytes" << std::endl;
        
        // 如果文件大小超过限制，应该触发轮转
        if (file_size >= test_config_.max_file_size) {
            // 检查备份文件是否存在
            EXPECT_TRUE(std::filesystem::exists("test_rotation.log.1"));
        } else {
            // 如果文件大小没有超过限制，跳过轮转检查
            std::cout << "File size not exceeded, skipping rotation check" << std::endl;
        }
    }
}

// 测试多线程日志记录
TEST_F(LoggerTest, MultiThreadLogging) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_multithread.log";
    logger.initialize(test_config_);
    
    const int num_threads = 5;
    const int messages_per_thread = 10;
    std::vector<std::thread> threads;
    
    // 创建多个线程同时写入日志
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&logger, i]() {
            auto module_logger = logger.getModuleLogger("thread_" + std::to_string(i));
            for (int j = 0; j < messages_per_thread; ++j) {
                module_logger->info("Thread " + std::to_string(i) + " message " + std::to_string(j), 
                                   __FILE__, __LINE__, __FUNCTION__);
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 检查文件是否存在
    EXPECT_TRUE(std::filesystem::exists("test_multithread.log"));
    
    // 读取文件内容
    std::ifstream file("test_multithread.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // 检查是否包含所有线程的消息
    for (int i = 0; i < num_threads; ++i) {
        EXPECT_TRUE(content.find("thread_" + std::to_string(i)) != std::string::npos);
    }
}

// 测试Logger关闭
TEST_F(LoggerTest, Shutdown) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_shutdown.log";
    logger.initialize(test_config_);
    
    // 创建模块日志记录器
    auto module_logger = logger.getModuleLogger("shutdown_test");
    ASSERT_NE(module_logger, nullptr);
    
    // 关闭Logger
    logger.shutdown();
    
    // 尝试使用已关闭的Logger（应该不会崩溃）
    module_logger->info("Message after shutdown", __FILE__, __LINE__, __FUNCTION__);
    
    // 重新初始化应该正常工作
    logger.initialize(test_config_);
    auto new_module_logger = logger.getModuleLogger("new_test");
    ASSERT_NE(new_module_logger, nullptr);
}

// 测试空消息处理
TEST_F(LoggerTest, EmptyMessage) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_empty.log";
    logger.initialize(test_config_);
    
    // 测试空消息
    logger.info("", __FILE__, __LINE__, __FUNCTION__);
    logger.info("   ", __FILE__, __LINE__, __FUNCTION__); // 只有空格
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 检查文件是否存在
    EXPECT_TRUE(std::filesystem::exists("test_empty.log"));
}

// 测试特殊字符处理
TEST_F(LoggerTest, SpecialCharacters) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_special.log";
    logger.initialize(test_config_);
    
    // 测试包含特殊字符的消息
    std::string special_message = "Special chars: !@#$%^&*()_+-=[]{}|;':\",./<>?`~";
    logger.info(special_message, __FILE__, __LINE__, __FUNCTION__);
    
    // 测试包含换行符的消息
    std::string multiline_message = "Line 1\nLine 2\nLine 3";
    logger.info(multiline_message, __FILE__, __LINE__, __FUNCTION__);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 检查文件内容
    std::ifstream file("test_special.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    EXPECT_TRUE(content.find(special_message) != std::string::npos);
    EXPECT_TRUE(content.find("Line 1") != std::string::npos);
}

// 测试性能（大量日志记录）
TEST_F(LoggerTest, PerformanceTest) {
    Logger& logger = Logger::getInstance();
    test_config_.target = LogTarget::FILE;
    test_config_.file_path = "test_performance.log";
    logger.initialize(test_config_);
    
    const int num_messages = 1000;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // 记录大量日志
    for (int i = 0; i < num_messages; ++i) {
        logger.info("Performance test message " + std::to_string(i), __FILE__, __LINE__, __FUNCTION__);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 等待文件写入
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 检查性能（应该在合理时间内完成）
    EXPECT_LT(duration.count(), 5000); // 5秒内完成
    
    // 检查文件是否存在
    EXPECT_TRUE(std::filesystem::exists("test_performance.log"));
    
    // 检查消息数量
    std::ifstream file("test_performance.log");
    ASSERT_TRUE(file.is_open());
    
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();
    
    // 计算消息数量
    size_t message_count = 0;
    size_t pos = 0;
    std::string search_str = "Performance test message";
    while ((pos = content.find(search_str, pos)) != std::string::npos) {
        message_count++;
        pos += search_str.length(); // 跳过当前匹配的字符串
    }
    
    // 由于日志格式可能包含时间戳等信息，实际消息数量可能更多
    // 我们只检查是否至少包含了期望数量的消息
    EXPECT_GE(message_count, num_messages);
}

// 主函数
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
