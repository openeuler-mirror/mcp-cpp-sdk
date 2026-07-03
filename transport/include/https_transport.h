#pragma once

#include "transport_base.h"

namespace mcp_transport {

/**
 * @brief HttpsTransport - HTTPS传输实现
 * 
 * 通过HTTPS（加密的HTTP）协议进行通信的传输实现
 * 支持客户端和服务端模式，使用SSL/TLS加密
 */
class HttpsTransport : public TransportBase {
public:
    HttpsTransport();
    virtual ~HttpsTransport();

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

    // HTTPS特有配置
    struct HttpsConfig {
        std::string cert_file;      // 证书文件路径
        std::string key_file;       // 私钥文件路径
        std::string ca_file;        // CA证书文件路径
        bool verify_peer = true;    // 是否验证对等方证书
        bool verify_hostname = true; // 是否验证主机名
    };
    
    void setHttpsConfig(const HttpsConfig& https_config);
    
    // 覆盖基类配置方法以添加SSL配置
    void configureNetIO(netio::NetConfig& net_config) override;

private:
    // HTTPS配置
    HttpsConfig https_config_;
};

} // namespace mcp_transport

