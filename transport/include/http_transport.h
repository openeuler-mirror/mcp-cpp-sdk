#pragma once

#include "transport_base.h"

namespace mcp_transport {

/**
 * @brief HttpTransport - HTTP传输实现
 * 
 * 通过HTTP协议进行通信的传输实现
 * 支持客户端和服务端模式
 */
class HttpTransport : public TransportBase {
public:
    HttpTransport();
    virtual ~HttpTransport();

    // 实现基类接口
    bool start(const TransportConfig& config) override;
    bool stop() override;
    ConnectionInfo getConnectionInfo() const override;

    std::future<TransportMessage> sendRequest(const TransportMessage& message) override;
    TransportMessage sendRequestSync(const TransportMessage& message) override;

    void setMessageCallback(MessageCallback callback) override;

    void setServerRequestHandler(std::shared_ptr<TransportServerRequestHandler> handler) override;
    void setSseServerRequestHandler(std::shared_ptr<SseServerRequestHandler> handler) override;
    void setConnectionRequestHandler(std::shared_ptr<SseServerConnectHandler> handler) override;

    // 实现基类纯虚函数
    std::string getTransportTypeName() const override;
    bool sendJsonRpcMessage(const std::string& json_message) override;
    nlohmann::json getExtendedStatistics() const override;
    bool doHealthCheck() const override;
    
    // 实现基类NetIO协议方法
    netio::Protocol getNetioProtocol() const override;
};

} // namespace mcp_transport

