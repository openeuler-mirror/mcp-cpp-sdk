/**
 * @file protocol_example.cpp
 * @brief MCP Protocol 使用示例
 */

#include "mcp_protocol_core.h"
#include "../../log/include/logger.h"
#include "../../common/json.hpp"

using namespace mcp;

int main() {
    auto logger = mcp_logger::getModuleLogger("mcp_protocol_example");
    logger->info("MCP Protocol Example");
    
    try {
        // 测试客户端能力创建
        auto client_caps = mcp::utils::createDefaultClientCapabilities();
        logger->info("Client capabilities created successfully");
        
        // 测试服务器能力创建
        auto server_caps = mcp::utils::createDefaultServerCapabilities();
        logger->info("Server capabilities created successfully");
        
        // 测试工具创建
        nlohmann::json schema = {
            {"type", "object"},
            {"properties", nlohmann::json::object()}
        };
        auto tool = mcp::utils::createToolFromSchema("test_tool", "A test tool", schema);
        logger->info("Tool created: " + tool.name);
        
        logger->info("✅ All protocol operations completed successfully");
        return 0;
        
    } catch (const std::exception& e) {
        logger->error("❌ Error: " + std::string(e.what()));
        return 1;
    }
}
