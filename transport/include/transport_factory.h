#pragma once

#include "transport_base.h"
#include "stdio_transport.h"
#include "http_transport.h"
#include "https_transport.h"
#include <memory>
#include <string>

namespace mcp_transport {

/**
 * @brief TransportFactory - 传输类工厂
 * 
 * 根据配置自动创建对应的传输实现类
 * 支持以下传输类型：
 * - IPC/Stdio: StdioTransport
 * - HTTP: HttpTransport
 * - HTTPS: HttpsTransport
 */
class TransportFactory {
public:
    /**
     * @brief 根据配置创建传输实例
     * @param config 传输配置
     * @return 传输实例的智能指针，失败返回nullptr
     */
    static std::unique_ptr<TransportBase> create(const TransportConfig& config);
    
    /**
     * @brief 根据传输类型创建传输实例
     * @param type 传输类型
     * @return 传输实例的智能指针，失败返回nullptr
     */
    static std::unique_ptr<TransportBase> create(TransportType type);
    
    /**
     * @brief 根据类型名称创建传输实例
     * @param type_name 类型名称（"stdio", "http", "https"）
     * @return 传输实例的智能指针，失败返回nullptr
     */
    static std::unique_ptr<TransportBase> create(const std::string& type_name);
    
    /**
     * @brief 创建StdioTransport实例
     * @return StdioTransport实例的智能指针
     */
    static std::unique_ptr<StdioTransport> createStdioTransport();
    
    /**
     * @brief 创建HttpTransport实例
     * @return HttpTransport实例的智能指针
     */
    static std::unique_ptr<HttpTransport> createHttpTransport();
    
    /**
     * @brief 创建HttpsTransport实例
     * @return HttpsTransport实例的智能指针
     */
    static std::unique_ptr<HttpsTransport> createHttpsTransport();
    
    /**
     * @brief 从配置自动判断传输类型并创建实例
     * @param config 传输配置（可能包含AUTO类型）
     * @return 传输实例的智能指针，失败返回nullptr
     */
    static std::unique_ptr<TransportBase> createFromConfig(const TransportConfig& config);
    
    /**
     * @brief 获取传输类型名称
     * @param type 传输类型
     * @return 类型名称字符串
     */
    static std::string getTypeName(TransportType type);
    
    /**
     * @brief 从类型名称获取传输类型
     * @param type_name 类型名称
     * @return 传输类型，无效返回TransportType::AUTO
     */
    static TransportType getTypeFromName(const std::string& type_name);

private:
    /**
     * @brief 从配置自动判断传输类型
     * @param config 传输配置
     * @return 判断后的传输类型
     */
    static TransportType determineType(const TransportConfig& config);
};

} // namespace mcp_transport

