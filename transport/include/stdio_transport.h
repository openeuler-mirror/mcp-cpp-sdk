#pragma once

#include "transport_base.h"
#include "../../ipc/include/pipe_communication.h"

namespace mcp_transport {

/**
 * @brief StdioTransport - 标准输入输出传输实现
 * 
 * 通过标准输入输出（stdin/stdout）进行通信的传输实现
 * 使用IPC模块（PipeCommunication）进行进程间通信
 * 适用于进程间通信，特别是父子进程之间的通信
 */
class StdioTransport : public TransportBase {
public:
    StdioTransport();
    virtual ~StdioTransport();

    // 实现基类接口
    bool start(const TransportConfig& config) override;
    bool stop() override;
    ConnectionInfo getConnectionInfo() const override;

    std::future<TransportMessage> sendRequest(const TransportMessage& message) override;
    TransportMessage sendRequestSync(const TransportMessage& message) override;

    void setMessageCallback(MessageCallback callback) override;

    void setServerRequestHandler(std::shared_ptr<TransportServerRequestHandler> handler) override;

    // 实现基类纯虚函数
    std::string getTransportTypeName() const override;
    bool sendJsonRpcMessage(const std::string& json_message) override;
    nlohmann::json getExtendedStatistics() const override;
    bool doHealthCheck() const override;

private:
    // IPC消息处理器适配器
    class IpcMessageHandlerAdapter : public mcp::MessageHandler {
    public:
        explicit IpcMessageHandlerAdapter(StdioTransport* transport) : transport_(transport) {}
        
        void onMessage(const mcp::Message& message) override;
        void onError(const std::string& error) override;
        
    private:
        StdioTransport* transport_;
    };
    
    // IPC服务端请求处理器适配器
    class IpcServerRequestHandlerAdapter : public mcp::ServerRequestHandler {
    public:
        explicit IpcServerRequestHandlerAdapter(StdioTransport* transport) : transport_(transport) {}
        
        std::string handleRequest(const std::string& method, const std::string& params, const std::string& request_id) override;
        void handleNotification(const std::string& method, const std::string& params) override;
        
    private:
        StdioTransport* transport_;
    };

    // 成员变量
    std::unique_ptr<mcp::PipeCommunication> ipc_comm_;
    std::shared_ptr<IpcMessageHandlerAdapter> message_handler_adapter_;
    std::shared_ptr<IpcServerRequestHandlerAdapter> server_handler_adapter_;
    
    // 服务端处理器
    std::shared_ptr<TransportServerRequestHandler> server_handler_;
};

} // namespace mcp_transport

