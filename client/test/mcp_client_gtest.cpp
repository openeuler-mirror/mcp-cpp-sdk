/**
 * @file mcp_client_gtest.cpp
 * @brief MCP Client Google Test 测试用例
 * 
 * 使用Google Test框架测试MCP客户端的基本接口和功能
 */

#include <gtest/gtest.h>
#include "mcp_client.h"
#include "../../mcp_protocol/include/mcp_protocol_core.h"
#include "../../transport/include/transport_base.h"
#include "../../log/include/logger.h"
#include <thread>
#include <chrono>
#include <future>

using namespace mcp;
using namespace mcp_logger;

// 全局日志模块记录器
std::shared_ptr<Logger::ModuleLogger> g_test_logger;

/**
 * @class MCPClientTest
 * @brief MCP客户端测试类
 * 
 * 提供测试环境的设置和清理
 */
class MCPClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 在每个测试之前执行
        g_test_logger->info("开始执行测试: " + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
        client_ = std::make_unique<MCPClient>();
        ASSERT_NE(client_, nullptr);
        g_test_logger->info("MCP客户端创建成功");
    }

    void TearDown() override {
        // 在每个测试之后执行
        g_test_logger->info("清理测试环境");
        if (client_ && client_->isConnected()) {
            g_test_logger->info("断开客户端连接");
            client_->disconnect();
        }
        client_.reset();
        g_test_logger->info("测试完成: " + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()));
    }

    std::unique_ptr<MCPClient> client_;
};

/**
 * @brief 测试MCP客户端的基本构造和析构
 */
TEST_F(MCPClientTest, BasicConstruction) {
    // 测试客户端创建
    g_test_logger->info("测试客户端基本构造");
    EXPECT_NE(client_, nullptr);
    
    // 测试初始连接状态
    g_test_logger->info("验证初始连接状态");
    EXPECT_FALSE(client_->isConnected());
    
    // 测试传输类型默认值
    g_test_logger->info("验证传输类型默认值");
    EXPECT_EQ(client_->getTransportType(), mcp_transport::TransportType::AUTO);
    g_test_logger->info("基本构造测试完成");
}

/**
 * @brief 测试客户端信息设置
 */
TEST_F(MCPClientTest, ClientInfoSetting) {
    const std::string test_name = "test-client";
    const std::string test_version = "1.0.0";
    
    // 设置客户端信息
    client_->setClientInfo(test_name, test_version);
    
    // 验证设置成功（注意：这里我们无法直接获取客户端信息，因为接口没有提供getter）
    // 但我们可以验证调用不会抛出异常
    EXPECT_NO_THROW(client_->setClientInfo("another-name", "2.0.0"));
}

/**
 * @brief 测试传输类型设置
 */
TEST_F(MCPClientTest, TransportTypeSetting) {
    // 测试设置IPC传输类型
    client_->setTransportType(mcp_transport::TransportType::IPC);
    EXPECT_EQ(client_->getTransportType(), mcp_transport::TransportType::IPC);
    
    // 测试设置HTTP传输类型
    client_->setTransportType(mcp_transport::TransportType::HTTP);
    EXPECT_EQ(client_->getTransportType(), mcp_transport::TransportType::HTTP);
    
    // 测试设置AUTO传输类型
    client_->setTransportType(mcp_transport::TransportType::AUTO);
    EXPECT_EQ(client_->getTransportType(), mcp_transport::TransportType::AUTO);
}

/**
 * @brief 测试端点管理
 */
TEST_F(MCPClientTest, EndpointManagement) {
    std::vector<std::string> endpoints = {
        "http://localhost:8080",
        "https://api.example.com",
        "cmd:python script.py"
    };
    
    // 测试设置端点列表
    client_->setEndpoints(endpoints);
    
    // 验证端点设置
    auto retrieved_endpoints = client_->getEndpoints();
    EXPECT_EQ(retrieved_endpoints.size(), endpoints.size());
    EXPECT_EQ(retrieved_endpoints[0], endpoints[0]);
    
    // 测试设置当前端点
    const std::string new_endpoint = "http://new-server:9090";
    client_->setCurrentEndpoint(new_endpoint);
    EXPECT_EQ(client_->getCurrentEndpoint(), new_endpoint);
}

/**
 * @brief 测试客户端能力设置
 */
TEST_F(MCPClientTest, CapabilitiesSetting) {
    ClientCapabilities capabilities;
    capabilities.tools = json::object();
    capabilities.resources = json::object();
    capabilities.prompts = json::object();
    
    // 测试设置能力
    EXPECT_NO_THROW(client_->setCapabilities(capabilities));
}

/**
 * @brief 测试连接管理（无实际服务器连接）
 */
TEST_F(MCPClientTest, ConnectionManagement) {
    g_test_logger->info("开始连接管理测试");
    
    // 测试未连接状态
    g_test_logger->info("验证未连接状态");
    EXPECT_FALSE(client_->isConnected());
    
    // 测试断开连接（应该不抛出异常）
    g_test_logger->info("测试断开连接操作");
    EXPECT_NO_THROW(client_->disconnect());
    
    // 测试连接无效端点（应该失败）
    g_test_logger->warning("测试连接无效端点");
    EXPECT_FALSE(client_->connect("invalid://endpoint"));
    EXPECT_FALSE(client_->isConnected());
    
    g_test_logger->info("连接管理测试完成");
}

/**
 * @brief 测试传输配置连接
 */
TEST_F(MCPClientTest, TransportConfigConnection) {
    mcp_transport::TransportConfig config;
    config.type = mcp_transport::TransportType::IPC;
    config.ipc.command = "echo";
    config.ipc.args = {"hello"};
    
    // 测试使用配置连接（可能会失败，因为echo不是MCP服务器）
    bool connected = client_->connectWithConfig(config);
    
    // 验证连接状态
    EXPECT_EQ(client_->isConnected(), connected);
    EXPECT_EQ(client_->getTransportType(), config.type);
}

/**
 * @brief 测试协议初始化（无服务器）
 */
TEST_F(MCPClientTest, ProtocolInitialization) {
    InitializeParams params;
    params.protocolVersion = PROTOCOL_VERSION;
    params.clientInfo.name = "test-client";
    params.clientInfo.version = "1.0.0";
    
    // 测试初始化（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->initialize(params);
    }, mcp_exception);
}

/**
 * @brief 测试ping功能（无服务器）
 */
TEST_F(MCPClientTest, PingWithoutServer) {
    // 测试ping（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->ping();
    }, mcp_exception);
}

/**
 * @brief 测试工具列表功能（无服务器）
 */
TEST_F(MCPClientTest, ListToolsWithoutServer) {
    // 测试列出工具（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->listTools();
    }, mcp_exception);
}

/**
 * @brief 测试工具调用功能（无服务器）
 */
TEST_F(MCPClientTest, CallToolWithoutServer) {
    ToolCallParams params;
    params.name = "test-tool";
    params.arguments = json::object();
    
    // 测试调用工具（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->callTool(params);
    }, mcp_exception);
}

/**
 * @brief 测试资源列表功能（无服务器）
 */
TEST_F(MCPClientTest, ListResourcesWithoutServer) {
    // 测试列出资源（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->listResources();
    }, mcp_exception);
}

/**
 * @brief 测试读取资源功能（无服务器）
 */
TEST_F(MCPClientTest, ReadResourceWithoutServer) {
    const std::string test_uri = "file://test.txt";
    
    // 测试读取资源（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->readResource(test_uri);
    }, mcp_exception);
}

/**
 * @brief 测试提示列表功能（无服务器）
 */
TEST_F(MCPClientTest, ListPromptsWithoutServer) {
    // 测试列出提示（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->listPrompts();
    }, mcp_exception);
}

/**
 * @brief 测试获取提示功能（无服务器）
 */
TEST_F(MCPClientTest, GetPromptWithoutServer) {
    const std::string test_name = "test-prompt";
    json test_args = json::object();
    
    // 测试获取提示（应该失败，因为没有连接服务器）
    EXPECT_THROW({
        client_->getPrompt(test_name, test_args);
    }, mcp_exception);
}

/**
 * @brief 测试通知发送（无服务器）
 */
TEST_F(MCPClientTest, SendNotificationWithoutServer) {
    const std::string method = "test/notification";
    json params = json::object();
    
    // 测试发送通知（应该不抛出异常，但不会发送成功）
    EXPECT_NO_THROW(client_->sendNotification(method, params));
}

/**
 * @brief 测试通知处理器设置
 */
TEST_F(MCPClientTest, NotificationHandlerSetting) {
    bool handler_called = false;
    
    // 设置通知处理器
    auto handler = [&handler_called](const std::string& method, const json& params) {
        handler_called = true;
        EXPECT_EQ(method, "test/method");
        EXPECT_TRUE(params.is_object());
    };
    
    EXPECT_NO_THROW(client_->setNotificationHandler("test/method", handler));
    
    // 注意：由于没有实际的通知发送，我们无法测试处理器是否被调用
    EXPECT_FALSE(handler_called);
}

/**
 * @brief 测试工具调用处理器设置
 */
TEST_F(MCPClientTest, ToolCallHandlerSetting) {
    bool handler_called = false;
    
    // 设置工具调用处理器
    auto handler = [&handler_called](const ToolCallParams& params) -> ToolCallResult {
        handler_called = true;
        ToolCallResult result;
        result.content = json::object();
        result.content["received"] = params.name;
        return result;
    };
    
    EXPECT_NO_THROW(client_->setToolCallHandler(handler));
    
    // 注意：由于没有实际的工具调用，我们无法测试处理器是否被调用
    EXPECT_FALSE(handler_called);
}

/**
 * @brief 测试资源读取处理器设置
 */
TEST_F(MCPClientTest, ResourceReadHandlerSetting) {
    bool handler_called = false;
    
    // 设置资源读取处理器
    auto handler = [&handler_called](const std::string& /*uri*/) -> json {
        handler_called = true;
        return json::object();
    };
    
    EXPECT_NO_THROW(client_->setResourceReadHandler(handler));
    
    // 注意：由于没有实际的资源读取，我们无法测试处理器是否被调用
    EXPECT_FALSE(handler_called);
}

/**
 * @brief 测试提示获取处理器设置
 */
TEST_F(MCPClientTest, PromptGetHandlerSetting) {
    bool handler_called = false;
    
    // 设置提示获取处理器
    auto handler = [&handler_called](const std::string& /*name*/, const json& /*arguments*/) -> json {
        handler_called = true;
        return json::object();
    };
    
    EXPECT_NO_THROW(client_->setPromptGetHandler(handler));
    
    // 注意：由于没有实际的提示获取，我们无法测试处理器是否被调用
    EXPECT_FALSE(handler_called);
}

/**
 * @brief 测试服务器信息获取（未初始化）
 */
TEST_F(MCPClientTest, ServerInfoRetrieval) {
    // 测试获取服务器信息（应该返回默认值）
    ServerInfo server_info = client_->getServerInfo();
    EXPECT_TRUE(server_info.name.empty());
    EXPECT_TRUE(server_info.version.empty());
    
    // 测试获取服务器能力（应该返回默认值）
    ServerCapabilities server_caps = client_->getServerCapabilities();
    EXPECT_TRUE(server_caps.tools.empty());
    EXPECT_TRUE(server_caps.resources.empty());
    EXPECT_TRUE(server_caps.prompts.empty());
}

/**
 * @brief 测试传输状态获取
 */
TEST_F(MCPClientTest, TransportStatusRetrieval) {
    // 测试获取传输状态
    auto status = client_->getTransportStatus();
    EXPECT_EQ(status, mcp_transport::TransportStatus::DISCONNECTED);
}

/**
 * @brief 测试端点解析功能
 */
TEST_F(MCPClientTest, EndpointParsing) {
    // 测试HTTP端点（应该失败，因为没有服务器）
    EXPECT_FALSE(client_->connect("http://localhost:8080"));
    
    // 测试HTTPS端点（应该失败，因为没有服务器）
    EXPECT_FALSE(client_->connect("https://api.example.com"));
    
    // 测试IPC端点（echo hello是有效命令，transport会连接成功）
    // 这里我们期望transport连接成功，但MCP协议会失败
    bool connected = client_->connect("cmd:echo hello");
    if (connected) {
        // 如果连接成功，测试MCP初始化应该失败
        // 注意：transport连接成功不等于MCP连接成功
        // 这里我们只测试端点解析是否正确
        client_->disconnect();
    }
    
    // 测试无效端点
    EXPECT_FALSE(client_->connect("invalid-endpoint"));
}

/**
 * @brief 测试并发操作
 */
TEST_F(MCPClientTest, ConcurrentOperations) {
    const int num_threads = 4;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 创建多个线程同时进行客户端操作
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([this, i, &success_count]() {
            try {
                // 每个线程设置不同的传输类型
                mcp_transport::TransportType type = static_cast<mcp_transport::TransportType>(i % 3);
                client_->setTransportType(type);
                
                // 设置客户端信息
                client_->setClientInfo("thread-" + std::to_string(i), "1.0.0");
                
                // 设置端点
                std::vector<std::string> endpoints = {"http://localhost:" + std::to_string(8080 + i)};
                client_->setEndpoints(endpoints);
                
                success_count++;
            } catch (...) {
                // 忽略异常，因为我们知道某些操作会失败
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
 * @brief 测试内存管理
 */
TEST_F(MCPClientTest, MemoryManagement) {
    // 创建多个客户端实例
    std::vector<std::unique_ptr<MCPClient>> clients;
    
    for (int i = 0; i < 10; ++i) {
        auto client = std::make_unique<MCPClient>();
        client->setClientInfo("client-" + std::to_string(i), "1.0.0");
        client->setTransportType(mcp_transport::TransportType::IPC);
        clients.push_back(std::move(client));
    }
    
    // 验证所有客户端都创建成功
    EXPECT_EQ(clients.size(), 10);
    
    // 清理客户端（测试析构函数）
    clients.clear();
    
    // 验证清理完成
    EXPECT_TRUE(clients.empty());
}

/**
 * @brief 测试错误处理
 */
TEST_F(MCPClientTest, ErrorHandling) {
    // 测试空字符串端点
        EXPECT_FALSE(client_->connect(""));
    
    // 测试nullptr参数处理（通过异常）
    EXPECT_THROW({
        ToolCallParams params;
        params.name = "";
        client_->callTool(params);
    }, mcp_exception);
    
    // 测试无效的JSON参数
    EXPECT_THROW({
        json invalid_json;
        client_->callTool("test-tool", invalid_json);
    }, mcp_exception);
}

/**
 * @brief 性能测试
 */
TEST_F(MCPClientTest, PerformanceTest) {
    g_test_logger->info("开始性能测试");
    
    const int iterations = 10;
    auto start_time = std::chrono::high_resolution_clock::now();
    
    g_test_logger->info("执行 " + std::to_string(iterations) + " 次操作");
    
    // 执行大量操作
    for (int i = 0; i < iterations; ++i) {
        client_->setClientInfo("perf-client", "1.0.0");
        client_->setTransportType(mcp_transport::TransportType::IPC);
        
        std::vector<std::string> endpoints = {"http://localhost:" + std::to_string(8080 + i)};
        client_->setEndpoints(endpoints);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // 验证性能（应该在合理时间内完成）
    EXPECT_LT(duration.count(), 100); // 应该在100ms内完成10次操作
    
    std::string perf_msg = "Performance test: " + std::to_string(iterations) + 
                          " operations completed in " + std::to_string(duration.count()) + "ms";
    
    g_test_logger->info(perf_msg);
    std::cout << perf_msg << std::endl;
    
    g_test_logger->info("性能测试完成");
}

// 主函数用于运行测试
int main(int argc, char** argv) {
    // 初始化日志系统
    LogConfig log_config;
    log_config.level = mcp_logger::LogLevel::INFO;
    log_config.target = mcp_logger::LogTarget::BOTH; // 同时输出到控制台和文件
    log_config.file_path = "mcp_client_gtest.log"; // 与文件名相同的日志文件
    log_config.enable_timestamp = true;
    log_config.enable_thread_id = true;
    log_config.enable_file_location = true;
    log_config.auto_flush = true;
    
    // 初始化全局日志系统
    Logger::getInstance().initialize(log_config);
    
    // 获取测试模块日志记录器
    g_test_logger = Logger::getInstance().getModuleLogger("mcp_client_test");
    
    // 记录测试开始
    g_test_logger->info("=== MCP Client Google Test 开始运行 ===");
    g_test_logger->info("日志文件: " + log_config.file_path);
    
    // 初始化Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // 运行测试
    int result = RUN_ALL_TESTS();
    
    // 记录测试结束
    g_test_logger->info("=== MCP Client Google Test 运行完成 ===");
    std::string result_msg = "测试结果: " + std::string(result == 0 ? "全部通过" : "有测试失败");
    g_test_logger->info(result_msg);
    
    // 关闭日志系统
    Logger::getInstance().shutdown();
    
    return result;
}
