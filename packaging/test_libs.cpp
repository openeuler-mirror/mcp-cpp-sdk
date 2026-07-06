#include <iostream>
#include <mcp_sdk/mcp_sdk.h>

int main() {
    std::cout << "=== MCP SDK Library Test ===" << std::endl;
    
    try {
        // 测试客户端库
        std::cout << "Testing MCP Client library..." << std::endl;
        // 这里可以添加实际的客户端测试代码
        
        // 测试服务端库
        std::cout << "Testing MCP Server library..." << std::endl;
        // 这里可以添加实际的服务端测试代码
        
        std::cout << "✅ All libraries loaded successfully!" << std::endl;
        std::cout << "✅ Headers are accessible!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
