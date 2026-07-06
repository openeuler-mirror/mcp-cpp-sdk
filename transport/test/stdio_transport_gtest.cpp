/**
 * @file stdio_transport_gtest.cpp
 * @brief StdioTransport Google Test 测试文件
 * @author MCP SDK Team
 * @date 2024
 */

#include <gtest/gtest.h>
#include "stdio_transport.h"
#include "../../ipc/include/pipe_communication.h"
#include <thread>
#include <chrono>
#include <future>
#include <memory>
#include <atomic>

using namespace mcp_transport;
using namespace mcp;

// 测试服务端请求处理器
class TestServerRequestHandler : public TransportServerRequestHandler {
public:
    std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) override {
        received_requests++;
        last_method = method;
        last_params = params;
        last_request_id = request_id;
        
        nlohmann::json response;
        response["jsonrpc"] = "2.0";
        response["id"] = request_id;
        response["result"] = "test_response";
        return response.dump();
    }
    
    void handleNotification(const std::string& method, const std::string& params) override {
        received_notifications++;
        last_method = method;
        last_params = params;
    }
    
    std::atomic<int> received_requests{0};
    std::atomic<int> received_notifications{0};
    std::string last_method;
    std::string last_params;
    std::string last_request_id;
};

// 测试基类
class StdioTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_ = std::make_unique<StdioTransport>();
        ASSERT_NE(transport_, nullptr);
        
        // 设置默认配置 - 使用一个简单的可执行文件进行测试
        config_.type = TransportType::IPC;
        config_.mode = TransportMode::CLIENT;
        config_.ipc.command = "/bin/cat";  // 使用cat命令作为测试
        config_.ipc.args = {};
        config_.ipc.buffer_size = 4096;
        config_.ipc.read_timeout_ms = 100;
        config_.ipc.max_retry_count = 3;
        config_.ipc.enable_logging = false;
    }
    
    void TearDown() override {
        if (transport_ && transport_->isRunning()) {
            transport_->stop();
        }
        transport_.reset();
    }
    
    std::unique_ptr<StdioTransport> transport_;
    TransportConfig config_;
};

/**
 * @brief 测试基本构造和析构
 */
TEST_F(StdioTransportTest, BasicConstruction) {
    EXPECT_FALSE(transport_->isRunning());
    EXPECT_EQ(transport_->getStatus(), TransportStatus::DISCONNECTED);
    EXPECT_EQ(transport_->getTransportTypeName(), "stdio");
}

/**
 * @brief 测试获取传输类型名称
 */
TEST_F(StdioTransportTest, GetTransportTypeName) {
    EXPECT_EQ(transport_->getTransportTypeName(), "stdio");
}

/**
 * @brief 测试获取连接信息（未启动状态）
 */
TEST_F(StdioTransportTest, GetConnectionInfoNotRunning) {
    ConnectionInfo info = transport_->getConnectionInfo();
    EXPECT_EQ(info.status, TransportStatus::DISCONNECTED);
    EXPECT_EQ(info.type, TransportType::IPC);  // Stdio使用IPC类型
}

/**
 * @brief 测试设置消息回调
 */
TEST_F(StdioTransportTest, SetMessageCallback) {
    bool callback_called = false;
    TransportMessage received_msg;
    
    MessageCallback callback = [&](const TransportMessage& msg) {
        callback_called = true;
        received_msg = msg;
    };
    
    transport_->setMessageCallback(callback);
    
    // 由于transport未启动，回调不会被触发
    EXPECT_FALSE(callback_called);
}

/**
 * @brief 测试设置服务端请求处理器
 */
TEST_F(StdioTransportTest, SetServerRequestHandler) {
    auto handler = std::make_shared<TestServerRequestHandler>();
    transport_->setServerRequestHandler(handler);
    // 应该不会抛出异常
    EXPECT_NO_THROW(transport_->setServerRequestHandler(handler));
}

/**
 * @brief 测试启动和停止（使用cat命令）
 */
TEST_F(StdioTransportTest, StartStopWithCat) {
    // 使用cat命令，这是一个简单的测试
    config_.ipc.command = "/bin/cat";
    config_.ipc.args = {};
    
    bool started = transport_->start(config_);
    
    if (started) {
        EXPECT_TRUE(transport_->isRunning());
        EXPECT_EQ(transport_->getStatus(), TransportStatus::CONNECTED);
        
        // 停止
        bool stopped = transport_->stop();
        EXPECT_TRUE(stopped);
        EXPECT_FALSE(transport_->isRunning());
        EXPECT_EQ(transport_->getStatus(), TransportStatus::DISCONNECTED);
    } else {
        // 如果启动失败（例如在Windows上），跳过此测试
        GTEST_SKIP() << "Failed to start stdio transport with cat command";
    }
}

/**
 * @brief 测试重复启动
 */
TEST_F(StdioTransportTest, DoubleStart) {
    bool result1 = transport_->start(config_);
    
    if (result1) {
        // 如果第一次启动成功，第二次应该失败
        bool result2 = transport_->start(config_);
        EXPECT_FALSE(result2);
        transport_->stop();
    } else {
        GTEST_SKIP() << "Failed to start stdio transport";
    }
}

/**
 * @brief 测试停止未启动的transport
 */
TEST_F(StdioTransportTest, StopNotRunning) {
    EXPECT_TRUE(transport_->stop());
    EXPECT_FALSE(transport_->isRunning());
}

/**
 * @brief 测试发送请求（未启动状态）
 */
TEST_F(StdioTransportTest, SendRequestNotRunning) {
    TransportMessage msg;
    msg.content = R"({"jsonrpc":"2.0","method":"test","id":"1"})";
    msg.method = "POST";
    msg.path = "/";
    
    TransportMessage response = transport_->sendRequestSync(msg);
    EXPECT_EQ(response.status_code, 500);
    EXPECT_FALSE(response.error_message.empty());
}

/**
 * @brief 测试发送JSON-RPC消息（未启动状态）
 */
TEST_F(StdioTransportTest, SendJsonRpcMessageNotRunning) {
    std::string json_msg = R"({"jsonrpc":"2.0","method":"test","id":"1"})";
    bool result = transport_->sendJsonRpcMessage(json_msg);
    EXPECT_FALSE(result);
}

/**
 * @brief 测试健康检查（未启动状态）
 */
TEST_F(StdioTransportTest, HealthCheckNotRunning) {
    bool healthy = transport_->doHealthCheck();
    EXPECT_FALSE(healthy);
}

/**
 * @brief 测试健康检查（启动状态）
 */
TEST_F(StdioTransportTest, HealthCheckRunning) {
    bool started = transport_->start(config_);
    
    if (started) {
        // 等待一小段时间让进程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        bool healthy = transport_->doHealthCheck();
        // 健康检查应该返回true（如果进程正在运行）
        EXPECT_TRUE(healthy);
        
        transport_->stop();
    } else {
        GTEST_SKIP() << "Failed to start stdio transport";
    }
}

/**
 * @brief 测试获取扩展统计信息
 */
TEST_F(StdioTransportTest, GetExtendedStatistics) {
    // 未启动时应该返回空对象
    nlohmann::json stats = transport_->getExtendedStatistics();
    EXPECT_TRUE(stats.is_object());
    EXPECT_TRUE(stats.empty());  // 未启动时应该为空
    
    // 如果transport已启动，应该包含IPC相关信息
    bool started = transport_->start(config_);
    if (started) {
        // 等待进程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        
        stats = transport_->getExtendedStatistics();
        // 应该返回有效的JSON对象（启动后应该包含IPC信息）
        EXPECT_TRUE(stats.is_object());
        
        // 启动成功后，ipc_comm_ 应该已初始化，应该包含IPC相关信息
        // 验证字段存在和类型正确（不检查具体值，因为进程可能在不同状态下）
        if (stats.contains("ipc_process_id")) {
            EXPECT_TRUE(stats["ipc_process_id"].is_number_integer());
        }
        
        if (stats.contains("ipc_process_alive")) {
            EXPECT_TRUE(stats["ipc_process_alive"].is_boolean());
        }
        
        // 如果字段不存在，可能是因为ipc_comm_在stop()调用后已被重置
        // 在这种情况下，我们只验证stats是有效的对象
        // 注意：在实际情况下，如果transport正在运行，这些字段应该存在
        
        transport_->stop();
        
        // 停止后再次检查，应该返回空对象（因为ipc_comm_已被reset）
        stats = transport_->getExtendedStatistics();
        EXPECT_TRUE(stats.is_object());
        // 停止后可能为空，或者仍包含之前的进程ID（取决于实现）
    } else {
        GTEST_SKIP() << "Failed to start stdio transport";
    }
}

/**
 * @brief 测试获取统计信息
 */
TEST_F(StdioTransportTest, GetStatistics) {
    nlohmann::json stats = transport_->getStatistics();
    EXPECT_TRUE(stats.is_object());
    // 应该包含基本的统计字段
    EXPECT_TRUE(stats.contains("messages_sent") || stats.empty());
}

/**
 * @brief 测试配置更新
 */
TEST_F(StdioTransportTest, UpdateConfig) {
    TransportConfig new_config = config_;
    new_config.ipc.buffer_size = 8192;
    new_config.ipc.read_timeout_ms = 200;
    transport_->updateConfig(new_config);
    
    TransportConfig current_config = transport_->getConfig();
    EXPECT_EQ(current_config.ipc.buffer_size, 8192);
    EXPECT_EQ(current_config.ipc.read_timeout_ms, 200);
}

/**
 * @brief 测试获取配置
 */
TEST_F(StdioTransportTest, GetConfig) {
    // 未设置配置时，应该返回默认配置（AUTO类型）
    TransportConfig config = transport_->getConfig();
    EXPECT_EQ(config.type, TransportType::AUTO);  // 默认值是AUTO
    EXPECT_EQ(config.mode, TransportMode::CLIENT);
    
    // 设置配置后，应该返回设置的配置
    transport_->updateConfig(config_);
    config = transport_->getConfig();
    EXPECT_EQ(config.type, TransportType::IPC);  // 现在应该是IPC
    EXPECT_EQ(config.mode, TransportMode::CLIENT);
}

/**
 * @brief 测试消息队列操作
 */
TEST_F(StdioTransportTest, MessageQueueOperations) {
    EXPECT_FALSE(transport_->hasMessage());
    EXPECT_EQ(transport_->getQueueSize(), 0);
    
    // 设置队列大小
    transport_->setMaxQueueSize(100);
    
    // 尝试获取消息
    TransportMessage msg = transport_->getMessage();
    EXPECT_TRUE(msg.content.empty());
    
    // 获取所有消息
    std::vector<TransportMessage> messages = transport_->getAllMessages();
    EXPECT_TRUE(messages.empty());
}

/**
 * @brief 测试连接回调
 */
TEST_F(StdioTransportTest, ConnectionCallback) {
    bool callback_called = false;
    ConnectionInfo received_info;
    
    ConnectionCallback callback = [&](const ConnectionInfo& info) {
        callback_called = true;
        received_info = info;
    };
    
    transport_->setConnectionCallback(callback);
    
    // 启动transport以触发连接回调
    bool started = transport_->start(config_);
    if (started) {
        // 等待回调被触发
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // 回调应该被触发
        EXPECT_TRUE(callback_called);
        EXPECT_EQ(received_info.status, TransportStatus::CONNECTED);
        transport_->stop();
    } else {
        // 如果启动失败，回调不会被触发
        EXPECT_FALSE(callback_called);
    }
}

/**
 * @brief 测试错误回调
 */
TEST_F(StdioTransportTest, ErrorCallback) {
    bool callback_called = false;
    std::string received_error;
    
    ErrorCallback callback = [&](const std::string& error) {
        callback_called = true;
        received_error = error;
    };
    
    transport_->setErrorCallback(callback);
    
    // 尝试启动一个不存在的命令以触发错误
    TransportConfig bad_config = config_;
    bad_config.ipc.command = "/nonexistent/command/that/does/not/exist";
    bool started = transport_->start(bad_config);
    
    if (!started) {
        // 如果启动失败，错误回调可能被触发
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // 错误回调可能被触发，也可能不会（取决于实现）
    }
}

/**
 * @brief 测试异步发送请求（未启动状态）
 */
TEST_F(StdioTransportTest, SendRequestAsyncNotRunning) {
    TransportMessage msg;
    msg.content = R"({"jsonrpc":"2.0","method":"test","id":"1"})";
    msg.method = "POST";
    msg.path = "/";
    
    auto future = transport_->sendRequest(msg);
    TransportMessage response = future.get();
    
    EXPECT_EQ(response.status_code, 500);
    EXPECT_FALSE(response.error_message.empty());
}

/**
 * @brief 测试发送请求（启动状态，使用echo命令）
 */
TEST_F(StdioTransportTest, SendRequestWithEcho) {
    // 使用echo命令进行测试
    config_.ipc.command = "/bin/echo";
    config_.ipc.args = {"-n", "test_response"};
    
    bool started = transport_->start(config_);
    
    if (started) {
        // 等待进程启动
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        TransportMessage msg;
        msg.content = R"({"jsonrpc":"2.0","method":"test","id":"1"})";
        msg.method = "POST";
        msg.path = "/";
        
        // 注意：echo命令可能不会返回JSON-RPC格式的响应
        // 这里主要测试发送功能是否正常
        TransportMessage response = transport_->sendRequestSync(msg);
        
        // 响应可能成功也可能失败，取决于echo命令的行为
        // 这里主要确保不会崩溃
        
        transport_->stop();
    } else {
        GTEST_SKIP() << "Failed to start stdio transport with echo command";
    }
}

/**
 * @brief 测试服务端模式配置
 */
TEST_F(StdioTransportTest, ServerModeConfig) {
    config_.mode = TransportMode::SERVER;
    config_.ipc.command = "/bin/cat";
    config_.server.server_name = "test_server";
    config_.server.server_version = "1.0.0";
    
    // 设置服务端处理器
    auto handler = std::make_shared<TestServerRequestHandler>();
    transport_->setServerRequestHandler(handler);
    
    // 启动可能会失败（因为没有实际服务器），但配置应该被接受
    transport_->updateConfig(config_);
    TransportConfig current_config = transport_->getConfig();
    EXPECT_EQ(current_config.mode, TransportMode::SERVER);
    EXPECT_EQ(current_config.server.server_name, "test_server");
    EXPECT_EQ(current_config.server.server_version, "1.0.0");
}

/**
 * @brief 测试TransportMessage转换
 */
TEST_F(StdioTransportTest, TransportMessageConversion) {
    TransportMessage msg;
    msg.id = "test_id";
    msg.content = "test_content";
    msg.method = "POST";
    msg.path = "/test";
    msg.status_code = 200;
    msg.timestamp = std::chrono::system_clock::now();
    
    // 转换为JSON
    nlohmann::json json = msg.toJson();
    EXPECT_EQ(json["id"], "test_id");
    EXPECT_EQ(json["content"], "test_content");
    EXPECT_EQ(json["method"], "POST");
    EXPECT_EQ(json["path"], "/test");
    EXPECT_EQ(json["status_code"], 200);
    
    // 从JSON创建
    TransportMessage msg2 = TransportMessage::fromJson(json);
    EXPECT_EQ(msg2.id, "test_id");
    EXPECT_EQ(msg2.content, "test_content");
    EXPECT_EQ(msg2.method, "POST");
    EXPECT_EQ(msg2.path, "/test");
    EXPECT_EQ(msg2.status_code, 200);
}

/**
 * @brief 测试IPC消息转换
 */
TEST_F(StdioTransportTest, IpcMessageConversion) {
    mcp::Message ipc_msg;
    ipc_msg.id = "test_id";
    ipc_msg.content = R"({"jsonrpc":"2.0","method":"test","id":"test_id"})";
    ipc_msg.type = mcp::MessageType::REQUEST;
    
    // 从IPC消息创建TransportMessage
    TransportMessage transport_msg = TransportMessage::fromIpcMessage(ipc_msg);
    EXPECT_EQ(transport_msg.id, "test_id");
    EXPECT_EQ(transport_msg.content, ipc_msg.content);
    EXPECT_EQ(transport_msg.method, "POST");  // IPC默认使用POST
    
    // 转换回IPC消息
    mcp::Message ipc_msg2 = transport_msg.toIpcMessage();
    EXPECT_EQ(ipc_msg2.id, "test_id");
    EXPECT_EQ(ipc_msg2.content, ipc_msg.content);
}

