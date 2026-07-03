#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MCP FileSystem Client Python Interface

这个模块提供了 Python 接口来调用 server_filesystem_ipc_example 服务器。
使用官方的 Python MCP SDK 客户端与 C++ MCP 文件系统服务器进行交互。

使用方法:
    from mcp_filesystem_client import MCPFileSystemClient
    
    import asyncio
    
    async def main():
        # 基本用法
        client = MCPFileSystemClient(server_path="./build/bin/server_filesystem_ipc_example")
        
        # 指定日志目录（日志会写入到文件）
        client = MCPFileSystemClient(
            server_path="./build/bin/server_filesystem_ipc_example",
            log_dir="./logs",  # 指定日志目录
            log_to_console=True  # 同时输出到控制台（可选）
        )
        
        async with client:
            # 初始化
            await client.initialize()
            
            # 获取工具列表
            tools = await client.list_tools()
            
            # 调用工具
            result = await client.call_tool("read_file", {"path": "README.md"})
        
        # 断开连接（自动处理）
    
    asyncio.run(main())

关于 TaskGroup 错误的说明:
    ============================================================
    为什么会出现 "unhandled errors in a TaskGroup" 错误？
    ============================================================
    
    这个错误通常出现在关闭连接时，原因如下：
    
    1. **Python MCP SDK 的内部实现**:
       - Python MCP SDK 使用 asyncio.TaskGroup (Python 3.11+) 来管理多个后台任务
       - 这些任务包括：读取消息、处理响应、监控连接状态等
    
    2. **关闭时的竞态条件**:
       - 当调用 disconnect() 时，后台任务可能还在运行
       - 如果子进程（服务器）已经退出，读取流会立即关闭
       - 后台读取任务会检测到 EOF 或连接关闭，可能抛出异常
       - TaskGroup 会捕获这些异常并报告为 "unhandled errors"
    
    3. **为什么可以安全忽略**:
       - 这是正常的关闭流程的一部分
       - 资源已经被正确清理
       - 不影响程序的功能和稳定性
       - 只是内部任务在关闭时的正常异常
    
    4. **处理方式**:
       - 代码已经自动识别并处理这类错误
       - 使用 DEBUG 级别记录（不会显示为警告）
       - 确保所有资源都被正确清理
    
    5. **常见场景**:
       - 服务器进程正常退出后关闭客户端
       - 网络连接中断时关闭
       - 程序正常退出时的清理过程
    
    这个错误不会影响程序功能，可以安全忽略。
"""

import asyncio
import sys
import logging
from pathlib import Path
from typing import Dict, List, Optional, Any
from datetime import datetime

try:
    from mcp import ClientSession
    from mcp.client.stdio import stdio_client, StdioServerParameters
    from mcp.types import Implementation, ClientCapabilities
except ImportError as e:
    logging.error(f"错误: 无法导入 MCP SDK: {e}")
    logging.error("请安装 MCP Python SDK: pip install mcp")
    sys.exit(1)


class MCPError(Exception):
    """MCP 协议错误异常类"""
    pass


class MCPFileSystemClient:
    """MCP 文件系统客户端 Python 接口（使用官方 Python MCP SDK）"""
    
    def __init__(self, 
                 server_path: str = "../server/build/bin/server_filesystem_ipc_example",
                 server_args: Optional[List[str]] = None,
                 cwd: Optional[str] = None,
                 env: Optional[Dict[str, str]] = None,
                 debug: bool = False,
                 log_dir: Optional[str] = None,
                 log_to_console: bool = True):
        """
        初始化 MCP 文件系统客户端
        
        Args:
            server_path: server_filesystem_ipc_example 可执行文件的路径
            server_args: 启动服务器的额外参数
            cwd: 服务器工作目录
            env: 环境变量
            debug: 是否启用调试模式
            log_dir: 日志输出目录，如果指定则会将日志写入到该目录的文件中
            log_to_console: 是否同时输出日志到控制台（默认True）
        """
        self.server_path = Path(server_path)
        if not self.server_path.exists():
            # 尝试从当前文件所在目录查找
            current_dir = Path(__file__).parent
            alternative_path = current_dir / server_path
            if alternative_path.exists():
                self.server_path = alternative_path
            else:
                raise FileNotFoundError(f"服务器可执行文件不存在: {server_path}")
        
        self.server_args = server_args or []
        self.cwd = cwd
        self.env = env or {}
        self.debug = debug
        self.log_dir = log_dir
        self.log_to_console = log_to_console
        
        # 初始化日志记录器
        self.logger = logging.getLogger(f"{__name__}.{self.__class__.__name__}")
        if debug:
            self.logger.setLevel(logging.DEBUG)
        else:
            self.logger.setLevel(logging.INFO)
        
        # 清除已有的处理器，避免重复添加
        self.logger.handlers.clear()
        
        # 创建格式化器
        formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
        
        # 如果指定了日志目录，添加文件处理器
        if self.log_dir:
            log_dir_path = Path(self.log_dir)
            # 如果目录不存在，创建它
            log_dir_path.mkdir(parents=True, exist_ok=True)
            
            # 生成带时间戳的日志文件名
            timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
            log_file = log_dir_path / f"mcp_filesystem_client_{timestamp}.log"
            
            # 创建文件处理器
            file_handler = logging.FileHandler(log_file, encoding='utf-8')
            file_handler.setLevel(logging.DEBUG if debug else logging.INFO)
            file_handler.setFormatter(formatter)
            self.logger.addHandler(file_handler)
            
            self._log(f"日志文件: {log_file}", "DEBUG")
        
        # 如果启用控制台输出，添加控制台处理器
        if self.log_to_console:
            console_handler = logging.StreamHandler(sys.stderr)
            console_handler.setLevel(logging.DEBUG if debug else logging.INFO)
            console_handler.setFormatter(formatter)
            self.logger.addHandler(console_handler)
        
        # 客户端会话和传输
        self._session: Optional[ClientSession] = None
        self._read_stream = None
        self._write_stream = None
        self._stdio_transport = None
        
        # 服务器信息
        self.server_info: Optional[Dict[str, Any]] = None
        self.protocol_version: Optional[str] = None
        
    def _log(self, message: str, level: str = "INFO"):
        """日志输出（保持向后兼容）"""
        log_level_map = {
            "DEBUG": logging.DEBUG,
            "INFO": logging.INFO,
            "WARNING": logging.WARNING,
            "ERROR": logging.ERROR,
            "CRITICAL": logging.CRITICAL
        }
        log_level = log_level_map.get(level.upper(), logging.INFO)
        self.logger.log(log_level, message)
    
    async def __aenter__(self):
        """异步上下文管理器入口"""
        await self.connect()
        return self
    
    async def __aexit__(self, exc_type, exc_val, exc_tb):
        """异步上下文管理器出口"""
        await self.disconnect()
    
    async def connect(self):
        """连接到 MCP 服务器"""
        try:
            # 创建 stdio 服务器参数
            server_params = StdioServerParameters(
                command=str(self.server_path.absolute()),
                args=self.server_args,
                cwd=self.cwd,
                env=self.env if self.env else None
            )
            
            self._log(f"启动服务器: {self.server_path.absolute()} {self.server_args}")
            
            # 创建 stdio 客户端传输
            self._stdio_transport = stdio_client(server_params)
            self._read_stream, self._write_stream = await self._stdio_transport.__aenter__()
            
            # 创建客户端信息
            client_info = Implementation(
                name="mcp-python-client",
                version="1.0.0"
            )
            
            # 创建客户端会话
            self._session = ClientSession(
                read_stream=self._read_stream,
                write_stream=self._write_stream,
                client_info=client_info
            )
            
            # 启动会话
            await self._session.__aenter__()
            
            self._log("连接成功")
            
        except Exception as e:
            self._log(f"连接失败: {e}", "ERROR")
            raise MCPError(f"连接失败: {e}")
    
    async def disconnect(self):
        """断开与服务器的连接"""
        try:
            # 先关闭会话，再关闭传输
            if self._session:
                try:
                    # 尝试优雅关闭会话
                    await self._session.__aexit__(None, None, None)
                except Exception as e:
                    # TaskGroup 错误通常是内部任务异常，可以安全忽略
                    error_str = str(e)
                    error_type = type(e).__name__
                    if "TaskGroup" in error_type or "unhandled errors" in error_str or "sub-exception" in error_str:
                        self._log(f"关闭会话时出现 TaskGroup 错误（可安全忽略）: {e}", "DEBUG")
                    else:
                        self._log(f"关闭会话时出错: {e}", "WARNING")
                finally:
                    self._session = None
            
            # 等待一小段时间，让会话完全关闭
            await asyncio.sleep(0.1)
            
            # 然后关闭传输
            if self._stdio_transport:
                try:
                    # 尝试优雅关闭传输
                    await self._stdio_transport.__aexit__(None, None, None)
                except Exception as e:
                    # TaskGroup 错误通常是内部任务异常，可以安全忽略
                    error_str = str(e)
                    error_type = type(e).__name__
                    if "TaskGroup" in error_type or "unhandled errors" in error_str or "sub-exception" in error_str:
                        self._log(f"关闭传输时出现 TaskGroup 错误（可安全忽略）: {e}", "DEBUG")
                    else:
                        self._log(f"关闭传输时出错: {e}", "WARNING")
                finally:
                    self._stdio_transport = None
            
            self._read_stream = None
            self._write_stream = None
            
            self._log("已断开连接")
            
        except Exception as e:
            # TaskGroup 错误通常是内部任务异常，可以安全忽略
            error_str = str(e)
            error_type = type(e).__name__
            if "TaskGroup" in error_type or "unhandled errors" in error_str or "sub-exception" in error_str:
                self._log(f"断开连接时出现 TaskGroup 错误（可安全忽略）: {e}", "DEBUG")
            else:
                self._log(f"断开连接时出错: {e}", "ERROR")
            # 确保清理资源
            self._session = None
            self._stdio_transport = None
            self._read_stream = None
            self._write_stream = None
    
    async def initialize(self, 
                        client_name: str = "mcp-python-client",
                        client_version: str = "1.0.0",
                        protocol_version: str = "2025-11-25") -> Dict[str, Any]:
        """
        初始化 MCP 协议
        
        Args:
            client_name: 客户端名称
            client_version: 客户端版本
            protocol_version: 协议版本
            
        Returns:
            初始化结果，包含服务器信息
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            # 调用 initialize 方法（它不接受参数，会自动使用 ClientSession 中的 client_info）
            result = await self._session.initialize()
            
            self.protocol_version = result.protocolVersion
            self.server_info = {
                "name": result.serverInfo.name,
                "version": result.serverInfo.version
            }
            
            self._log(f"初始化成功: {result.serverInfo.name} v{result.serverInfo.version}")
            
            return {
                "protocolVersion": result.protocolVersion,
                "serverInfo": {
                    "name": result.serverInfo.name,
                    "version": result.serverInfo.version
                },
                "capabilities": result.capabilities.model_dump() if hasattr(result.capabilities, 'model_dump') else {}
            }
            
        except Exception as e:
            self._log(f"初始化失败: {e}", "ERROR")
            raise MCPError(f"初始化失败: {e}")
    
    async def list_tools(self) -> List[Dict[str, Any]]:
        """
        获取可用工具列表
        
        Returns:
            工具列表
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            result = await self._session.list_tools()
            tools = []
            
            for tool in result.tools:
                tool_dict = {
                    "name": tool.name,
                    "description": tool.description or "",
                    "inputSchema": tool.inputSchema.model_dump() if hasattr(tool.inputSchema, 'model_dump') else {}
                }
                tools.append(tool_dict)
            
            self._log(f"获取到 {len(tools)} 个工具")
            return tools
            
        except Exception as e:
            self._log(f"获取工具列表失败: {e}", "ERROR")
            raise MCPError(f"获取工具列表失败: {e}")
    
    async def call_tool(self, name: str, arguments: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        调用工具
        
        Args:
            name: 工具名称
            arguments: 工具参数
            
        Returns:
            工具调用结果
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            # 确保 arguments 是一个字典，不是其他类型
            if arguments is None:
                arguments = {}
            elif not isinstance(arguments, dict):
                raise ValueError(f"arguments 必须是字典类型，但收到: {type(arguments)}")
            
            # 直接传递参数，而不是创建参数对象
            result = await self._session.call_tool(
                name=name,
                arguments=arguments
            )
            
            # 转换结果为字典
            result_dict = {
                "content": []
            }
            
            # 处理 content，确保它是列表类型
            if result.content:
                # 如果 content 不是列表，尝试转换
                if not isinstance(result.content, list):
                    self._log(f"警告: content 不是列表类型: {type(result.content)}", "WARNING")
                    # 如果是字符串，包装成文本内容项
                    if isinstance(result.content, str):
                        result_dict["content"] = [{
                            "type": "text",
                            "text": result.content
                        }]
                    else:
                        # 尝试提取可能的文本内容
                        result_dict["content"] = [{
                            "type": "text",
                            "text": str(result.content)
                        }]
                else:
                    # content 是列表，正常处理
                    for item in result.content:
                        if hasattr(item, 'text'):
                            result_dict["content"].append({
                                "type": "text",
                                "text": item.text
                            })
                        elif hasattr(item, 'image'):
                            result_dict["content"].append({
                                "type": "image",
                                "data": item.image.data,
                                "mimeType": item.image.mimeType
                            })
                        else:
                            # 如果 item 是字典或其他类型，尝试转换
                            if isinstance(item, dict):
                                # 如果已经是字典格式，直接使用
                                if "type" in item and "text" in item:
                                    result_dict["content"].append(item)
                                elif "text" in item:
                                    result_dict["content"].append({
                                        "type": "text",
                                        "text": item["text"]
                                    })
                                else:
                                    result_dict["content"].append({
                                        "type": "text",
                                        "text": str(item)
                                    })
                            else:
                                result_dict["content"].append({
                                    "type": "text",
                                    "text": str(item)
                                })
            
            if result.isError:
                result_dict["isError"] = True
                result_dict["errorMessage"] = str(result.content[0].text) if result.content and len(result.content) > 0 and hasattr(result.content[0], 'text') else "Unknown error"
            else:
                result_dict["isError"] = False
            
            self._log(f"工具调用成功: {name}")
            return result_dict
            
        except Exception as e:
            # 提供更详细的错误信息
            error_msg = str(e)
            if self.debug:
                self.logger.exception("工具调用异常详情")
            self._log(f"工具调用失败: {error_msg}", "ERROR")
            raise MCPError(f"工具调用失败: {error_msg}")
    
    async def list_resources(self) -> List[Dict[str, Any]]:
        """
        获取可用资源列表
        
        Returns:
            资源列表
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            result = await self._session.list_resources()
            resources = []
            
            for resource in result.resources:
                resource_dict = {
                    "uri": resource.uri,
                    "name": resource.name,
                    "description": resource.description or "",
                    "mimeType": resource.mimeType or ""
                }
                resources.append(resource_dict)
            
            self._log(f"获取到 {len(resources)} 个资源")
            return resources
            
        except Exception as e:
            self._log(f"获取资源列表失败: {e}", "ERROR")
            raise MCPError(f"获取资源列表失败: {e}")
    
    async def read_resource(self, uri: str) -> Dict[str, Any]:
        """
        读取资源
        
        Args:
            uri: 资源 URI
            
        Returns:
            资源内容
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            # 直接传递 uri 参数
            result = await self._session.read_resource(uri=uri)
            
            # 转换结果为字典
            result_dict = {
                "contents": []
            }
            
            if result.contents:
                for content in result.contents:
                    if hasattr(content, 'text'):
                        result_dict["contents"].append({
                            "uri": uri,
                            "mimeType": content.mimeType or "text/plain",
                            "text": content.text
                        })
                    elif hasattr(content, 'blob'):
                        result_dict["contents"].append({
                            "uri": uri,
                            "mimeType": content.mimeType or "application/octet-stream",
                            "blob": content.blob
                        })
            
            self._log(f"资源读取成功: {uri}")
            return result_dict
            
        except Exception as e:
            self._log(f"资源读取失败: {e}", "ERROR")
            raise MCPError(f"资源读取失败: {e}")
    
    async def list_prompts(self) -> List[Dict[str, Any]]:
        """
        获取可用提示列表
        
        Returns:
            提示列表
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            result = await self._session.list_prompts()
            prompts = []
            
            for prompt in result.prompts:
                prompt_dict = {
                    "name": prompt.name,
                    "description": prompt.description or "",
                    "arguments": []
                }
                
                if prompt.arguments:
                    for arg in prompt.arguments:
                        arg_dict = {
                            "name": arg.name,
                            "description": arg.description or "",
                            "required": arg.required or False
                        }
                        prompt_dict["arguments"].append(arg_dict)
                
                prompts.append(prompt_dict)
            
            self._log(f"获取到 {len(prompts)} 个提示")
            return prompts
            
        except Exception as e:
            self._log(f"获取提示列表失败: {e}", "ERROR")
            raise MCPError(f"获取提示列表失败: {e}")
    
    async def get_prompt(self, name: str, arguments: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        获取提示
        
        Args:
            name: 提示名称
            arguments: 提示参数
            
        Returns:
            提示内容
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            # 直接传递参数，而不是创建参数对象
            result = await self._session.get_prompt(
                name=name,
                arguments=arguments or {}
            )
            
            # 转换结果为字典
            result_dict = {
                "description": result.description or "",
                "messages": []
            }
            
            if result.messages:
                for msg in result.messages:
                    msg_dict = {}
                    if hasattr(msg, 'role'):
                        msg_dict["role"] = msg.role
                    if hasattr(msg, 'content'):
                        if isinstance(msg.content, str):
                            msg_dict["content"] = msg.content
                        elif hasattr(msg.content, 'text'):
                            msg_dict["content"] = msg.content.text
                        else:
                            msg_dict["content"] = str(msg.content)
                    result_dict["messages"].append(msg_dict)
            
            self._log(f"提示获取成功: {name}")
            return result_dict
            
        except Exception as e:
            self._log(f"提示获取失败: {e}", "ERROR")
            raise MCPError(f"提示获取失败: {e}")
    
    async def initialized(self):
        """
        发送初始化完成通知
        
        在调用 initialize() 成功后，应该调用此方法来通知服务器客户端已准备好接收请求。
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            # 尝试使用 Python MCP SDK 的 send_notification 方法
            if hasattr(self._session, 'send_notification'):
                await self._session.send_notification(
                    method="notifications/initialized",
                    params={}
                )
            elif hasattr(self._session, '_write_stream') and self._session._write_stream:
                # 使用底层方式发送通知（JSON-RPC 2.0 通知格式）
                import json
                notification = {
                    "jsonrpc": "2.0",
                    "method": "notifications/initialized",
                    "params": {}
                }
                message = json.dumps(notification) + "\n"
                if hasattr(self._session._write_stream, 'write'):
                    self._session._write_stream.write(message.encode('utf-8'))
                    await self._session._write_stream.drain()
                else:
                    self._log("警告: 无法发送 initialized 通知（写入流不可用）", "WARNING")
                    return
            else:
                # 如果 SDK 不支持，记录警告但不抛出异常
                self._log("警告: Python MCP SDK 可能不支持 send_notification，跳过 initialized 通知", "WARNING")
                return
            
            self._log("初始化完成通知已发送")
            
        except Exception as e:
            self._log(f"发送初始化完成通知失败: {e}", "WARNING")
            # 不抛出异常，因为这不是致命错误
    
    async def ping(self) -> Dict[str, Any]:
        """
        发送 ping 请求测试连接
        
        Returns:
            ping 响应结果
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        try:
            # 尝试使用 Python MCP SDK 的 ping 方法
            if hasattr(self._session, 'ping'):
                result = await self._session.ping()
            elif hasattr(self._session, 'request'):
                # 尝试使用通用的 request 方法
                result = await self._session.request(
                    method="ping",
                    params={}
                )
            else:
                # 如果 SDK 不支持 ping，尝试使用底层方式
                # 注意：这需要根据实际的 Python MCP SDK 实现来调整
                # 由于底层实现可能比较复杂，我们抛出异常提示用户
                raise MCPError("Python MCP SDK 不支持 ping 方法。请检查 SDK 版本或使用其他方式测试连接。")
            
            # 转换结果为字典
            if isinstance(result, dict):
                result_dict = result
            elif hasattr(result, 'model_dump'):
                result_dict = result.model_dump()
            elif hasattr(result, 'dict'):
                result_dict = result.dict()
            else:
                result_dict = {"result": str(result)}
            
            self._log("Ping 成功")
            return result_dict
            
        except MCPError:
            # 重新抛出 MCPError
            raise
        except Exception as e:
            self._log(f"Ping 失败: {e}", "ERROR")
            raise MCPError(f"Ping 失败: {e}")


async def main():
    """示例用法"""
    import sys
    
    # 确定服务器路径和日志目录
    script_dir = Path(__file__).parent
    log_dir = script_dir / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    
    # 生成带时间戳的日志文件名
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = log_dir / f"mcp_filesystem_client_{timestamp}.log"
    
    # 配置根日志记录器
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.INFO)
    
    # 清除已有的处理器，避免重复添加
    logger.handlers.clear()
    
    # 创建格式化器
    formatter = logging.Formatter('%(asctime)s - %(name)s - %(levelname)s - %(message)s')
    
    # 添加文件处理器（将日志写入文件）
    file_handler = logging.FileHandler(log_file, encoding='utf-8')
    file_handler.setLevel(logging.DEBUG)  # 文件记录所有级别的日志
    file_handler.setFormatter(formatter)
    logger.addHandler(file_handler)
    
    # 添加控制台处理器（同时输出到控制台）
    console_handler = logging.StreamHandler(sys.stdout)
    console_handler.setLevel(logging.INFO)
    console_formatter = logging.Formatter('%(message)s')
    console_handler.setFormatter(console_formatter)
    logger.addHandler(console_handler)
    
    logger.info(f"日志文件: {log_file}")
    
    # 确定服务器路径
    server_path = script_dir / "build" / "bin" / "server_filesystem_ipc_example"
    
    # if not server_path.exists():
    #     server_path = script_dir / "server_filesystem_ipc_example"
    # server_path = "../../dev_server/server/build/bin/server_filesystem_ipc_example" 
    # if not server_path.exists():
    #     logger.error(f"错误: 找不到服务器可执行文件")
    #     logger.error(f"请确保 server_filesystem_ipc_example 存在于以下位置之一:")
    #     logger.error(f"  - {script_dir / 'build' / 'bin' / 'server_filesystem_ipc_example'}")
    #     logger.error(f"  - {script_dir / 'server_filesystem_ipc_example'}")
    #     sys.exit(1)
    
    logger.info("🚀 MCP FileSystem Client Python Interface (使用官方 Python MCP SDK)")
    logger.info("=" * 70)
    
    try:
        # 使用异步上下文管理器，指定日志目录
        server_path = "../server/build/bin/mcp_filesystem_server_ipc_example"
        async with MCPFileSystemClient(
            server_path=str(server_path), 
            debug=True,
            log_dir=str(log_dir),
            log_to_console=True
        ) as client:
            # 初始化
            logger.info("\n📤 初始化 MCP 协议...")
            init_result = await client.initialize()
            logger.info(f"✅ 初始化成功")
            logger.debug(f"完整初始化结果: {init_result}")  # 完整初始化结果写入日志文件
            logger.info(f"  服务器: {init_result['serverInfo']['name']} v{init_result['serverInfo']['version']}")
            logger.info(f"  协议版本: {init_result['protocolVersion']}")
            
            # 发送初始化完成通知
            logger.info("\n📤 发送初始化完成通知...")
            try:
                await client.initialized()
                logger.info("✅ 初始化完成通知发送成功")
            except MCPError as e:
                logger.warning(f"⚠️  初始化完成通知发送失败: {e} (非致命错误)")
            
            # 等待服务器就绪
            await asyncio.sleep(1)
            
            # 获取工具列表
            logger.info("\n📤 获取工具列表...")
            tools = await client.list_tools()
            logger.info(f"✅ 获取到 {len(tools)} 个工具:")
            logger.debug(f"完整工具列表: {tools}")  # 完整工具列表写入日志文件
            for tool in tools:
                logger.info(f"   - {tool['name']}: {tool.get('description', 'N/A')}")
            
            # 获取资源列表
            logger.info("\n📤 获取资源列表...")
            resources = await client.list_resources()
            logger.info(f"✅ 获取到 {len(resources)} 个资源:")
            logger.debug(f"完整资源列表: {resources}")  # 完整资源列表写入日志文件
            for resource in resources:
                logger.info(f"   - {resource['name']} ({resource['uri']})")
            
            # 获取提示列表
            logger.info("\n📤 获取提示列表...")
            try:
                prompts = await client.list_prompts()
                logger.info(f"✅ 获取到 {len(prompts)} 个提示:")
                logger.debug(f"完整提示列表: {prompts}")  # 完整提示列表写入日志文件
                for prompt in prompts:
                    logger.info(f"   - {prompt['name']}: {prompt.get('description', 'N/A')}")
            except MCPError as e:
                logger.warning(f"⚠️  获取提示列表失败: {e}")
            
            # Ping 测试
            logger.info("\n📤 发送 Ping 测试...")
            try:
                ping_result = await client.ping()
                logger.info("✅ Ping 成功")
                logger.debug(f"完整 Ping 结果: {ping_result}")  # 完整结果写入日志文件
                logger.info(f"   响应: {str(ping_result)[:200]}...")  # 预览输出到控制台
            except MCPError as e:
                logger.warning(f"⚠️  Ping 失败: {e} (可能服务器不支持 ping)")
            
            # 调用工具示例
            logger.info("\n📤 调用工具: read_file...")
            try:
                result = await client.call_tool("read_file", {"path": "README.md"})
                if "content" in result:
                    content = result["content"]
                    if isinstance(content, list) and len(content) > 0:
                        text = content[0].get("text", "")
                        logger.info(f"✅ 文件读取成功 (长度: {len(text)} 字符)")
                        logger.debug(f"   完整内容:\n{text}")  # 完整内容写入日志文件（DEBUG级别）
                        logger.info(f"   内容预览: {text[:200]}...")  # 预览输出到控制台
                    else:
                        logger.info(f"✅ 工具调用成功")
                        logger.debug(f"完整结果: {result}")  # 完整结果写入日志文件
                else:
                    logger.info(f"✅ 工具调用成功: {result}")
                    logger.debug(f"完整结果: {result}")  # 完整结果写入日志文件
            except MCPError as e:
                logger.error(f"❌ 工具调用失败: {e}")
            
            # 列出目录
            logger.info("\n📤 调用工具: list_directory...")
            try:
                result = await client.call_tool("list_directory", {"path": "."})
                logger.info(f"✅ 目录列表获取成功")
                if "content" in result:
                    content = result["content"]
                    if isinstance(content, list) and len(content) > 0:
                        text = content[0].get("text", "")
                        logger.debug(f"   完整结果:\n{text}")  # 完整内容写入日志文件（DEBUG级别）
                        logger.info(f"   结果预览: {text[:200]}...")  # 预览输出到控制台
                    else:
                        logger.info(f"   结果: {result}")
                        logger.debug(f"完整结果: {result}")  # 完整结果写入日志文件
                else:
                    logger.info(f"   结果: {result}")
                    logger.debug(f"完整结果: {result}")  # 完整结果写入日志文件
            except MCPError as e:
                logger.error(f"❌ 工具调用失败: {e}")
            
            # 写入文件示例
            logger.info("\n📤 调用工具: write_file...")
            try:
                test_content = "这是一个测试文件\n由 Python MCP 客户端创建\n时间: " + datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                result = await client.call_tool("write_file", {
                    "path": "test_python_client.txt",
                    "contents": [{"text": test_content}]
                })
                if result.get("isError", False):
                    logger.error(f"❌ 工具调用失败: {result.get('errorMessage', 'Unknown error')}")
                else:
                    logger.info(f"✅ 文件写入成功: test_python_client.txt")
                    logger.debug(f"完整结果: {result}")  # 完整结果写入日志文件
            except MCPError as e:
                logger.warning(f"⚠️  工具调用失败: {e} (可能服务器不支持 write_file)")
            
            # 创建目录示例
            logger.info("\n📤 调用工具: create_directory...")
            try:
                result = await client.call_tool("create_directory", {
                    "path": "test_python_dir"
                })
                if result.get("isError", False):
                    logger.warning(f"⚠️  工具调用失败: {result.get('errorMessage', 'Unknown error')} (目录可能已存在)")
                else:
                    logger.info(f"✅ 目录创建成功: test_python_dir")
                    logger.debug(f"完整结果: {result}")  # 完整结果写入日志文件
            except MCPError as e:
                logger.warning(f"⚠️  工具调用失败: {e} (可能服务器不支持 create_directory)")
            
            # 再次读取文件验证写入
            logger.info("\n📤 调用工具: read_file (验证写入)...")
            try:
                result = await client.call_tool("read_file", {"path": "test_python_client.txt"})
                if result.get("isError", False):
                    logger.warning(f"⚠️  文件读取失败: {result.get('errorMessage', 'Unknown error')}")
                else:
                    if "content" in result:
                        content = result["content"]
                        if isinstance(content, list) and len(content) > 0:
                            text = content[0].get("text", "")
                            logger.info(f"✅ 文件读取成功 (长度: {len(text)} 字符)")
                            logger.debug(f"   完整内容:\n{text}")  # 完整内容写入日志文件
                            logger.info(f"   内容预览: {text[:200]}...")  # 预览输出到控制台
            except MCPError as e:
                logger.warning(f"⚠️  文件读取失败: {e}")
            
            logger.info("\n✅ 所有测试完成")
    
    except MCPError as e:
        logger.error(f"❌ MCP 错误: {e}")
        sys.exit(1)
    except Exception as e:
        logger.error(f"❌ 错误: {e}")
        logger.exception("异常详情:")
        sys.exit(1)


if __name__ == "__main__":
    asyncio.run(main())
