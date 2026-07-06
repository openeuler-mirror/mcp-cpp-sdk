/**
 * @file mcp_filesystem_server.cpp
 * @brief MCP FileSystem Server Implementation
 * 
 * This file provides the implementation of the MCP file system server.
 */

#include "mcp_filesystem_server.h"
#include <filesystem>
#include <algorithm>
#include <sstream>

namespace mcp {

MCPFileSystemServer::MCPFileSystemServer(const std::string& root_path, bool enable_write) 
    : MCPServer(), root_path_(root_path), enable_write_(enable_write) {
    
    // 设置服务器信息
    setServerInfo("MCP FileSystem Server", "1.0.0");
    
    // 设置服务器能力
    ServerCapabilities capabilities;
    capabilities.tools = json{{"listChanged", true}};
    capabilities.resources = json{{"subscribe", true}, {"listChanged", true}};
    capabilities.prompts = json{{"listChanged", true}};
    setCapabilities(capabilities);
    
    // 设置文件系统特定的工具和资源
    setupDefaultTools();
    setupDefaultResources();
    
    // 设置自定义工具调用处理器
    setToolCallHandler([this](const ToolCallParams& params) -> ToolCallResult {
        return handleFileSystemToolCall(params);
    });
    
    mcp_logger::getModuleLogger("mcp_filesystem_server")->info("MCPFileSystemServer created with root path: " + root_path_);
}

void MCPFileSystemServer::setRootPath(const std::string& root_path) {
    root_path_ = root_path;
    mcp_logger::getModuleLogger("mcp_filesystem_server")->info("Root path changed to: " + root_path_);
}

const std::string& MCPFileSystemServer::getRootPath() const {
    return root_path_;
}

void MCPFileSystemServer::setWriteEnabled(bool enable) {
    enable_write_ = enable;
    mcp_logger::getModuleLogger("mcp_filesystem_server")->info("Write operations " + std::string(enable ? "enabled" : "disabled"));
}

bool MCPFileSystemServer::isWriteEnabled() const {
    return enable_write_;
}

void MCPFileSystemServer::addFileSystemResource(const std::string& path, const std::string& name, const std::string& description) {
    // 解析路径为绝对路径进行验证
    std::string resolved_path = resolvePath(path);
    if (!validatePath(resolved_path)) {
        mcp_logger::getModuleLogger("mcp_filesystem_server")->error("Invalid path for resource: " + path);
        return;
    }
    
    Resource resource;
    resource.uri = "file://" + resolved_path;
    resource.name = name.empty() ? std::filesystem::path(resolved_path).filename().string() : name;
    resource.description = description.empty() ? "File system resource: " + resolved_path : description;
    
    // 检查路径是否存在以确定 MIME 类型
    if (std::filesystem::exists(resolved_path)) {
        resource.mimeType = std::filesystem::is_directory(resolved_path) ? "application/json" : "text/plain";
    } else {
        resource.mimeType = "text/plain"; // 默认类型
    }
    
    addResource(resource);
    
    // 添加到文件系统资源列表（使用原始路径）
    if (std::find(fs_resources_.begin(), fs_resources_.end(), path) == fs_resources_.end()) {
        fs_resources_.push_back(path);
    }
    
    mcp_logger::getModuleLogger("mcp_filesystem_server")->info("Added file system resource: " + resolved_path);
}

void MCPFileSystemServer::removeFileSystemResource(const std::string& path) {
    std::string uri = "file://" + path;
    removeResource(uri);
    
    // 从文件系统资源列表中移除
    fs_resources_.erase(std::remove(fs_resources_.begin(), fs_resources_.end(), path), fs_resources_.end());
    
    mcp_logger::getModuleLogger("mcp_filesystem_server")->info("Removed file system resource: " + path);
}

std::vector<std::string> MCPFileSystemServer::listFileSystemResources() const {
    return fs_resources_;
}

bool MCPFileSystemServer::validatePath(const std::string& path) const {
    try {
        std::filesystem::path fs_path(path);
        std::filesystem::path root_fs_path(root_path_);
        
        // 如果路径是相对的，先将其解析为相对于根路径
        if (!fs_path.is_absolute()) {
            fs_path = root_fs_path / fs_path;
        }
        
        // 规范化路径（如果路径存在则使用canonical，否则使用absolute）
        std::error_code ec;
        if (std::filesystem::exists(fs_path, ec)) {
            fs_path = std::filesystem::canonical(fs_path, ec);
            if (ec) {
                fs_path = std::filesystem::absolute(fs_path).lexically_normal();
            }
        } else {
            // 对于不存在的路径，使用absolute和lexically_normal
            fs_path = std::filesystem::absolute(fs_path).lexically_normal();
        }
        
        // 规范化根路径
        if (std::filesystem::exists(root_fs_path, ec)) {
            root_fs_path = std::filesystem::canonical(root_fs_path, ec);
            if (ec) {
                root_fs_path = std::filesystem::absolute(root_fs_path).lexically_normal();
            }
        } else {
            root_fs_path = std::filesystem::absolute(root_fs_path).lexically_normal();
        }
        
        // 检查路径是否在根路径内（使用字符串比较，确保路径格式一致）
        std::string path_str = fs_path.string();
        std::string root_str = root_fs_path.string();
        
        // 确保根路径以路径分隔符结尾，或者路径在根路径后紧跟路径分隔符
        if (path_str == root_str) {
            return true; // 路径就是根路径
        }
        
        // 检查路径是否以根路径开头
        if (path_str.find(root_str) == 0) {
            // 确保下一个字符是路径分隔符（防止路径劫持，如 /tmp 和 /tmp2）
            if (path_str.length() > root_str.length()) {
                char next_char = path_str[root_str.length()];
                return (next_char == '/' || next_char == '\\');
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("mcp_filesystem_server")->error("Path validation error: " + std::string(e.what()));
        return false;
    }
}

std::string MCPFileSystemServer::resolvePath(const std::string& path) const {
    try {
        std::filesystem::path fs_path(path);
        std::filesystem::path root_fs_path(root_path_);
        
        // 如果路径是绝对的，直接使用
        if (fs_path.is_absolute()) {
            std::error_code ec;
            if (std::filesystem::exists(fs_path, ec)) {
                return std::filesystem::canonical(fs_path, ec).string();
            } else {
                return std::filesystem::absolute(fs_path).lexically_normal().string();
            }
        }
        
        // 否则相对于根路径解析
        std::filesystem::path combined = root_fs_path / fs_path;
        std::error_code ec;
        if (std::filesystem::exists(combined, ec)) {
            return std::filesystem::canonical(combined, ec).string();
        } else {
            return std::filesystem::absolute(combined).lexically_normal().string();
        }
    } catch (const std::exception& e) {
        mcp_logger::getModuleLogger("mcp_filesystem_server")->error("Path resolution error: " + std::string(e.what()));
        return path; // 返回原始路径作为fallback
    }
}

void MCPFileSystemServer::setupDefaultTools() {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server");
    logger->info("Setting up file system tools");
    
    // 文件读取工具
    Tool readFileTool;
    readFileTool.name = "read_file";
    readFileTool.description = "Read the contents of a file";
    readFileTool.inputSchema = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Path to the file to read (relative to root path)"}}}
        }},
        {"required", {"path"}}
    };
    addTool(readFileTool);
    
    // 文件写入工具（如果启用写操作）
    if (enable_write_) {
        Tool writeFileTool;
        writeFileTool.name = "write_file";
        writeFileTool.description = "Write content to a file";
        writeFileTool.inputSchema = {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "Path to the file to write (relative to root path)"}}},
                {"content", {{"type", "string"}, {"description", "Content to write to the file"}}}
            }},
            {"required", {"path", "content"}}
        };
        addTool(writeFileTool);
    }
    
    // 目录列表工具
    Tool listDirectoryTool;
    listDirectoryTool.name = "list_directory";
    listDirectoryTool.description = "List the contents of a directory";
    listDirectoryTool.inputSchema = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Path to the directory to list (relative to root path, default: root)"}}}
        }},
        {"required", json::array()}
    };
    addTool(listDirectoryTool);
    
    // 目录创建工具（如果启用写操作）
    if (enable_write_) {
        Tool createDirectoryTool;
        createDirectoryTool.name = "create_directory";
        createDirectoryTool.description = "Create a new directory";
        createDirectoryTool.inputSchema = {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "Path of the directory to create (relative to root path)"}}}
            }},
            {"required", {"path"}}
        };
        addTool(createDirectoryTool);
    }
    
    // 文件删除工具（如果启用写操作）
    if (enable_write_) {
        Tool deleteFileTool;
        deleteFileTool.name = "delete_file";
        deleteFileTool.description = "Delete a file or directory";
        deleteFileTool.inputSchema = {
            {"type", "object"},
            {"properties", {
                {"path", {{"type", "string"}, {"description", "Path of the file or directory to delete (relative to root path)"}}}
            }},
            {"required", {"path"}}
        };
        addTool(deleteFileTool);
    }
    
    // 文件信息工具
    Tool fileInfoTool;
    fileInfoTool.name = "file_info";
    fileInfoTool.description = "Get information about a file or directory";
    fileInfoTool.inputSchema = {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}, {"description", "Path to the file or directory (relative to root path)"}}}
        }},
        {"required", {"path"}}
    };
    addTool(fileInfoTool);
    
    logger->info("File system tools setup completed");
}

void MCPFileSystemServer::setupDefaultResources() {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server");
    logger->info("Setting up file system resources");
    
    // 添加根目录作为资源
    addFileSystemResource(root_path_, "Root Directory", "The root directory for file system operations");
    
    // 如果根目录存在且有子目录，添加一些常见的子目录作为资源
    try {
        std::filesystem::path root_fs_path(root_path_);
        if (std::filesystem::exists(root_fs_path) && std::filesystem::is_directory(root_fs_path)) {
            for (const auto& entry : std::filesystem::directory_iterator(root_fs_path)) {
                if (entry.is_directory()) {
                    std::string relative_path = std::filesystem::relative(entry.path(), root_fs_path).string();
                    addFileSystemResource(relative_path, entry.path().filename().string(), 
                                        "Directory: " + entry.path().filename().string());
                }
            }
        }
    } catch (const std::exception& e) {
        logger->error("Error setting up default resources: " + std::string(e.what()));
    }
    
    logger->info("File system resources setup completed");
}

ToolCallResult MCPFileSystemServer::handleFileSystemToolCall(const ToolCallParams& params) {
    auto logger = mcp_logger::getModuleLogger("mcp_filesystem_server");
    logger->info("Handling file system tool call: " + params.name);
    
    ToolCallResult result; 
    try {
        if (params.name == "read_file") {
            std::string path = params.arguments.value("path", "");
            if (path.empty()) {
                result.isError = true;
                result.errorMessage = "Path parameter is required";
                return result;
            }
            
            std::string resolved_path = resolvePath(path);
            if (!validatePath(resolved_path)) {
                result.isError = true;
                result.errorMessage = "Invalid path: " + path;
                return result;
            }
            
            std::ifstream file(resolved_path);
            if (!file.is_open()) {
                result.isError = true;
                result.errorMessage = "Cannot open file: " + path;
                return result;
            }
            
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            
            result.content = json{
                {"content", content},
                {"path", path},
                {"resolved_path", resolved_path},
                {"size", content.size()}
            };
            return result;
        }
        else if (params.name == "write_file") {
            // 检查是否允许写入
            if (!enable_write_) {
                result.isError = true;
                result.errorMessage = "Write operations are disabled (read-only mode)";
                return result;
            }
            std::string path = params.arguments.value("path", "");
            std::string content = params.arguments.value("content", "");
            
            if (path.empty()) {
                result.isError = true;
                result.errorMessage = "Path parameter is required";
                return result;
            }
            
            std::string resolved_path = resolvePath(path);
            if (!validatePath(resolved_path)) {
                result.isError = true;
                result.errorMessage = "Invalid path: " + path;
                return result;
            }
            
            // 确保目录存在
            std::filesystem::path file_path(resolved_path);
            std::filesystem::create_directories(file_path.parent_path());
            
            std::ofstream file(resolved_path);
            if (!file.is_open()) {
                result.isError = true;
                result.errorMessage = "Cannot create file: " + path;
                return result;
            }
            
            file << content;
            file.close();
            
            result.content = json{
                {"message", "File written successfully"},
                {"path", path},
                {"resolved_path", resolved_path},
                {"size", content.size()}
            };
            return result;
        }
        else if (params.name == "list_directory") {
            std::string path = params.arguments.value("path", "");
            if (path.empty()) {
                path = "."; // 默认列出根目录
            }
            
            std::string resolved_path = resolvePath(path);
            if (!validatePath(resolved_path)) {
                result.isError = true;
                result.errorMessage = "Invalid path: " + path;
                return result;
            }
            
            std::filesystem::path dir_path(resolved_path);
            if (!std::filesystem::exists(dir_path)) {
                result.isError = true;
                result.errorMessage = "Directory does not exist: " + path;
                return result;
            }
            
            if (!std::filesystem::is_directory(dir_path)) {
                result.isError = true;
                result.errorMessage = "Path is not a directory: " + path;
                return result;
            }
            
            json files = json::array();
            for (const auto& entry : std::filesystem::directory_iterator(dir_path)) {
                json file_info;
                file_info["name"] = entry.path().filename().string();
                file_info["path"] = std::filesystem::relative(entry.path(), std::filesystem::path(root_path_)).string();
                file_info["is_directory"] = entry.is_directory();
                file_info["size"] = entry.is_regular_file() ? entry.file_size() : 0;
                
                // 添加更多文件信息
                auto status = entry.status();
                if (std::filesystem::is_regular_file(status)) {
                    file_info["type"] = "file";
                } else if (std::filesystem::is_directory(status)) {
                    file_info["type"] = "directory";
                } else {
                    file_info["type"] = "other";
                }
                
                files.push_back(file_info);
            }
            
            result.content = json{
                {"files", files},
                {"path", path},
                {"resolved_path", resolved_path},
                {"count", files.size()}
            };
            return result;
        }
        else if (params.name == "create_directory") {
            // 检查是否允许写入
            if (!enable_write_) {
                result.isError = true;
                result.errorMessage = "Write operations are disabled (read-only mode)";
                return result;
            }
            std::string path = params.arguments.value("path", "");
            if (path.empty()) {
                result.isError = true;
                result.errorMessage = "Path parameter is required";
                return result;
            }
            
            std::string resolved_path = resolvePath(path);
            if (!validatePath(resolved_path)) {
                result.isError = true;
                result.errorMessage = "Invalid path: " + path;
                return result;
            }
            
            std::filesystem::path dir_path(resolved_path);
            if (std::filesystem::exists(dir_path)) {
                result.isError = true;
                result.errorMessage = "Directory already exists: " + path;
                return result;
            }
            
            if (!std::filesystem::create_directories(dir_path)) {
                result.isError = true;
                result.errorMessage = "Failed to create directory: " + path;
                return result;
            }
            
            result.content = json{
                {"message", "Directory created successfully"},
                {"path", path},
                {"resolved_path", resolved_path}
            };
            return result;
        }
        else if (params.name == "delete_file") {
            // 检查是否允许写入
            if (!enable_write_) {
                result.isError = true;
                result.errorMessage = "Write operations are disabled (read-only mode)";
                return result;
            }
            std::string path = params.arguments.value("path", "");
            if (path.empty()) {
                result.isError = true;
                result.errorMessage = "Path parameter is required";
                return result;
            }
            
            std::string resolved_path = resolvePath(path);
            if (!validatePath(resolved_path)) {
                result.isError = true;
                result.errorMessage = "Invalid path: " + path;
                return result;
            }
            
            std::filesystem::path file_path(resolved_path);
            if (!std::filesystem::exists(file_path)) {
                result.isError = true;
                result.errorMessage = "File or directory does not exist: " + path;
                return result;
            }
            
            std::error_code ec;
            bool removed = std::filesystem::remove_all(file_path, ec);
            if (!removed || ec) {
                result.isError = true;
                result.errorMessage = "Failed to delete: " + path + " (" + ec.message() + ")";
                return result;
            }
            
            result.content = json{
                {"message", "File or directory deleted successfully"},
                {"path", path},
                {"resolved_path", resolved_path}
            };
            return result;
        }
        else if (params.name == "file_info") {
            std::string path = params.arguments.value("path", "");
            if (path.empty()) {
                result.isError = true;
                result.errorMessage = "Path parameter is required";
                return result;
            }
            
            std::string resolved_path = resolvePath(path);
            if (!validatePath(resolved_path)) {
                result.isError = true;
                result.errorMessage = "Invalid path: " + path;
                return result;
            }
            
            std::filesystem::path file_path(resolved_path);
            if (!std::filesystem::exists(file_path)) {
                result.isError = true;
                result.errorMessage = "File or directory does not exist: " + path;
                return result;
            }
            
            auto status = std::filesystem::status(file_path);
            json file_info;
            file_info["path"] = path;
            file_info["resolved_path"] = resolved_path;
            file_info["name"] = file_path.filename().string();
            file_info["exists"] = true;
            file_info["is_directory"] = std::filesystem::is_directory(status);
            file_info["is_regular_file"] = std::filesystem::is_regular_file(status);
            file_info["is_symlink"] = std::filesystem::is_symlink(status);
            
            if (std::filesystem::is_regular_file(status)) {
                file_info["size"] = std::filesystem::file_size(file_path);
                file_info["type"] = "file";
            } else if (std::filesystem::is_directory(status)) {
                file_info["type"] = "directory";
                // 计算目录中的文件数量
                int file_count = 0;
                int dir_count = 0;
                for (const auto& entry : std::filesystem::directory_iterator(file_path)) {
                    if (entry.is_directory()) {
                        dir_count++;
                    } else {
                        file_count++;
                    }
                }
                file_info["file_count"] = file_count;
                file_info["directory_count"] = dir_count;
            } else {
                file_info["type"] = "other";
            }
            
            result.content = file_info;
            return result;
        }
        else {
            result.isError = true;
            result.errorMessage = "Unknown tool: " + params.name;
            return result;
        }
    } catch (const std::exception& e) {
        result.isError = true;
        result.errorMessage = "Tool execution error: " + std::string(e.what());
        logger->error("Tool call failed: " + result.errorMessage);
        return result;
    }
}

} // namespace mcp
