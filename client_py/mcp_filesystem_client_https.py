#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MCP FileSystem Client Python Interface (HTTPS)

这个模块提供了 Python 接口来通过 HTTPS 连接 MCP 文件系统服务器。
使用 HTTP/HTTPS 请求与 C++ MCP 文件系统服务器进行交互。

使用方法:
    from mcp_filesystem_client_https import MCPFileSystemClientHTTPS
    
    import asyncio
    
    async def main():
        # 基本用法
        client = MCPFileSystemClientHTTPS(
            host="localhost",
            port=8443,
            endpoint="/mcp"
        )
        
        # 指定证书文件（可选，用于自签名证书）
        client = MCPFileSystemClientHTTPS(
            host="localhost",
            port=8443,
            endpoint="/mcp",
            ca_file="./certs/ca.crt",  # CA 证书文件（可选）
            verify_ssl=True  # 是否验证 SSL 证书
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
"""

import asyncio
import sys
import logging
import json
import ssl
from pathlib import Path
from typing import Dict, List, Optional, Any
from datetime import datetime
from urllib.parse import urljoin

try:
    import aiohttp
    from aiohttp import ClientSession, ClientTimeout, TCPConnector
except ImportError as e:
    logging.error(f"错误: 无法导入 aiohttp: {e}")
    logging.error("请安装 aiohttp: pip install aiohttp")
    sys.exit(1)


class MCPError(Exception):
    """MCP 协议错误异常类"""
    pass


class MCPFileSystemClientHTTPS:
    """MCP 文件系统客户端 Python 接口（HTTPS 传输）"""
    
    def __init__(self, 
                 host: str = "localhost",
                 port: int = 10081,
                 endpoint: str = "/mcp",
                 ca_file: Optional[str] = None,
                 verify_ssl: bool = True,
                 timeout: int = 30,
                 debug: bool = False,
                 log_dir: Optional[str] = None,
                 log_to_console: bool = True,
                 disable_session_validation: bool = False):
        """
        初始化 MCP 文件系统客户端（HTTPS）
        
        Args:
            host: 服务器主机地址
            port: 服务器端口
            endpoint: HTTPS 端点路径
            ca_file: CA 证书文件路径（用于验证自签名证书）
            verify_ssl: 是否验证 SSL 证书（默认 True）
            timeout: 请求超时时间（秒）
            debug: 是否启用调试模式
            log_dir: 日志输出目录，如果指定则会将日志写入到该目录的文件中
            log_to_console: 是否同时输出日志到控制台（默认True）
            disable_session_validation: 是否禁用 session ID 验证（仅用于测试）
        """
        self.host = host
        self.port = port
        self.endpoint = endpoint
        self.ca_file = ca_file
        self.verify_ssl = verify_ssl
        self.timeout = timeout
        self.debug = debug
        self.log_dir = log_dir
        self.log_to_console = log_to_console
        self.disable_session_validation = disable_session_validation
        
        # 构建基础 URL
        self.base_url = f"https://{host}:{port}{endpoint}"
        
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
            log_file = log_dir_path / f"mcp_filesystem_client_https_{timestamp}.log"
            
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
        
        # 客户端会话
        self._session: Optional[ClientSession] = None
        self._session_id: Optional[str] = None
        self._request_id_counter = 0
        
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
    
    def _generate_request_id(self) -> str:
        """生成请求 ID"""
        self._request_id_counter += 1
        return str(self._request_id_counter)
    
    async def _make_request(self, method: str, params: Optional[Dict[str, Any]] = None) -> Dict[str, Any]:
        """
        发送 JSON-RPC 2.0 请求
        
        Args:
            method: MCP 方法名
            params: 方法参数
            
        Returns:
            响应结果
        """
        if not self._session:
            raise MCPError("客户端未连接")
        
        request_id = self._generate_request_id()
        request_data = {
            "jsonrpc": "2.0",
            "id": request_id,
            "method": method
        }
        
        if params:
            request_data["params"] = params
        
        # 如果禁用了 session 验证，在 params 中添加标记
        if self.disable_session_validation and params:
            params["_disable_session_validation"] = True
        
        # 添加 session_id 到请求头（如果已获取）
        headers = {
            "Content-Type": "application/json; charset=utf-8",
            "Accept": "application/json",
            "User-Agent": "MCP-FileSystem-Client-HTTPS/1.0"
        }
        
        if self._session_id:
            headers["X-Session-Id"] = self._session_id
        
        try:
            self._log(f"发送请求: {method} (ID: {request_id})", "DEBUG")
            self._log(f"请求数据: {json.dumps(request_data, indent=2)}", "DEBUG")
            
            async with self._session.post(
                self.base_url,
                json=request_data,
                headers=headers,
                ssl=self._ssl_context
            ) as response:
                # 检查响应头中是否有 session_id
                if "X-Session-Id" in response.headers:
                    self._session_id = response.headers["X-Session-Id"]
                    self._log(f"收到 Session ID: {self._session_id}", "DEBUG")
                
                response_text = await response.text()
                self._log(f"响应状态: {response.status}", "DEBUG")
                self._log(f"响应数据: {response_text}", "DEBUG")
                
                if response.status != 200:
                    raise MCPError(f"HTTP 错误 {response.status}: {response_text}")
                
                response_data = json.loads(response_text)
                
                # 检查 JSON-RPC 错误
                if "error" in response_data:
                    error = response_data["error"]
                    error_code = error.get("code", -1)
                    error_message = error.get("message", "Unknown error")
                    raise MCPError(f"JSON-RPC 错误 [{error_code}]: {error_message}")
                
                # 返回结果
                return response_data.get("result", {})
                
        except aiohttp.ClientError as e:
            self._log(f"HTTP 请求失败: {e}", "ERROR")
            raise MCPError(f"HTTP 请求失败: {e}")
        except json.JSONDecodeError as e:
            self._log(f"JSON 解析失败: {e}", "ERROR")
            raise MCPError(f"JSON 解析失败: {e}")
        except Exception as e:
            self._log(f"请求失败: {e}", "ERROR")
            raise MCPError(f"请求失败: {e}")
    
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
            # 创建 SSL 上下文
            self._ssl_context = ssl.create_default_context()
            
            if self.ca_file:
                # 如果提供了 CA 证书文件，使用它来验证
                self._ssl_context.load_verify_locations(self.ca_file)
                self._ssl_context.check_hostname = True
                self._ssl_context.verify_mode = ssl.CERT_REQUIRED
                self._log(f"使用 CA 证书文件: {self.ca_file}")
            elif not self.verify_ssl:
                # 如果禁用 SSL 验证（用于自签名证书测试）
                self._ssl_context.check_hostname = False
                self._ssl_context.verify_mode = ssl.CERT_NONE
                self._log("SSL 证书验证已禁用（仅用于测试）", "WARNING")
            else:
                # 使用系统默认的 CA 证书
                self._ssl_context.check_hostname = True
                self._ssl_context.verify_mode = ssl.CERT_REQUIRED
            
            # 创建 aiohttp 客户端会话
            timeout = ClientTimeout(total=self.timeout)
            connector = TCPConnector(ssl=self._ssl_context)
            
            self._session = ClientSession(
                timeout=timeout,
                connector=connector
            )
            
            self._log(f"连接到服务器: {self.base_url}")
            
        except Exception as e:
            self._log(f"连接失败: {e}", "ERROR")
            raise MCPError(f"连接失败: {e}")
    
    async def disconnect(self):
        """断开与服务器的连接"""
        try:
            if self._session:
                await self._session.close()
                self._session = None
            
            self._session_id = None
            self._log("已断开连接")
            
        except Exception as e:
            self._log(f"断开连接时出错: {e}", "WARNING")
            # 确保清理资源
            self._session = None
            self._session_id = None
    
    async def initialize(self, 
                        client_name: str = "mcp-python-client-https",
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
            params = {
                "protocolVersion": protocol_version,
                "capabilities": {},
                "clientInfo": {
                    "name": client_name,
                    "version": client_version
                }
            }
            
            result = await self._make_request("initialize", params)
            
            self.protocol_version = result.get("protocolVersion", protocol_version)
            server_info = result.get("serverInfo", {})
            self.server_info = {
                "name": server_info.get("name", "Unknown"),
                "version": server_info.get("version", "Unknown")
            }
            
            self._log(f"初始化成功: {self.server_info['name']} v{self.server_info['version']}")
            
            return {
                "protocolVersion": self.protocol_version,
                "serverInfo": self.server_info,
                "capabilities": result.get("capabilities", {})
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
            result = await self._make_request("tools/list")
            tools = result.get("tools", [])
            
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
            # 确保 arguments 是一个字典
            if arguments is None:
                arguments = {}
            elif not isinstance(arguments, dict):
                raise ValueError(f"arguments 必须是字典类型，但收到: {type(arguments)}")
            
            params = {
                "name": name,
                "arguments": arguments
            }
            
            result = await self._make_request("tools/call", params)
            
            # 转换结果为统一格式
            result_dict = {
                "content": []
            }
            
            # 处理 content
            if "content" in result:
                content = result["content"]
                if isinstance(content, list):
                    result_dict["content"] = content
                elif isinstance(content, str):
                    result_dict["content"] = [{
                        "type": "text",
                        "text": content
                    }]
                else:
                    result_dict["content"] = [{
                        "type": "text",
                        "text": str(content)
                    }]
            
            if "isError" in result:
                result_dict["isError"] = result["isError"]
                if result["isError"] and "content" in result and len(result["content"]) > 0:
                    if isinstance(result["content"][0], dict) and "text" in result["content"][0]:
                        result_dict["errorMessage"] = result["content"][0]["text"]
                    else:
                        result_dict["errorMessage"] = str(result["content"][0])
            else:
                result_dict["isError"] = False
            
            self._log(f"工具调用成功: {name}")
            return result_dict
            
        except Exception as e:
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
            result = await self._make_request("resources/list")
            resources = result.get("resources", [])
            
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
            params = {
                "uri": uri
            }
            
            result = await self._make_request("resources/read", params)
            
            # 转换结果为字典
            result_dict = {
                "contents": result.get("contents", [])
            }
            
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
            result = await self._make_request("prompts/list")
            prompts = result.get("prompts", [])
            
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
            params = {
                "name": name
            }
            
            if arguments:
                params["arguments"] = arguments
            
            result = await self._make_request("prompts/get", params)
            
            # 转换结果为字典
            result_dict = {
                "description": result.get("description", ""),
                "messages": result.get("messages", [])
            }
            
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
            # 发送 JSON-RPC 2.0 通知（没有 id 字段）
            notification = {
                "jsonrpc": "2.0",
                "method": "notifications/initialized",
                "params": {}
            }
            
            headers = {
                "Content-Type": "application/json; charset=utf-8",
                "Accept": "application/json",
                "User-Agent": "MCP-FileSystem-Client-HTTPS/1.0"
            }
            
            if self._session_id:
                headers["X-Session-Id"] = self._session_id
            
            async with self._session.post(
                self.base_url,
                json=notification,
                headers=headers,
                ssl=self._ssl_context
            ) as response:
                if response.status == 200:
                    self._log("初始化完成通知已发送")
                else:
                    self._log(f"发送初始化完成通知失败: HTTP {response.status}", "WARNING")
            
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
            result = await self._make_request("ping")
            
            # 转换结果为字典
            if isinstance(result, dict):
                result_dict = result
            else:
                result_dict = {"result": str(result)}
            
            self._log("Ping 成功")
            return result_dict
            
        except Exception as e:
            self._log(f"Ping 失败: {e}", "ERROR")
            raise MCPError(f"Ping 失败: {e}")


async def main():
    """示例用法"""
    import sys
    
    # 确定日志目录
    script_dir = Path(__file__).parent
    log_dir = script_dir / "logs"
    log_dir.mkdir(parents=True, exist_ok=True)
    
    # 生成带时间戳的日志文件名
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    log_file = log_dir / f"mcp_filesystem_client_https_{timestamp}.log"
    
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
    
    logger.info("🚀 MCP FileSystem Client Python Interface (HTTPS)")
    logger.info("=" * 70)
    
    try:
        # 使用异步上下文管理器
        async with MCPFileSystemClientHTTPS(
            host="localhost",
            port=10081,
            endpoint="/mcp",
            verify_ssl=False,  # 对于自签名证书，设置为 False
            # ca_file="./certs/ca.crt",  # 如果提供了 CA 证书文件，可以使用它
            debug=True,
            log_dir=str(log_dir),
            log_to_console=True,
            disable_session_validation=False  # 如果需要禁用 session 验证，设置为 True
        ) as client:
            # 初始化
            logger.info("\n📤 初始化 MCP 协议...")
            init_result = await client.initialize()
            logger.info(f"✅ 初始化成功")
            logger.debug(f"完整初始化结果: {init_result}")
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
            logger.debug(f"完整工具列表: {tools}")
            for tool in tools:
                logger.info(f"   - {tool['name']}: {tool.get('description', 'N/A')}")
            
            # 获取资源列表
            logger.info("\n📤 获取资源列表...")
            resources = await client.list_resources()
            logger.info(f"✅ 获取到 {len(resources)} 个资源:")
            logger.debug(f"完整资源列表: {resources}")
            for resource in resources:
                logger.info(f"   - {resource['name']} ({resource['uri']})")
            
            # 获取提示列表
            logger.info("\n📤 获取提示列表...")
            try:
                prompts = await client.list_prompts()
                logger.info(f"✅ 获取到 {len(prompts)} 个提示:")
                logger.debug(f"完整提示列表: {prompts}")
                for prompt in prompts:
                    logger.info(f"   - {prompt['name']}: {prompt.get('description', 'N/A')}")
            except MCPError as e:
                logger.warning(f"⚠️  获取提示列表失败: {e}")
            
            # Ping 测试
            logger.info("\n📤 发送 Ping 测试...")
            try:
                ping_result = await client.ping()
                logger.info("✅ Ping 成功")
                logger.debug(f"完整 Ping 结果: {ping_result}")
                logger.info(f"   响应: {str(ping_result)[:200]}...")
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
                        logger.debug(f"   完整内容:\n{text}")
                        logger.info(f"   内容预览: {text[:200]}...")
                    else:
                        logger.info(f"✅ 工具调用成功")
                        logger.debug(f"完整结果: {result}")
                else:
                    logger.info(f"✅ 工具调用成功: {result}")
                    logger.debug(f"完整结果: {result}")
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
                        logger.debug(f"   完整结果:\n{text}")
                        logger.info(f"   结果预览: {text[:200]}...")
                    else:
                        logger.info(f"   结果: {result}")
                        logger.debug(f"完整结果: {result}")
                else:
                    logger.info(f"   结果: {result}")
                    logger.debug(f"完整结果: {result}")
            except MCPError as e:
                logger.error(f"❌ 工具调用失败: {e}")
            
            # 写入文件示例
            logger.info("\n📤 调用工具: write_file...")
            try:
                test_content = "这是一个测试文件\n由 Python MCP HTTPS 客户端创建\n时间: " + datetime.now().strftime("%Y-%m-%d %H:%M:%S")
                result = await client.call_tool("write_file", {
                    "path": "test_python_client_https.txt",
                    "contents": [{"text": test_content}]
                })
                if result.get("isError", False):
                    logger.error(f"❌ 工具调用失败: {result.get('errorMessage', 'Unknown error')}")
                else:
                    logger.info(f"✅ 文件写入成功: test_python_client_https.txt")
                    logger.debug(f"完整结果: {result}")
            except MCPError as e:
                logger.warning(f"⚠️  工具调用失败: {e} (可能服务器不支持 write_file)")
            
            # 创建目录示例
            logger.info("\n📤 调用工具: create_directory...")
            try:
                result = await client.call_tool("create_directory", {
                    "path": "test_python_dir_https"
                })
                if result.get("isError", False):
                    logger.warning(f"⚠️  工具调用失败: {result.get('errorMessage', 'Unknown error')} (目录可能已存在)")
                else:
                    logger.info(f"✅ 目录创建成功: test_python_dir_https")
                    logger.debug(f"完整结果: {result}")
            except MCPError as e:
                logger.warning(f"⚠️  工具调用失败: {e} (可能服务器不支持 create_directory)")
            
            # 再次读取文件验证写入
            logger.info("\n📤 调用工具: read_file (验证写入)...")
            try:
                result = await client.call_tool("read_file", {"path": "test_python_client_https.txt"})
                if result.get("isError", False):
                    logger.warning(f"⚠️  文件读取失败: {result.get('errorMessage', 'Unknown error')}")
                else:
                    if "content" in result:
                        content = result["content"]
                        if isinstance(content, list) and len(content) > 0:
                            text = content[0].get("text", "")
                            logger.info(f"✅ 文件读取成功 (长度: {len(text)} 字符)")
                            logger.debug(f"   完整内容:\n{text}")
                            logger.info(f"   内容预览: {text[:200]}...")
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
