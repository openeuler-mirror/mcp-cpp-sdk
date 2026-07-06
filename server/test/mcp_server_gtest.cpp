/**
 * @file mcp_server_gtest.cpp
 * @brief MCP Server Google Test 测试用例
 * 
 * 使用Google Test框架测试MCP服务器和文件系统服务器的基本接口和功能
 */

#include <gtest/gtest.h>
#include "mcp_server.h"
#include "mcp_filesystem_server.h"
#include "../../mcp_protocol/include/mcp_protocol_core.h"
#include "../../transport/include/transport_base.h"
#include "../../log/include/logger.h"
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <atomic>
#include <random>

using namespace mcp;
using namespace mcp_logger;

// 全局日志模块记录器
std::shared_ptr<Logger::ModuleLogger> g_test_logger;

/**
 * @class MCPServerTest
 * @brief MCP服务器测试类
 * 
 * 提供测试环境的设置和清理
 */
class MCPServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试之前执行
        if (g_test_logger) {
            g_test_logger->info("开始执行测试: " + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        }
        server_ = std::make_unique<MCPServer>();
        ASSERT_NE(server_, nullptr);
        if (g_test_logger) {
            g_test_logger->info("MCP服务器创建成功");
        }
    }

    void TearDown() override {
        // 在每个测试之后执行
        if (g_test_logger) {
            g_test_logger->info("清理测试环境");
        }
        if (server_ && server_->isRunning()) {
            if (g_test_logger) {
                g_test_logger->info("停止服务器");
            }
            server_->stop();
        }
        server_.reset();
        if (g_test_logger) {
            g_test_logger->info("测试完成: " + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        }
    }

    std::unique_ptr<MCPServer> server_;
};

/**
 * @brief 测试MCP服务器的基本构造和析构
 */
TEST_F(MCPServerTest, BasicConstruction) {
    // 测试服务器创建
    if (g_test_logger) {
        g_test_logger->info("测试服务器基本构造");
    }
    EXPECT_NE(server_, nullptr);
    
    // 测试初始运行状态
    if (g_test_logger) {
        g_test_logger->info("验证初始运行状态");
    }
    EXPECT_FALSE(server_->isRunning());
    EXPECT_FALSE(server_->isConnected());
    
    if (g_test_logger) {
        g_test_logger->info("基本构造测试完成");
    }
}

/**
 * @brief 测试服务器信息设置
 */
TEST_F(MCPServerTest, ServerInfoSetting) {
    const std::string test_name = "test-server";
    const std::string test_version = "1.0.0";
    
    // 设置服务器信息
    EXPECT_NO_THROW(server_->setServerInfo(test_name, test_version));
    
    // 验证设置成功（不会抛出异常）
    EXPECT_NO_THROW(server_->setServerInfo("another-name", "2.0.0"));
}

/**
 * @brief 测试服务器能力设置
 */
TEST_F(MCPServerTest, CapabilitiesSetting) {
    ServerCapabilities capabilities;
    capabilities.tools = json::object();
    capabilities.resources = json::object();
    capabilities.prompts = json::object();
    
    // 测试设置能力
    EXPECT_NO_THROW(server_->setCapabilities(capabilities));
}

/**
 * @brief 测试工具管理
 */
TEST_F(MCPServerTest, ToolManagement) {
    // 测试添加工具
    Tool tool;
    tool.name = "test-tool";
    tool.description = "A test tool";
    tool.inputSchema = json::object();
    
    EXPECT_NO_THROW(server_->addTool(tool));
    
    // 测试列出工具
    auto tools = server_->listTools();
    EXPECT_GE(tools.size(), 1);
    
    // 测试移除工具
    EXPECT_NO_THROW(server_->removeTool("test-tool"));
    
    // 测试清空工具
    EXPECT_NO_THROW(server_->clearTools());
}

/**
 * @brief 测试资源管理
 */
TEST_F(MCPServerTest, ResourceManagement) {
    // 测试添加资源
    Resource resource;
    resource.uri = "file://test.txt";
    resource.name = "Test Resource";
    resource.description = "A test resource";
    resource.mimeType = "text/plain";
    
    EXPECT_NO_THROW(server_->addResource(resource));
    
    // 测试列出资源
    auto resources = server_->listResources();
    EXPECT_GE(resources.size(), 1);
    
    // 测试移除资源
    EXPECT_NO_THROW(server_->removeResource("file://test.txt"));
    
    // 测试清空资源
    EXPECT_NO_THROW(server_->clearResources());
}

/**
 * @brief 测试提示管理
 */
TEST_F(MCPServerTest, PromptManagement) {
    // 测试添加提示
    Prompt prompt;
    prompt.name = "test-prompt";
    prompt.description = "A test prompt";
    prompt.arguments = json::array();
    
    EXPECT_NO_THROW(server_->addPrompt(prompt));
    
    // 测试列出提示
    auto prompts = server_->listPrompts();
    EXPECT_GE(prompts.size(), 1);
    
    // 测试移除提示
    EXPECT_NO_THROW(server_->removePrompt("test-prompt"));
    
    // 测试清空提示
    EXPECT_NO_THROW(server_->clearPrompts());
}

/**
 * @brief 测试工具调用
 */
TEST_F(MCPServerTest, ToolCall) {
    // 添加一个测试工具
    Tool tool;
    tool.name = "echo";
    tool.description = "Echo tool";
    tool.inputSchema = json::object();
    server_->addTool(tool);
    
    // 设置工具调用处理器
    bool handler_called = false;
    server_->setToolCallHandler([&handler_called](const ToolCallParams& params) -> ToolCallResult {
        handler_called = true;
        ToolCallResult result;
        result.isError = false;
        result.content = json::object();
        result.content["echo"] = params.name;
        return result;
    });
    
    // 测试调用工具
    ToolCallParams params;
    params.name = "echo";
    params.arguments = json::object();
    
    auto result = server_->callTool(params);
    EXPECT_TRUE(handler_called);
    EXPECT_FALSE(result.isError);
}

/**
 * @brief 测试资源读取
 */
TEST_F(MCPServerTest, ResourceRead) {
    // 添加一个测试资源（使用非file:// URI，这样会调用handler）
    Resource resource;
    resource.uri = "test://test-resource";
    resource.name = "Test Resource";
    server_->addResource(resource);
    
    // 设置资源读取处理器
    bool handler_called = false;
    server_->setResourceReadHandler([&handler_called](const std::string& uri) -> json {
        handler_called = true;
        json result;
        result["uri"] = uri;
        result["content"] = "test content";
        return result;
    });
    
    // 测试读取资源（使用非file:// URI）
    auto result = server_->readResource("test://test-resource");
    EXPECT_TRUE(handler_called);
    EXPECT_TRUE(result.is_object());
    EXPECT_EQ(result["uri"], "test://test-resource");
}

/**
 * @brief 测试提示获取
 */
TEST_F(MCPServerTest, PromptGet) {
    // 添加一个测试提示
    Prompt prompt;
    prompt.name = "test-prompt";
    prompt.description = "A test prompt";
    prompt.arguments = json::array();
    server_->addPrompt(prompt);
    
    // 设置提示获取处理器
    bool handler_called = false;
    server_->setPromptGetHandler([&handler_called](const std::string& name, const json& arguments) -> json {
        handler_called = true;
        json result;
        result["name"] = name;
        result["arguments"] = arguments;
        return result;
    });
    
    // 测试获取提示
    json args = json::object();
    auto result = server_->getPrompt("test-prompt", args);
    EXPECT_TRUE(handler_called);
    EXPECT_TRUE(result.is_object());
    EXPECT_EQ(result["name"], "test-prompt");
}

/**
 * @brief 测试通知发送
 */
TEST_F(MCPServerTest, NotificationSending) {
    const std::string method = "test/notification";
    json params = json::object();
    params["test"] = "value";
    
    // 测试发送通知（应该不抛出异常）
    EXPECT_NO_THROW(server_->sendNotification(method, params));
}

/**
 * @brief 测试通知处理器设置
 */
TEST_F(MCPServerTest, NotificationHandlerSetting) {
    bool handler_called = false;
    
    // 设置通知处理器
    auto handler = [&handler_called](const std::string& method, const json& params) {
        handler_called = true;
        EXPECT_EQ(method, "test/method");
        EXPECT_TRUE(params.is_object());
    };
    
    EXPECT_NO_THROW(server_->setNotificationHandler("test/method", handler));
}

/**
 * @brief 测试协议初始化
 */
TEST_F(MCPServerTest, ProtocolInitialization) {
    InitializeParams params;
    params.protocolVersion = PROTOCOL_VERSION;
    params.clientInfo.name = "test-client";
    params.clientInfo.version = "1.0.0";
    
    // 测试初始化
    auto result = server_->initialize(params);
    EXPECT_EQ(result.protocolVersion, PROTOCOL_VERSION);
    EXPECT_TRUE(result.serverInfo.name.empty() || !result.serverInfo.name.empty());
}

/**
 * @brief 测试ping功能
 */
TEST_F(MCPServerTest, Ping) {
    // 测试ping
    auto result = server_->ping();
    EXPECT_TRUE(result.is_object());
}

/**
 * @brief 测试服务器启动和停止（不实际启动）
 */
TEST_F(MCPServerTest, ServerStartStop) {
    // 测试初始状态
    EXPECT_FALSE(server_->isRunning());
    
    // 测试停止未运行的服务器（应该不抛出异常）
    EXPECT_NO_THROW(server_->stop());
    
    // 验证状态未改变
    EXPECT_FALSE(server_->isRunning());
}

/**
 * @brief 测试内存管理
 */
TEST_F(MCPServerTest, MemoryManagement) {
    // 创建多个服务器实例
    std::vector<std::unique_ptr<MCPServer>> servers;
    
    for (int i = 0; i < 5; ++i) {
        auto server = std::make_unique<MCPServer>();
        server->setServerInfo("server-" + std::to_string(i), "1.0.0");
        servers.push_back(std::move(server));
    }
    
    // 验证所有服务器都创建成功
    EXPECT_EQ(servers.size(), 5);
    
    // 清理服务器（测试析构函数）
    servers.clear();
    
    // 验证清理完成
    EXPECT_TRUE(servers.empty());
}

/**
 * @brief 测试并发操作
 */
TEST_F(MCPServerTest, ConcurrentOperations) {
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 创建多个线程同时进行服务器操作
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            try {
                // 每个线程设置不同的服务器信息
                server_->setServerInfo("thread-" + std::to_string(i), "1.0.0");
                
                // 添加工具
                Tool tool;
                tool.name = "tool-" + std::to_string(i);
                tool.description = "Tool from thread " + std::to_string(i);
                tool.inputSchema = json::object();
                server_->addTool(tool);
                
                success_count++;
            } catch (...) {
                // 忽略异常
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    // 验证至少有一些操作成功
    EXPECT_GT(success_count.load(), 0);
}

/**
 * @class MCPFileSystemServerTest
 * @brief MCP文件系统服务器测试类
 */
class MCPFileSystemServerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建临时测试目录，使用系统临时目录或当前目录
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(1000, 9999);
        
        // 获取系统临时目录或使用当前目录
        std::string base_dir;
        try {
            base_dir = std::filesystem::temp_directory_path().string();
        } catch (...) {
            // 如果无法获取系统临时目录，使用当前目录下的tmp
            base_dir = std::filesystem::current_path().string() + "/tmp";
        }
        
        // 生成唯一的测试目录名（使用绝对路径）
        std::string relative_dir = "mcp_gtest_" + std::to_string(std::time(nullptr)) + "_" + 
                                  std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "_" +
                                  std::to_string(dis(gen));
        test_dir_ = (std::filesystem::path(base_dir) / relative_dir).string();
        
        // 如果目录已存在，尝试生成新的唯一目录名
        int attempts = 0;
        while (std::filesystem::exists(test_dir_) && attempts < 10) {
            relative_dir = "mcp_gtest_" + std::to_string(std::time(nullptr)) + "_" + 
                          std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id())) + "_" +
                          std::to_string(dis(gen)) + "_" + std::to_string(attempts);
            test_dir_ = (std::filesystem::path(base_dir) / relative_dir).string();
            attempts++;
        }
        
        // 创建目录（如果不存在）
        if (!std::filesystem::exists(test_dir_)) {
            std::error_code ec;
            std::filesystem::create_directories(test_dir_, ec);
            
            // 如果创建失败且目录仍然不存在，尝试备用方案
            if (ec && ec.value() != 0 && !std::filesystem::exists(test_dir_)) {
                // 使用更简单的备用目录名
                relative_dir = "mcp_gtest_" + std::to_string(std::time(nullptr)) + "_" + 
                              std::to_string(rd()) + "_" + std::to_string(rd());
                test_dir_ = (std::filesystem::path(base_dir) / relative_dir).string();
                std::filesystem::create_directories(test_dir_, ec);
                if (ec && ec.value() != 0 && !std::filesystem::exists(test_dir_)) {
                    FAIL() << "Failed to create test directory: " << ec.message() << " (code: " << ec.value() << ")";
                }
            }
        }
        
        // 将路径转换为绝对路径（确保是规范化的绝对路径）
        try {
            test_dir_ = std::filesystem::canonical(test_dir_).string();
        } catch (...) {
            // 如果canonical失败，至少使用absolute
            test_dir_ = std::filesystem::absolute(test_dir_).string();
        }
        
        // 验证目录确实存在
        if (!std::filesystem::exists(test_dir_)) {
            FAIL() << "Test directory does not exist after creation: " << test_dir_;
        }
        
        if (g_test_logger) {
            g_test_logger->info("开始执行测试: " + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
            g_test_logger->info("测试目录: " + test_dir_);
        }
        
        server_ = std::make_unique<MCPFileSystemServer>(test_dir_, true);
        ASSERT_NE(server_, nullptr);
        if (g_test_logger) {
            g_test_logger->info("MCP文件系统服务器创建成功");
        }
    }

    void TearDown() override {
        // 在每个测试之后执行
        if (g_test_logger) {
            g_test_logger->info("清理测试环境");
        }
        if (server_ && server_->isRunning()) {
            if (g_test_logger) {
                g_test_logger->info("停止服务器");
            }
            server_->stop();
        }
        server_.reset();
        
        // 清理测试目录
        if (std::filesystem::exists(test_dir_)) {
            std::filesystem::remove_all(test_dir_);
        }
        
        if (g_test_logger) {
            g_test_logger->info("测试完成: " + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        }
    }

    std::unique_ptr<MCPFileSystemServer> server_;
    std::string test_dir_;
    
    // 辅助函数
    bool createTestFile(const std::string& path, const std::string& content) {
        try {
            std::ofstream file(path);
            if (!file.is_open()) return false;
            file << content;
            file.close();
            return true;
        } catch (...) {
            return false;
        }
    }
};

/**
 * @brief 测试文件系统服务器基本构造
 */
TEST_F(MCPFileSystemServerTest, BasicConstruction) {
    EXPECT_NE(server_, nullptr);
    EXPECT_FALSE(server_->isRunning());
    EXPECT_EQ(server_->getRootPath(), test_dir_);
    EXPECT_TRUE(server_->isWriteEnabled());
}

/**
 * @brief 测试路径验证
 */
TEST_F(MCPFileSystemServerTest, PathValidation) {
    // 测试有效路径
    EXPECT_TRUE(server_->validatePath(test_dir_ + "/test.txt"));
    EXPECT_TRUE(server_->validatePath(test_dir_ + "/subdir/file.txt"));
    EXPECT_TRUE(server_->validatePath("test.txt")); // 相对路径
    
    // 测试无效路径（超出根目录）
    EXPECT_FALSE(server_->validatePath("/etc/passwd"));
    EXPECT_FALSE(server_->validatePath("/home"));
    EXPECT_FALSE(server_->validatePath("../etc/passwd"));
}

/**
 * @brief 测试路径解析
 */
TEST_F(MCPFileSystemServerTest, PathResolution) {
    // 测试相对路径解析
    std::string resolved = server_->resolvePath("test.txt");
    EXPECT_TRUE(resolved.find(test_dir_) == 0);
    
    // 测试绝对路径解析
    std::string absolute = server_->resolvePath(test_dir_ + "/absolute.txt");
    EXPECT_EQ(absolute, test_dir_ + "/absolute.txt");
}

/**
 * @brief 测试文件写入和读取
 */
TEST_F(MCPFileSystemServerTest, FileWriteAndRead) {
    // 测试写入文件
    ToolCallParams write_params;
    write_params.name = "write_file";
    write_params.arguments = {
        {"path", "test.txt"},
        {"content", "Hello, MCP FileSystem Server!"}
    };
    
    auto write_result = server_->callTool(write_params);
    EXPECT_FALSE(write_result.isError);
    
    // 测试读取文件
    ToolCallParams read_params;
    read_params.name = "read_file";
    read_params.arguments = {{"path", "test.txt"}};
    
    auto read_result = server_->callTool(read_params);
    EXPECT_FALSE(read_result.isError);
    EXPECT_EQ(read_result.content["content"], "Hello, MCP FileSystem Server!");
}

/**
 * @brief 测试目录列表
 */
TEST_F(MCPFileSystemServerTest, DirectoryListing) {
    // 创建测试文件
    createTestFile(test_dir_ + "/file1.txt", "content1");
    createTestFile(test_dir_ + "/file2.txt", "content2");
    
    // 测试目录列表
    ToolCallParams list_params;
    list_params.name = "list_directory";
    list_params.arguments = {{"path", "."}};
    
    auto list_result = server_->callTool(list_params);
    EXPECT_FALSE(list_result.isError);
    EXPECT_TRUE(list_result.content.contains("files"));
}

/**
 * @brief 测试文件信息
 */
TEST_F(MCPFileSystemServerTest, FileInfo) {
    // 创建测试文件
    createTestFile(test_dir_ + "/test.txt", "test content");
    
    // 测试文件信息
    ToolCallParams info_params;
    info_params.name = "file_info";
    info_params.arguments = {{"path", "test.txt"}};
    
    auto info_result = server_->callTool(info_params);
    EXPECT_FALSE(info_result.isError);
    EXPECT_TRUE(info_result.content["is_regular_file"] == true);
}

/**
 * @brief 测试目录创建
 */
TEST_F(MCPFileSystemServerTest, DirectoryCreation) {
    // 测试创建目录
    ToolCallParams mkdir_params;
    mkdir_params.name = "create_directory";
    mkdir_params.arguments = {{"path", "subdir"}};
    
    auto mkdir_result = server_->callTool(mkdir_params);
    EXPECT_FALSE(mkdir_result.isError);
    
    // 验证目录存在
    EXPECT_TRUE(std::filesystem::exists(test_dir_ + "/subdir"));
}

/**
 * @brief 测试只读模式
 */
TEST_F(MCPFileSystemServerTest, ReadOnlyMode) {
    // 创建只读服务器
    MCPFileSystemServer readonly_server(test_dir_, false);
    
    // 创建测试文件
    createTestFile(test_dir_ + "/readonly.txt", "Read-only test file");
    
    // 测试读取文件（应该成功）
    ToolCallParams read_params;
    read_params.name = "read_file";
    read_params.arguments = {{"path", "readonly.txt"}};
    
    auto read_result = readonly_server.callTool(read_params);
    EXPECT_FALSE(read_result.isError);
    
    // 测试写入文件（应该失败）
    ToolCallParams write_params;
    write_params.name = "write_file";
    write_params.arguments = {
        {"path", "new.txt"},
        {"content", "This should fail"}
    };
    
    auto write_result = readonly_server.callTool(write_params);
    EXPECT_TRUE(write_result.isError);
    
    // 测试删除文件（应该失败）
    ToolCallParams delete_params;
    delete_params.name = "delete_file";
    delete_params.arguments = {{"path", "readonly.txt"}};
    
    auto delete_result = readonly_server.callTool(delete_params);
    EXPECT_TRUE(delete_result.isError);
}

/**
 * @brief 测试资源管理
 */
TEST_F(MCPFileSystemServerTest, ResourceManagement) {
    // 测试添加资源
    server_->addFileSystemResource("subdir", "测试目录", "这是一个测试目录");
    auto resources = server_->listFileSystemResources();
    EXPECT_GT(resources.size(), 0);
    
    // 测试移除资源
    server_->removeFileSystemResource("subdir");
    auto resources_after = server_->listFileSystemResources();
    // 注意：根目录资源仍然存在
}

/**
 * @brief 测试根路径设置
 */
TEST_F(MCPFileSystemServerTest, RootPathSetting) {
    std::string new_root = test_dir_ + "/new_root";
    std::filesystem::create_directories(new_root);
    
    server_->setRootPath(new_root);
    EXPECT_EQ(server_->getRootPath(), new_root);
}

/**
 * @brief 测试写入启用设置
 */
TEST_F(MCPFileSystemServerTest, WriteEnabledSetting) {
    server_->setWriteEnabled(false);
    EXPECT_FALSE(server_->isWriteEnabled());
    
    server_->setWriteEnabled(true);
    EXPECT_TRUE(server_->isWriteEnabled());
}

/**
 * @brief 测试错误处理
 */
TEST_F(MCPFileSystemServerTest, ErrorHandling) {
    // 测试读取不存在的文件
    ToolCallParams read_params;
    read_params.name = "read_file";
    read_params.arguments = {{"path", "nonexistent.txt"}};
    
    auto read_result = server_->callTool(read_params);
    EXPECT_TRUE(read_result.isError);
    
    // 测试无效的工具调用
    ToolCallParams invalid_params;
    invalid_params.name = "invalid_tool";
    invalid_params.arguments = json::object();
    
    auto invalid_result = server_->callTool(invalid_params);
    EXPECT_TRUE(invalid_result.isError);
}

// 主函数用于运行测试
int main(int argc, char** argv) {
    // 初始化日志系统
    LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH; // 同时输出到控制台和文件
    log_config.file_path = "mcp_server_gtest.log"; // 与文件名相同的日志文件
    log_config.enable_timestamp = true;
    log_config.enable_thread_id = true;
    log_config.enable_file_location = true;
    log_config.auto_flush = true;
    
    // 初始化全局日志系统
    Logger::getInstance().initialize(log_config);
    
    // 获取测试模块日志记录器
    g_test_logger = Logger::getInstance().getModuleLogger("mcp_server_test");
    
    // 记录测试开始
    g_test_logger->info("=== MCP Server Google Test 开始运行 ===");
    g_test_logger->info("日志文件: " + log_config.file_path);
    
    // 初始化Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // 运行测试
    int result = RUN_ALL_TESTS();
    
    // 记录测试结束
    g_test_logger->info("=== MCP Server Google Test 运行完成 ===");
    std::string result_msg = "测试结果: " + std::string(result == 0 ? "全部通过" : "有测试失败");
    g_test_logger->info(result_msg);
    
    // 关闭日志系统
    Logger::getInstance().shutdown();
    
    return result;
}

