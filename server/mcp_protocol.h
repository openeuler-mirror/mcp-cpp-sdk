/**
 * @file mcp_protocol.h
 * @brief Unified MCP Protocol Header
 * 
 * This file provides a unified interface to all MCP protocol modules.
 * It includes the core protocol definitions, client, and server implementations.
 * Implements the 2024-11-05 protocol specification.
 */

#ifndef MCP_PROTOCOL_H
#define MCP_PROTOCOL_H

// Include all MCP modules
#include "mcp_protocol_core.h"  // Core protocol definitions
#include "mcp_client.h"         // Client implementation
#include "mcp_server.h"         // Server implementation

#endif // MCP_PROTOCOL_H
