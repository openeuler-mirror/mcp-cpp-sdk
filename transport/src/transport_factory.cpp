#include "../include/transport_factory.h"
#include "../../log/include/logger.h"

namespace mcp_transport {

std::unique_ptr<TransportBase> TransportFactory::create(const TransportConfig& config) {
    return createFromConfig(config);
}

std::unique_ptr<TransportBase> TransportFactory::create(TransportType type) {
    switch (type) {
        case TransportType::IPC:
            return std::make_unique<StdioTransport>();
        case TransportType::HTTP:
            return std::make_unique<HttpTransport>();
        case TransportType::HTTPS:
            return std::make_unique<HttpsTransport>();
        case TransportType::AUTO:
        default:
            if (auto logger = mcp_logger::getModuleLogger("transport_factory")) {
                logger->warning("Cannot create transport with AUTO type, use createFromConfig() instead");
            }
            return nullptr;
    }
}

std::unique_ptr<TransportBase> TransportFactory::create(const std::string& type_name) {
    TransportType type = getTypeFromName(type_name);
    if (type == TransportType::AUTO && type_name != "auto") {
        if (auto logger = mcp_logger::getModuleLogger("transport_factory")) {
            logger->error("Unknown transport type name: " + type_name);
        }
        return nullptr;
    }
    
    if (type == TransportType::IPC) {
        return std::make_unique<StdioTransport>();
    } else if (type == TransportType::HTTP) {
        return std::make_unique<HttpTransport>();
    } else if (type == TransportType::HTTPS) {
        return std::make_unique<HttpsTransport>();
    }
    
    return nullptr;
}

std::unique_ptr<StdioTransport> TransportFactory::createStdioTransport() {
    return std::make_unique<StdioTransport>();
}

std::unique_ptr<HttpTransport> TransportFactory::createHttpTransport() {
    return std::make_unique<HttpTransport>();
}

std::unique_ptr<HttpsTransport> TransportFactory::createHttpsTransport() {
    return std::make_unique<HttpsTransport>();
}

std::unique_ptr<TransportBase> TransportFactory::createFromConfig(const TransportConfig& config) {
    TransportType actual_type = determineType(config);
    
    switch (actual_type) {
        case TransportType::IPC:
            return std::make_unique<StdioTransport>();
            
        case TransportType::HTTP:
            return std::make_unique<HttpTransport>();
            
        case TransportType::HTTPS:
            return std::make_unique<HttpsTransport>();
        
        case TransportType::AUTO:
        default: {
            if (auto logger = mcp_logger::getModuleLogger("transport_factory")) {
                logger->error("Failed to determine transport type from config");
            }
            return nullptr;
        }
    }
}

std::string TransportFactory::getTypeName(TransportType type) {
    switch (type) {
        case TransportType::IPC:
            return "stdio";
        case TransportType::HTTP:
            return "http";
        case TransportType::HTTPS:
            return "https";
        case TransportType::AUTO:
            return "auto";
        default:
            return "unknown";
    }
}

TransportType TransportFactory::getTypeFromName(const std::string& type_name) {
    if (type_name == "stdio" || type_name == "ipc" || type_name == "IPC") {
        return TransportType::IPC;
    } else if (type_name == "http" || type_name == "HTTP") {
        return TransportType::HTTP;
    } else if (type_name == "https" || type_name == "HTTPS") {
        return TransportType::HTTPS;
    } else if (type_name == "sse" || type_name == "SSE") {
        // 为了向后兼容，SSE 映射到 HTTP
        return TransportType::HTTP;
    } else if (type_name == "auto" || type_name == "AUTO") {
        return TransportType::AUTO;
    }
    return TransportType::AUTO;
}

TransportType TransportFactory::determineType(const TransportConfig& config) {
    // 如果已经指定了类型，直接返回
    if (config.type != TransportType::AUTO) {
        return config.type;
    }
    
    // AUTO模式：根据配置自动判断
    // 优先检查IPC配置
    if (!config.ipc.command.empty()) {
        if (auto logger = mcp_logger::getModuleLogger("transport_factory")) {
            logger->info("Auto-detected transport type: IPC (from command: " + config.ipc.command + ")");
        }
        return TransportType::IPC;
    }
    
    // 检查HTTP/HTTPS配置
    if (!config.sse.host.empty() && config.sse.port > 0) {
        TransportType detected_type = (config.sse.protocol == netio::Protocol::HTTPS) 
                                    ? TransportType::HTTPS 
                                    : TransportType::HTTP;
        if (auto logger = mcp_logger::getModuleLogger("transport_factory")) {
            std::string type_str = (detected_type == TransportType::HTTPS) ? "HTTPS" : "HTTP";
            logger->info("Auto-detected transport type: " + type_str +
                        " (host: " + config.sse.host + 
                        ", port: " + std::to_string(config.sse.port) + ")");
        }
        return detected_type;
    }
    
    // 无法确定类型
    if (auto logger = mcp_logger::getModuleLogger("transport_factory")) {
        logger->error("Cannot determine transport type: both IPC and HTTP/HTTPS configs are incomplete");
    }
    return TransportType::AUTO;
}

} // namespace mcp_transport

