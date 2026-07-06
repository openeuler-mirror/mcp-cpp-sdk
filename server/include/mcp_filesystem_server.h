/**
 * @file mcp_filesystem_server.h
 * @brief MCP FileSystem Server Implementation
 * 
 * This file provides a specialized MCP server implementation for file system operations.
 * It extends the base MCPServer with file system specific tools and resources.
 */

#ifndef MCP_FILESYSTEM_SERVER_H
#define MCP_FILESYSTEM_SERVER_H

#include "mcp_server.h"
#include <string>
#include <vector>

namespace mcp {

/**
 * @brief MCP FileSystem Server
 * 
 * A specialized MCP server that provides file system operations including:
 * - File reading and writing
 * - Directory listing and creation
 * - File system resource management
 * - Path validation and security
 */
class MCPFileSystemServer : public MCPServer {
public:
    /**
     * @brief Constructor
     * @param root_path The root path for file operations (default: current directory)
     * @param enable_write Whether to enable write operations (default: true)
     */
    explicit MCPFileSystemServer(const std::string& root_path = ".", bool enable_write = true);
    
    /**
     * @brief Destructor
     */
    virtual ~MCPFileSystemServer() = default;
    
    /**
     * @brief Set the root path for file operations
     * @param root_path The root path
     */
    void setRootPath(const std::string& root_path);
    
    /**
     * @brief Get the current root path
     * @return The root path
     */
    const std::string& getRootPath() const;
    
    /**
     * @brief Enable or disable write operations
     * @param enable Whether to enable write operations
     */
    void setWriteEnabled(bool enable);
    
    /**
     * @brief Check if write operations are enabled
     * @return True if write operations are enabled
     */
    bool isWriteEnabled() const;
    
    /**
     * @brief Add a file system resource
     * @param path The path to add as a resource
     * @param name The display name for the resource
     * @param description The description of the resource
     */
    void addFileSystemResource(const std::string& path, const std::string& name = "", const std::string& description = "");
    
    /**
     * @brief Remove a file system resource
     * @param path The path to remove from resources
     */
    void removeFileSystemResource(const std::string& path);
    
    /**
     * @brief List all file system resources
     * @return Vector of resource paths
     */
    std::vector<std::string> listFileSystemResources() const;
    
    /**
     * @brief Validate if a path is within the allowed root path
     * @param path The path to validate
     * @return True if the path is valid and safe
     */
    bool validatePath(const std::string& path) const;
    
    /**
     * @brief Resolve a path relative to the root path
     * @param path The relative path
     * @return The resolved absolute path
     */
    std::string resolvePath(const std::string& path) const;

protected:
    /**
     * @brief Setup default file system tools
     */
    void setupDefaultTools() override;
    
    /**
     * @brief Setup default file system resources
     */
    void setupDefaultResources() override;

private:
    /**
     * @brief Handle file system specific tool calls
     * @param params The tool call parameters
     * @return The tool call result
     */
    ToolCallResult handleFileSystemToolCall(const ToolCallParams& params);
    
    std::string root_path_;           // Root path for file operations
    bool enable_write_;               // Whether write operations are enabled
    std::vector<std::string> fs_resources_;  // File system resources
};

} // namespace mcp

#endif // MCP_FILESYSTEM_SERVER_H
