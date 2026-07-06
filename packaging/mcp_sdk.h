/**
 * @file mcp_sdk.h
 * @brief MCP SDK Unified Header
 * 
 * This is the main header file for the MCP SDK that provides
 * unified access to both client and server functionality.
 */

#ifndef MCP_SDK_H
#define MCP_SDK_H

// Protocol headers
#include <mcp_sdk/protocol/mcp_protocol_core.h>
#include <mcp_sdk/protocol/mcp_message.h>

// Transport headers
#include <mcp_sdk/transport/transport.h>

// IPC headers
#include <mcp_sdk/ipc/pipe_communication.h>
#include <mcp_sdk/ipc/mcp_protocol_messages.h>

// Network headers
#include <mcp_sdk/net/netio.h>

// Logging headers
#include <mcp_sdk/log/logger.h>

// Common headers
#include <mcp_sdk/common/httplib.h>
#include <mcp_sdk/common/json.hpp>
#include <mcp_sdk/common/base64.hpp>

// Client headers
#include <mcp_sdk/client/mcp_client.h>

// Server headers
#include <mcp_sdk/server/mcp_server.h>
#include <mcp_sdk/server/mcp_filesystem_server.h>
#include <mcp_sdk/server/mcp_protocol.h>

#endif // MCP_SDK_H
