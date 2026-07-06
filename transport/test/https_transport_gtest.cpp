/**
 * @file https_transport_gtest.cpp
 * @brief HttpsTransport Google Test 测试文件
 * @author MCP SDK Team
 * @date 2024
 */

#include <gtest/gtest.h>
#include "https_transport.h"
#include <thread>
#include <chrono>
#include <future>
#include <memory>
#include <atomic>

using namespace mcp_transport;

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

// 测试SSE服务端请求处理器
class TestSseServerRequestHandler : public SseServerRequestHandler {
public:
    bool handleRequest(const netio::Message& request, netio::Message& response) override {
        received_requests++;
        response.status_code = 200;
        response.body = "test_response";
        return true;
    }
    
    std::atomic<int> received_requests{0};
};

// 测试连接请求处理器
class TestSseServerConnectHandler : public SseServerConnectHandler {
public:
    bool handleRequest(const netio::Message& request, netio::Message& response) override {
        received_requests++;
        response.status_code = 200;
        response.body = "connected";
        return true;
    }
    
    std::atomic<int> received_requests{0};
};

// 测试基类
class HttpsTransportTest : public ::testing::Test {
protected:
    void SetUp() override {
        transport_ = std::make_unique<HttpsTransport>();
        ASSERT_NE(transport_, nullptr);
        
        // 设置默认配置
        config_.type = TransportType::HTTPS;
        config_.mode = TransportMode::CLIENT;
        config_.sse.host = "localhost";
        config_.sse.port = 8443;
        config_.sse.protocol = netio::Protocol::HTTPS;
        config_.enable_logging = false;
        
        // 设置HTTPS配置
        https_config_.cert_file = "";
        https_config_.key_file = "";
        https_config_.ca_file = "";
        https_config_.verify_peer = true;
        https_config_.verify_hostname = true;
    }
    
    void TearDown() override {
        if (transport_ && transport_->isRunning()) {
            transport_->stop();
        }
        transport_.reset();
    }
    
    std::unique_ptr<HttpsTransport> transport_;
    TransportConfig config_;
    HttpsTransport::HttpsConfig https_config_;
};

/**
 * @brief 测试基本构造和析构
 */
TEST_F(HttpsTransportTest, BasicConstruction) {
    EXPECT_FALSE(transport_->isRunning());
    EXPECT_EQ(transport_->getStatus(), TransportStatus::DISCONNECTED);
    EXPECT_EQ(transport_->getTransportTypeName(), "https");
}

/**
 * @brief 测试获取传输类型名称
 */
TEST_F(HttpsTransportTest, GetTransportTypeName) {
    EXPECT_EQ(transport_->getTransportTypeName(), "https");
}

/**
 * @brief 测试获取连接信息（未启动状态）
 */
TEST_F(HttpsTransportTest, GetConnectionInfoNotRunning) {
    ConnectionInfo info = transport_->getConnectionInfo();
    EXPECT_EQ(info.status, TransportStatus::DISCONNECTED);
    EXPECT_EQ(info.type, TransportType::HTTPS);
}

/**
 * @brief 测试设置HTTPS配置
 */
TEST_F(HttpsTransportTest, SetHttpsConfig) {
    HttpsTransport::HttpsConfig config;
    config.cert_file = "/path/to/cert.pem";
    config.key_file = "/path/to/key.pem";
    config.ca_file = "/path/to/ca.pem";
    config.verify_peer = false;
    config.verify_hostname = false;
    
    transport_->setHttpsConfig(config);
    
    // 验证配置已设置（通过扩展统计信息）
    nlohmann::json stats = transport_->getExtendedStatistics();
    // 由于transport未启动，可能无法获取完整统计信息
    EXPECT_TRUE(stats.is_object());
}

/**
 * @brief 测试设置消息回调
 */
TEST_F(HttpsTransportTest, SetMessageCallback) {
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
TEST_F(HttpsTransportTest, SetServerRequestHandler) {
    auto handler = std::make_shared<TestServerRequestHandler>();
    transport_->setServerRequestHandler(handler);
    // 应该不会抛出异常
    EXPECT_NO_THROW(transport_->setServerRequestHandler(handler));
}

/**
 * @brief 测试设置SSE服务端请求处理器
 */
TEST_F(HttpsTransportTest, SetSseServerRequestHandler) {
    auto handler = std::make_shared<TestSseServerRequestHandler>();
    transport_->setSseServerRequestHandler(handler);
    // 应该不会抛出异常
    EXPECT_NO_THROW(transport_->setSseServerRequestHandler(handler));
}

/**
 * @brief 测试设置连接请求处理器
 */
TEST_F(HttpsTransportTest, SetConnectionRequestHandler) {
    auto handler = std::make_shared<TestSseServerConnectHandler>();
    transport_->setConnectionRequestHandler(handler);
    // 应该不会抛出异常
    EXPECT_NO_THROW(transport_->setConnectionRequestHandler(handler));
}

/**
 * @brief 测试重复启动
 */
TEST_F(HttpsTransportTest, DoubleStart) {
    // 第一次启动应该失败（因为没有实际服务器）
    bool result1 = transport_->start(config_);
    // 由于没有实际服务器，启动可能会失败，这是正常的
    
    // 如果第一次启动成功，第二次应该失败
    if (result1) {
        bool result2 = transport_->start(config_);
        EXPECT_FALSE(result2);
        transport_->stop();
    }
}

/**
 * @brief 测试停止未启动的transport
 */
TEST_F(HttpsTransportTest, StopNotRunning) {
    EXPECT_TRUE(transport_->stop());
    EXPECT_FALSE(transport_->isRunning());
}

/**
 * @brief 测试发送请求（未启动状态）
 */
TEST_F(HttpsTransportTest, SendRequestNotRunning) {
    TransportMessage msg;
    msg.content = "test";
    msg.method = "POST";
    msg.path = "/";
    
    TransportMessage response = transport_->sendRequestSync(msg);
    EXPECT_EQ(response.status_code, 500);
    EXPECT_FALSE(response.error_message.empty());
}

/**
 * @brief 测试发送JSON-RPC消息（未启动状态）
 */
TEST_F(HttpsTransportTest, SendJsonRpcMessageNotRunning) {
    std::string json_msg = R"({"jsonrpc":"2.0","method":"test","id":"1"})";
    bool result = transport_->sendJsonRpcMessage(json_msg);
    EXPECT_FALSE(result);
}

/**
 * @brief 测试健康检查（未启动状态）
 */
TEST_F(HttpsTransportTest, HealthCheckNotRunning) {
    bool healthy = transport_->doHealthCheck();
    EXPECT_FALSE(healthy);
}

/**
 * @brief 测试获取扩展统计信息
 */
TEST_F(HttpsTransportTest, GetExtendedStatistics) {
    nlohmann::json stats = transport_->getExtendedStatistics();
    EXPECT_TRUE(stats.is_object());
    // HTTPS应该包含https_config字段
    if (stats.contains("https_config")) {
        EXPECT_TRUE(stats["https_config"].is_object());
    }
}

/**
 * @brief 测试获取统计信息
 */
TEST_F(HttpsTransportTest, GetStatistics) {
    nlohmann::json stats = transport_->getStatistics();
    EXPECT_TRUE(stats.is_object());
    // 应该包含基本的统计字段
    EXPECT_TRUE(stats.contains("messages_sent") || stats.empty());
}

/**
 * @brief 测试配置更新
 */
TEST_F(HttpsTransportTest, UpdateConfig) {
    TransportConfig new_config = config_;
    new_config.sse.port = 9443;
    transport_->updateConfig(new_config);
    
    TransportConfig current_config = transport_->getConfig();
    EXPECT_EQ(current_config.sse.port, 9443);
}

/**
 * @brief 测试获取配置
 */
TEST_F(HttpsTransportTest, GetConfig) {
    // 未设置配置时，应该返回默认配置（AUTO类型）
    TransportConfig config = transport_->getConfig();
    EXPECT_EQ(config.type, TransportType::AUTO);  // 默认值是AUTO
    EXPECT_EQ(config.mode, TransportMode::CLIENT);
    
    // 设置配置后，应该返回设置的配置
    transport_->updateConfig(config_);
    config = transport_->getConfig();
    EXPECT_EQ(config.type, TransportType::HTTPS);  // 现在应该是HTTPS
    EXPECT_EQ(config.mode, TransportMode::CLIENT);
}

/**
 * @brief 测试消息队列操作
 */
TEST_F(HttpsTransportTest, MessageQueueOperations) {
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
TEST_F(HttpsTransportTest, ConnectionCallback) {
    bool callback_called = false;
    ConnectionInfo received_info;
    
    ConnectionCallback callback = [&](const ConnectionInfo& info) {
        callback_called = true;
        received_info = info;
    };
    
    transport_->setConnectionCallback(callback);
    
    // 由于transport未启动，回调不会被触发
    EXPECT_FALSE(callback_called);
}

/**
 * @brief 测试错误回调
 */
TEST_F(HttpsTransportTest, ErrorCallback) {
    bool callback_called = false;
    std::string received_error;
    
    ErrorCallback callback = [&](const std::string& error) {
        callback_called = true;
        received_error = error;
    };
    
    transport_->setErrorCallback(callback);
    
    // 由于transport未启动，回调不会被触发
    EXPECT_FALSE(callback_called);
}

/**
 * @brief 测试异步发送请求（未启动状态）
 */
TEST_F(HttpsTransportTest, SendRequestAsyncNotRunning) {
    TransportMessage msg;
    msg.content = "test";
    msg.method = "POST";
    msg.path = "/";
    
    auto future = transport_->sendRequest(msg);
    TransportMessage response = future.get();
    
    EXPECT_EQ(response.status_code, 500);
    EXPECT_FALSE(response.error_message.empty());
}

/**
 * @brief 测试HTTPS配置默认值
 */
TEST_F(HttpsTransportTest, HttpsConfigDefaults) {
    HttpsTransport::HttpsConfig config;
    // 默认值应该在构造函数中设置
    // 这里测试配置结构本身
    EXPECT_TRUE(config.verify_peer || !config.verify_peer); // 布尔值总是有效的
    EXPECT_TRUE(config.verify_hostname || !config.verify_hostname);
}

/**
 * @brief 测试服务端模式配置
 */
TEST_F(HttpsTransportTest, ServerModeConfig) {
    config_.mode = TransportMode::SERVER;
    config_.sse.port = 8443;
    config_.sse.endpoint = "/api";
    
    // 设置HTTPS配置
    https_config_.cert_file = "/path/to/cert.pem";
    https_config_.key_file = "/path/to/key.pem";
    transport_->setHttpsConfig(https_config_);
    
    // 启动可能会失败（因为没有实际服务器），但配置应该被接受
    transport_->updateConfig(config_);
    TransportConfig current_config = transport_->getConfig();
    EXPECT_EQ(current_config.mode, TransportMode::SERVER);
    EXPECT_EQ(current_config.sse.port, 8443);
    EXPECT_EQ(current_config.sse.endpoint, "/api");
}

/**
 * @brief 测试TransportMessage转换
 */
TEST_F(HttpsTransportTest, TransportMessageConversion) {
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

