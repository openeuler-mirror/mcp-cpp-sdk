/**
 * @file mcp_protocol_gtest.cpp
 * @brief Comprehensive GTest suite for MCP Protocol Core and Message interfaces
 */

#include <gtest/gtest.h>
#include "mcp_protocol_core.h"
#include "mcp_message.h"
#include "../../log/include/logger.h"

using namespace mcp;

// ============================================================================
// Test Fixture for common setup
// ============================================================================
class MCPProtocolTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize logger if needed
        auto logger = mcp_logger::getModuleLogger("mcp_protocol_gtest");
        logger->info("Setting up test fixture");
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// ============================================================================
// Tests for mcp_message.h - error_code enum
// ============================================================================
TEST_F(MCPProtocolTest, ErrorCodeValues) {
    EXPECT_EQ(static_cast<int>(error_code::parse_error), -32700);
    EXPECT_EQ(static_cast<int>(error_code::invalid_request), -32600);
    EXPECT_EQ(static_cast<int>(error_code::method_not_found), -32601);
    EXPECT_EQ(static_cast<int>(error_code::invalid_params), -32602);
    EXPECT_EQ(static_cast<int>(error_code::internal_error), -32603);
    EXPECT_EQ(static_cast<int>(error_code::server_error_start), -32000);
    EXPECT_EQ(static_cast<int>(error_code::server_error_end), -32099);
}

// ============================================================================
// Tests for mcp_message.h - mcp_exception
// ============================================================================
TEST_F(MCPProtocolTest, McpException) {
    mcp_exception ex(error_code::invalid_request, "Test error message");
    EXPECT_EQ(ex.code(), error_code::invalid_request);
    EXPECT_STREQ(ex.what(), "Test error message");
}

// ============================================================================
// Tests for mcp_message.h - request structure
// ============================================================================
TEST_F(MCPProtocolTest, RequestCreate) {
    auto req = request::create("test/method", json{{"key", "value"}});
    EXPECT_EQ(req.jsonrpc, "2.0");
    EXPECT_EQ(req.method, "test/method");
    EXPECT_FALSE(req.id.is_null());
    EXPECT_EQ(req.params["key"], "value");
}

TEST_F(MCPProtocolTest, RequestCreateWithId) {
    auto req = request::createWithId("custom-id", "test/method", json{{"key", "value"}});
    EXPECT_EQ(req.jsonrpc, "2.0");
    EXPECT_EQ(req.method, "test/method");
    EXPECT_EQ(req.id.get<std::string>(), "custom-id");
    EXPECT_EQ(req.params["key"], "value");
}

TEST_F(MCPProtocolTest, RequestCreateNotification) {
    auto req = request::createNotification("notifications/test", json{{"key", "value"}});
    EXPECT_EQ(req.jsonrpc, "2.0");
    EXPECT_EQ(req.method, "notifications/test");
    EXPECT_TRUE(req.id.is_null());
    EXPECT_TRUE(req.isNotification());
    EXPECT_EQ(req.params["key"], "value");
}

TEST_F(MCPProtocolTest, RequestIsNotification) {
    auto req1 = request::create("test/method");
    EXPECT_FALSE(req1.isNotification());
    
    auto req2 = request::createNotification("notifications/test");
    EXPECT_TRUE(req2.isNotification());
}

TEST_F(MCPProtocolTest, RequestToJson) {
    auto req = request::create("test/method", json{{"key", "value"}});
    auto j = req.toJson();
    
    EXPECT_EQ(j["jsonrpc"], "2.0");
    EXPECT_EQ(j["method"], "test/method");
    EXPECT_TRUE(j.contains("id"));
    EXPECT_EQ(j["params"]["key"], "value");
}

TEST_F(MCPProtocolTest, RequestToJsonNotification) {
    auto req = request::createNotification("notifications/test", json{{"key", "value"}});
    auto j = req.toJson();
    
    EXPECT_EQ(j["jsonrpc"], "2.0");
    EXPECT_EQ(j["method"], "notifications/test");
    EXPECT_FALSE(j.contains("id"));
    EXPECT_EQ(j["params"]["key"], "value");
}

TEST_F(MCPProtocolTest, RequestFromJson) {
    json j = {
        {"jsonrpc", "2.0"},
        {"id", 123},
        {"method", "test/method"},
        {"params", {{"key", "value"}}}
    };
    
    auto req = request::fromJson(j);
    EXPECT_EQ(req.jsonrpc, "2.0");
    EXPECT_EQ(req.id, 123);
    EXPECT_EQ(req.method, "test/method");
    EXPECT_EQ(req.params["key"], "value");
}

TEST_F(MCPProtocolTest, RequestFromJsonWithoutParams) {
    json j = {
        {"jsonrpc", "2.0"},
        {"id", 456},
        {"method", "test/method"}
    };
    
    auto req = request::fromJson(j);
    EXPECT_EQ(req.jsonrpc, "2.0");
    EXPECT_EQ(req.id, 456);
    EXPECT_EQ(req.method, "test/method");
    EXPECT_TRUE(req.params.empty());
}

// ============================================================================
// Tests for mcp_message.h - response structure
// ============================================================================
TEST_F(MCPProtocolTest, ResponseCreateSuccess) {
    json req_id = 123;
    json result_data = {{"key", "value"}};
    auto res = response::createSuccess(req_id, result_data);
    
    EXPECT_EQ(res.jsonrpc, "2.0");
    EXPECT_EQ(res.id, 123);
    EXPECT_FALSE(res.isError());
    EXPECT_EQ(res.result["key"], "value");
}

TEST_F(MCPProtocolTest, ResponseCreateError) {
    json req_id = 456;
    auto res = response::createError(req_id, error_code::invalid_params, "Invalid parameters");
    
    EXPECT_EQ(res.jsonrpc, "2.0");
    EXPECT_EQ(res.id, 456);
    EXPECT_TRUE(res.isError());
    EXPECT_EQ(res.error["code"], static_cast<int>(error_code::invalid_params));
    EXPECT_EQ(res.error["message"], "Invalid parameters");
}

TEST_F(MCPProtocolTest, ResponseCreateErrorWithData) {
    json req_id = 789;
    json error_data = {{"details", "More info"}};
    auto res = response::createError(req_id, error_code::method_not_found, "Method not found", error_data);
    
    EXPECT_EQ(res.jsonrpc, "2.0");
    EXPECT_EQ(res.id, 789);
    EXPECT_TRUE(res.isError());
    EXPECT_EQ(res.error["code"], static_cast<int>(error_code::method_not_found));
    EXPECT_EQ(res.error["message"], "Method not found");
    EXPECT_EQ(res.error["data"]["details"], "More info");
}

TEST_F(MCPProtocolTest, ResponseIsError) {
    json req_id = 1;
    auto success_res = response::createSuccess(req_id);
    EXPECT_FALSE(success_res.isError());
    
    auto error_res = response::createError(req_id, error_code::internal_error, "Error");
    EXPECT_TRUE(error_res.isError());
}

TEST_F(MCPProtocolTest, ResponseToJsonSuccess) {
    json req_id = 123;
    json result_data = {{"key", "value"}};
    auto res = response::createSuccess(req_id, result_data);
    auto j = res.toJson();
    
    EXPECT_EQ(j["jsonrpc"], "2.0");
    EXPECT_EQ(j["id"], 123);
    EXPECT_TRUE(j.contains("result"));
    EXPECT_FALSE(j.contains("error"));
    EXPECT_EQ(j["result"]["key"], "value");
}

TEST_F(MCPProtocolTest, ResponseToJsonError) {
    json req_id = 456;
    auto res = response::createError(req_id, error_code::parse_error, "Parse error");
    auto j = res.toJson();
    
    EXPECT_EQ(j["jsonrpc"], "2.0");
    EXPECT_EQ(j["id"], 456);
    EXPECT_FALSE(j.contains("result"));
    EXPECT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"]["code"], static_cast<int>(error_code::parse_error));
    EXPECT_EQ(j["error"]["message"], "Parse error");
}

TEST_F(MCPProtocolTest, ResponseFromJsonSuccess) {
    json j = {
        {"jsonrpc", "2.0"},
        {"id", 123},
        {"result", {{"key", "value"}}}
    };
    
    auto res = response::fromJson(j);
    EXPECT_EQ(res.jsonrpc, "2.0");
    EXPECT_EQ(res.id, 123);
    EXPECT_FALSE(res.isError());
    EXPECT_EQ(res.result["key"], "value");
}

TEST_F(MCPProtocolTest, ResponseFromJsonError) {
    json j = {
        {"jsonrpc", "2.0"},
        {"id", 456},
        {"error", {
            {"code", static_cast<int>(error_code::invalid_request)},
            {"message", "Invalid request"}
        }}
    };
    
    auto res = response::fromJson(j);
    EXPECT_EQ(res.jsonrpc, "2.0");
    EXPECT_EQ(res.id, 456);
    EXPECT_TRUE(res.isError());
    EXPECT_EQ(res.error["code"], static_cast<int>(error_code::invalid_request));
    EXPECT_EQ(res.error["message"], "Invalid request");
}

// ============================================================================
// Tests for mcp_protocol_core.h - ClientCapabilities
// ============================================================================
TEST_F(MCPProtocolTest, ClientCapabilitiesToJson) {
    ClientCapabilities caps;
    caps.logging = json{{"level", "info"}};
    caps.prompts = json{{"listChanged", true}};
    caps.resources = json{{"subscribe", true}};
    caps.tools = json{{"listChanged", true}};
    caps.sampling = json{{"maxTokens", 4096}};
    
    auto j = caps.toJson();
    EXPECT_EQ(j["logging"]["level"], "info");
    EXPECT_EQ(j["prompts"]["listChanged"], true);
    EXPECT_EQ(j["resources"]["subscribe"], true);
    EXPECT_EQ(j["tools"]["listChanged"], true);
    EXPECT_EQ(j["sampling"]["maxTokens"], 4096);
}

TEST_F(MCPProtocolTest, ClientCapabilitiesFromJson) {
    json j = {
        {"logging", {{"level", "debug"}}},
        {"prompts", {{"listChanged", false}}},
        {"resources", {{"subscribe", true}}},
        {"tools", {{"listChanged", true}}},
        {"sampling", {{"maxTokens", 2048}}}
    };
    
    auto caps = ClientCapabilities::fromJson(j);
    EXPECT_EQ(caps.logging["level"], "debug");
    EXPECT_EQ(caps.prompts["listChanged"], false);
    EXPECT_EQ(caps.resources["subscribe"], true);
    EXPECT_EQ(caps.tools["listChanged"], true);
    EXPECT_EQ(caps.sampling["maxTokens"], 2048);
}

TEST_F(MCPProtocolTest, ClientCapabilitiesEmptyFields) {
    ClientCapabilities caps;
    auto j = caps.toJson();
    // Empty fields should not appear in JSON
    EXPECT_FALSE(j.contains("logging"));
    EXPECT_FALSE(j.contains("prompts"));
    EXPECT_FALSE(j.contains("resources"));
    EXPECT_FALSE(j.contains("tools"));
    EXPECT_FALSE(j.contains("sampling"));
}

// ============================================================================
// Tests for mcp_protocol_core.h - ServerCapabilities
// ============================================================================
TEST_F(MCPProtocolTest, ServerCapabilitiesToJson) {
    ServerCapabilities caps;
    caps.experimental = json{{"feature", true}};
    caps.prompts = json{{"listChanged", false}};
    caps.resources = json{{"subscribe", false}};
    caps.tools = json{{"listChanged", false}};
    caps.logging = json{{"level", "info"}};
    caps.sampling = json{{"maxTokens", 2048}};
    
    auto j = caps.toJson();
    EXPECT_EQ(j["experimental"]["feature"], true);
    EXPECT_EQ(j["prompts"]["listChanged"], false);
    EXPECT_EQ(j["resources"]["subscribe"], false);
    EXPECT_EQ(j["tools"]["listChanged"], false);
    EXPECT_EQ(j["logging"]["level"], "info");
    EXPECT_EQ(j["sampling"]["maxTokens"], 2048);
}

TEST_F(MCPProtocolTest, ServerCapabilitiesFromJson) {
    json j = {
        {"experimental", {{"feature", true}}},
        {"prompts", {{"listChanged", true}}},
        {"resources", {{"subscribe", true}}},
        {"tools", {{"listChanged", true}}},
        {"logging", {{"level", "debug"}}},
        {"sampling", {{"maxTokens", 4096}}}
    };
    
    auto caps = ServerCapabilities::fromJson(j);
    EXPECT_EQ(caps.experimental["feature"], true);
    EXPECT_EQ(caps.prompts["listChanged"], true);
    EXPECT_EQ(caps.resources["subscribe"], true);
    EXPECT_EQ(caps.tools["listChanged"], true);
    EXPECT_EQ(caps.logging["level"], "debug");
    EXPECT_EQ(caps.sampling["maxTokens"], 4096);
}

// ============================================================================
// Tests for mcp_protocol_core.h - ClientInfo
// ============================================================================
TEST_F(MCPProtocolTest, ClientInfoToJson) {
    ClientInfo info;
    info.name = "TestClient";
    info.version = "1.0.0";
    
    auto j = info.toJson();
    EXPECT_EQ(j["name"], "TestClient");
    EXPECT_EQ(j["version"], "1.0.0");
}

TEST_F(MCPProtocolTest, ClientInfoFromJson) {
    json j = {
        {"name", "TestClient"},
        {"version", "1.0.0"}
    };
    
    auto info = ClientInfo::fromJson(j);
    EXPECT_EQ(info.name, "TestClient");
    EXPECT_EQ(info.version, "1.0.0");
}

TEST_F(MCPProtocolTest, ClientInfoFromJsonPartial) {
    json j = {
        {"name", "TestClient"}
    };
    
    auto info = ClientInfo::fromJson(j);
    EXPECT_EQ(info.name, "TestClient");
    EXPECT_EQ(info.version, "");  // Default value
}

// ============================================================================
// Tests for mcp_protocol_core.h - ServerInfo
// ============================================================================
TEST_F(MCPProtocolTest, ServerInfoToJson) {
    ServerInfo info;
    info.name = "TestServer";
    info.version = "2.0.0";
    
    auto j = info.toJson();
    EXPECT_EQ(j["name"], "TestServer");
    EXPECT_EQ(j["version"], "2.0.0");
}

TEST_F(MCPProtocolTest, ServerInfoFromJson) {
    json j = {
        {"name", "TestServer"},
        {"version", "2.0.0"}
    };
    
    auto info = ServerInfo::fromJson(j);
    EXPECT_EQ(info.name, "TestServer");
    EXPECT_EQ(info.version, "2.0.0");
}

// ============================================================================
// Tests for mcp_protocol_core.h - InitializeParams
// ============================================================================
TEST_F(MCPProtocolTest, InitializeParamsToJson) {
    InitializeParams params;
    params.protocolVersion = "2024-11-05";
    params.capabilities.logging = json{{"level", "info"}};
    params.clientInfo.name = "TestClient";
    params.clientInfo.version = "1.0.0";
    
    auto j = params.toJson();
    EXPECT_EQ(j["protocolVersion"], "2024-11-05");
    EXPECT_EQ(j["capabilities"]["logging"]["level"], "info");
    EXPECT_EQ(j["clientInfo"]["name"], "TestClient");
    EXPECT_EQ(j["clientInfo"]["version"], "1.0.0");
}

TEST_F(MCPProtocolTest, InitializeParamsFromJson) {
    json j = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"logging", {{"level", "debug"}}}
        }},
        {"clientInfo", {
            {"name", "TestClient"},
            {"version", "1.0.0"}
        }}
    };
    
    auto params = InitializeParams::fromJson(j);
    EXPECT_EQ(params.protocolVersion, "2024-11-05");
    EXPECT_EQ(params.capabilities.logging["level"], "debug");
    EXPECT_EQ(params.clientInfo.name, "TestClient");
    EXPECT_EQ(params.clientInfo.version, "1.0.0");
}

TEST_F(MCPProtocolTest, InitializeParamsDefaultVersion) {
    json j = {
        {"capabilities", json::object()},
        {"clientInfo", json::object()}
    };
    
    auto params = InitializeParams::fromJson(j);
    EXPECT_EQ(params.protocolVersion, PROTOCOL_VERSION);
}

// ============================================================================
// Tests for mcp_protocol_core.h - InitializeResult
// ============================================================================
TEST_F(MCPProtocolTest, InitializeResultToJson) {
    InitializeResult result;
    result.protocolVersion = "2024-11-05";
    result.capabilities.prompts = json{{"listChanged", false}};
    result.serverInfo.name = "TestServer";
    result.serverInfo.version = "2.0.0";
    
    auto j = result.toJson();
    EXPECT_EQ(j["protocolVersion"], "2024-11-05");
    EXPECT_EQ(j["capabilities"]["prompts"]["listChanged"], false);
    EXPECT_EQ(j["serverInfo"]["name"], "TestServer");
    EXPECT_EQ(j["serverInfo"]["version"], "2.0.0");
}

TEST_F(MCPProtocolTest, InitializeResultFromJson) {
    json j = {
        {"protocolVersion", "2024-11-05"},
        {"capabilities", {
            {"prompts", {{"listChanged", true}}}
        }},
        {"serverInfo", {
            {"name", "TestServer"},
            {"version", "2.0.0"}
        }}
    };
    
    auto result = InitializeResult::fromJson(j);
    EXPECT_EQ(result.protocolVersion, "2024-11-05");
    EXPECT_EQ(result.capabilities.prompts["listChanged"], true);
    EXPECT_EQ(result.serverInfo.name, "TestServer");
    EXPECT_EQ(result.serverInfo.version, "2.0.0");
}

// ============================================================================
// Tests for mcp_protocol_core.h - Tool
// ============================================================================
TEST_F(MCPProtocolTest, ToolToJson) {
    Tool tool;
    tool.name = "test_tool";
    tool.description = "A test tool";
    tool.inputSchema = json{{"type", "object"}};
    
    auto j = tool.toJson();
    EXPECT_EQ(j["name"], "test_tool");
    EXPECT_EQ(j["description"], "A test tool");
    EXPECT_EQ(j["inputSchema"]["type"], "object");
}

TEST_F(MCPProtocolTest, ToolFromJson) {
    json j = {
        {"name", "test_tool"},
        {"description", "A test tool"},
        {"inputSchema", {{"type", "object"}}}
    };
    
    auto tool = Tool::fromJson(j);
    EXPECT_EQ(tool.name, "test_tool");
    EXPECT_EQ(tool.description, "A test tool");
    EXPECT_EQ(tool.inputSchema["type"], "object");
}

TEST_F(MCPProtocolTest, ToolFromJsonPartial) {
    json j = {
        {"name", "test_tool"}
    };
    
    auto tool = Tool::fromJson(j);
    EXPECT_EQ(tool.name, "test_tool");
    EXPECT_EQ(tool.description, "");
    EXPECT_TRUE(tool.inputSchema.empty());
}

// ============================================================================
// Tests for mcp_protocol_core.h - ToolCallParams
// ============================================================================
TEST_F(MCPProtocolTest, ToolCallParamsToJson) {
    ToolCallParams params;
    params.name = "test_tool";
    params.arguments = json{{"arg1", "value1"}};
    
    auto j = params.toJson();
    EXPECT_EQ(j["name"], "test_tool");
    EXPECT_EQ(j["arguments"]["arg1"], "value1");
}

TEST_F(MCPProtocolTest, ToolCallParamsFromJson) {
    json j = {
        {"name", "test_tool"},
        {"arguments", {{"arg1", "value1"}}}
    };
    
    auto params = ToolCallParams::fromJson(j);
    EXPECT_EQ(params.name, "test_tool");
    EXPECT_EQ(params.arguments["arg1"], "value1");
}

// ============================================================================
// Tests for mcp_protocol_core.h - ToolCallResult
// ============================================================================
TEST_F(MCPProtocolTest, ToolCallResultToJsonSuccess) {
    ToolCallResult result;
    result.isError = false;
    result.content = json{{"output", "success"}};
    
    auto j = result.toJson();
    EXPECT_TRUE(j.contains("content"));
    EXPECT_FALSE(j.contains("error"));
    // Content should be an array of content items (MCP protocol format)
    EXPECT_TRUE(j["content"].is_array());
    EXPECT_EQ(j["content"].size(), 1);
    EXPECT_EQ(j["content"][0]["type"], "text");
    // The object is serialized as JSON string in the text field
    EXPECT_TRUE(j["content"][0]["text"].is_string());
}

TEST_F(MCPProtocolTest, ToolCallResultToJsonError) {
    ToolCallResult result;
    result.isError = true;
    result.errorMessage = "Tool execution failed";
    
    auto j = result.toJson();
    EXPECT_FALSE(j.contains("content"));
    EXPECT_TRUE(j.contains("error"));
    EXPECT_EQ(j["error"], "Tool execution failed");
}

TEST_F(MCPProtocolTest, ToolCallResultFromJsonSuccess) {
    json j = {
        {"content", {{"output", "success"}}}
    };
    
    auto result = ToolCallResult::fromJson(j);
    EXPECT_FALSE(result.isError);
    EXPECT_EQ(result.content["output"], "success");
}

TEST_F(MCPProtocolTest, ToolCallResultFromJsonError) {
    json j = {
        {"error", "Tool execution failed"}
    };
    
    auto result = ToolCallResult::fromJson(j);
    EXPECT_TRUE(result.isError);
    EXPECT_EQ(result.errorMessage, "Tool execution failed");
}

// ============================================================================
// Tests for mcp_protocol_core.h - Resource
// ============================================================================
TEST_F(MCPProtocolTest, ResourceToJson) {
    Resource resource;
    resource.uri = "file:///test.txt";
    resource.name = "Test Resource";
    resource.description = "A test resource";
    resource.mimeType = "text/plain";
    
    auto j = resource.toJson();
    EXPECT_EQ(j["uri"], "file:///test.txt");
    EXPECT_EQ(j["name"], "Test Resource");
    EXPECT_EQ(j["description"], "A test resource");
    EXPECT_EQ(j["mimeType"], "text/plain");
}

TEST_F(MCPProtocolTest, ResourceFromJson) {
    json j = {
        {"uri", "file:///test.txt"},
        {"name", "Test Resource"},
        {"description", "A test resource"},
        {"mimeType", "text/plain"}
    };
    
    auto resource = Resource::fromJson(j);
    EXPECT_EQ(resource.uri, "file:///test.txt");
    EXPECT_EQ(resource.name, "Test Resource");
    EXPECT_EQ(resource.description, "A test resource");
    EXPECT_EQ(resource.mimeType, "text/plain");
}

TEST_F(MCPProtocolTest, ResourceFromJsonPartial) {
    json j = {
        {"uri", "file:///test.txt"}
    };
    
    auto resource = Resource::fromJson(j);
    EXPECT_EQ(resource.uri, "file:///test.txt");
    EXPECT_EQ(resource.name, "");
    EXPECT_EQ(resource.description, "");
    EXPECT_EQ(resource.mimeType, "");
}

// ============================================================================
// Tests for mcp_protocol_core.h - Prompt
// ============================================================================
TEST_F(MCPProtocolTest, PromptToJson) {
    Prompt prompt;
    prompt.name = "test_prompt";
    prompt.description = "A test prompt";
    prompt.arguments = {"arg1", "arg2", "arg3"};
    
    auto j = prompt.toJson();
    EXPECT_EQ(j["name"], "test_prompt");
    EXPECT_EQ(j["description"], "A test prompt");
    EXPECT_TRUE(j["arguments"].is_array());
    EXPECT_EQ(j["arguments"].size(), 3);
    EXPECT_EQ(j["arguments"][0], "arg1");
    EXPECT_EQ(j["arguments"][1], "arg2");
    EXPECT_EQ(j["arguments"][2], "arg3");
}

TEST_F(MCPProtocolTest, PromptFromJson) {
    json j = {
        {"name", "test_prompt"},
        {"description", "A test prompt"},
        {"arguments", {"arg1", "arg2", "arg3"}}
    };
    
    auto prompt = Prompt::fromJson(j);
    EXPECT_EQ(prompt.name, "test_prompt");
    EXPECT_EQ(prompt.description, "A test prompt");
    EXPECT_EQ(prompt.arguments.size(), 3);
    EXPECT_EQ(prompt.arguments[0], "arg1");
    EXPECT_EQ(prompt.arguments[1], "arg2");
    EXPECT_EQ(prompt.arguments[2], "arg3");
}

TEST_F(MCPProtocolTest, PromptFromJsonNoArguments) {
    json j = {
        {"name", "test_prompt"},
        {"description", "A test prompt"}
    };
    
    auto prompt = Prompt::fromJson(j);
    EXPECT_EQ(prompt.name, "test_prompt");
    EXPECT_EQ(prompt.description, "A test prompt");
    EXPECT_TRUE(prompt.arguments.empty());
}

// ============================================================================
// Tests for mcp_protocol_core.h - utils namespace functions
// ============================================================================
TEST_F(MCPProtocolTest, CreateDefaultClientCapabilities) {
    auto caps = utils::createDefaultClientCapabilities();
    
    // logging is set to empty json object, so it's empty but exists
    EXPECT_TRUE(caps.logging.is_object());
    EXPECT_FALSE(caps.prompts.empty());
    EXPECT_FALSE(caps.resources.empty());
    EXPECT_FALSE(caps.tools.empty());
    EXPECT_FALSE(caps.sampling.empty());
    
    EXPECT_EQ(caps.prompts["listChanged"], true);
    EXPECT_EQ(caps.resources["subscribe"], true);
    EXPECT_EQ(caps.tools["listChanged"], true);
    EXPECT_EQ(caps.sampling["maxTokens"], 4096);
    EXPECT_EQ(caps.sampling["temperature"], 0.7);
}

TEST_F(MCPProtocolTest, CreateDefaultServerCapabilities) {
    auto caps = utils::createDefaultServerCapabilities();
    
    EXPECT_FALSE(caps.prompts.empty());
    EXPECT_FALSE(caps.resources.empty());
    EXPECT_FALSE(caps.tools.empty());
    EXPECT_FALSE(caps.sampling.empty());
    
    EXPECT_EQ(caps.prompts["listChanged"], false);
    EXPECT_EQ(caps.resources["listChanged"], false);
    EXPECT_EQ(caps.resources["subscribe"], false);
    EXPECT_EQ(caps.tools["listChanged"], false);
    EXPECT_EQ(caps.sampling["maxTokens"], 2048);
    EXPECT_EQ(caps.sampling["temperature"], 0.5);
}

TEST_F(MCPProtocolTest, IsValidProtocolVersion) {
    EXPECT_TRUE(utils::isValidProtocolVersion("2024-11-05"));
    EXPECT_TRUE(utils::isValidProtocolVersion("2025-03-26"));
    EXPECT_TRUE(utils::isValidProtocolVersion("2025-06-18"));
    EXPECT_FALSE(utils::isValidProtocolVersion("2024-01-01"));
    EXPECT_FALSE(utils::isValidProtocolVersion("invalid"));
    EXPECT_FALSE(utils::isValidProtocolVersion(""));
}

TEST_F(MCPProtocolTest, CreateToolFromSchema) {
    json schema = {
        {"type", "object"},
        {"properties", {
            {"param1", {{"type", "string"}}}
        }}
    };
    
    auto tool = utils::createToolFromSchema("test_tool", "A test tool", schema);
    EXPECT_EQ(tool.name, "test_tool");
    EXPECT_EQ(tool.description, "A test tool");
    EXPECT_EQ(tool.inputSchema["type"], "object");
    EXPECT_TRUE(tool.inputSchema.contains("properties"));
}

TEST_F(MCPProtocolTest, CreateResourceFromUri) {
    auto resource = utils::createResourceFromUri("file:///test.txt", "Test Resource", "A test resource");
    EXPECT_EQ(resource.uri, "file:///test.txt");
    EXPECT_EQ(resource.name, "Test Resource");
    EXPECT_EQ(resource.description, "A test resource");
    EXPECT_EQ(resource.mimeType, "text/plain");
}

TEST_F(MCPProtocolTest, CreateResourceFromUriDefaultName) {
    auto resource = utils::createResourceFromUri("file:///test.txt");
    EXPECT_EQ(resource.uri, "file:///test.txt");
    EXPECT_EQ(resource.name, "file:///test.txt");  // Uses URI as name if not provided
    EXPECT_EQ(resource.description, "");
    EXPECT_EQ(resource.mimeType, "text/plain");
}

TEST_F(MCPProtocolTest, CreateResourceFromUriWithNameOnly) {
    auto resource = utils::createResourceFromUri("file:///test.txt", "Test Resource");
    EXPECT_EQ(resource.uri, "file:///test.txt");
    EXPECT_EQ(resource.name, "Test Resource");
    EXPECT_EQ(resource.description, "");
    EXPECT_EQ(resource.mimeType, "text/plain");
}

// ============================================================================
// Tests for protocol constants
// ============================================================================
TEST_F(MCPProtocolTest, ProtocolVersionConstant) {
    EXPECT_STREQ(PROTOCOL_VERSION, "2025-11-25");
    EXPECT_STREQ(MCP_VERSION, "2025-11-25");
    EXPECT_STREQ(PROTOCOL_VERSION_2024_11_05, "2024-11-05");
    EXPECT_STREQ(PROTOCOL_VERSION_2025_11_25, "2025-11-25");
    EXPECT_STREQ(MCP_VERSION_2024_11_05, "2024-11-05");
    EXPECT_STREQ(MCP_VERSION_2025_11_25, "2025-11-25");
}

TEST_F(MCPProtocolTest, ProtocolMethodsConstants) {
    EXPECT_STREQ(methods::INITIALIZE, "initialize");
    EXPECT_STREQ(methods::PING, "ping");
    EXPECT_STREQ(methods::RESOURCES_LIST, "resources/list");
    EXPECT_STREQ(methods::RESOURCES_READ, "resources/read");
    EXPECT_STREQ(methods::RESOURCES_TEMPLATES_LIST, "resources/templates/list");
    EXPECT_STREQ(methods::TOOLS_LIST, "tools/list");
    EXPECT_STREQ(methods::TOOLS_CALL, "tools/call");
    EXPECT_STREQ(methods::PROMPTS_LIST, "prompts/list");
    EXPECT_STREQ(methods::PROMPTS_GET, "prompts/get");
    EXPECT_STREQ(methods::LOGGING_SET_LEVEL, "logging/setLevel");
    EXPECT_STREQ(methods::TASKS_GET, "tasks/get");
    EXPECT_STREQ(methods::TASKS_RESULT, "tasks/result");
    EXPECT_STREQ(methods::TASKS_LIST, "tasks/list");
    EXPECT_STREQ(methods::TASKS_CANCEL, "tasks/cancel");
    EXPECT_STREQ(methods::ELICITATION_CREATE, "elicitation/create");
    EXPECT_STREQ(methods::COMPLETION_COMPLETE, "completion/complete");
    EXPECT_STREQ(methods::SAMPLING_CREATE_MESSAGE, "sampling/createMessage");
    EXPECT_STREQ(methods::RESOURCES_SUBSCRIBE, "resources/subscribe");
    EXPECT_STREQ(methods::RESOURCES_UNSUBSCRIBE, "resources/unsubscribe");
    EXPECT_STREQ(methods::ROOTS_LIST, "roots/list");
    EXPECT_STREQ(methods::NOTIFICATION_INITIALIZED, "notifications/initialized");
    EXPECT_STREQ(methods::NOTIFICATION_CANCELLED, "notifications/cancelled");
    EXPECT_STREQ(methods::NOTIFICATION_RESOURCES_LIST_CHANGED, "notifications/resources/list_changed");
    EXPECT_STREQ(methods::NOTIFICATION_RESOURCE_UPDATED, "notifications/resources/updated");
    EXPECT_STREQ(methods::NOTIFICATION_PROMPTS_LIST_CHANGED, "notifications/prompts/list_changed");
    EXPECT_STREQ(methods::NOTIFICATION_TOOLS_LIST_CHANGED, "notifications/tools/list_changed");
    EXPECT_STREQ(methods::NOTIFICATION_ROOTS_LIST_CHANGED, "notifications/roots/list_changed");
    EXPECT_STREQ(methods::NOTIFICATION_MESSAGE, "notifications/message");
    EXPECT_STREQ(methods::NOTIFICATION_PROGRESS, "notifications/progress");
    EXPECT_STREQ(methods::NOTIFICATION_TASKS_STATUS, "notifications/tasks/status");
    EXPECT_STREQ(methods::NOTIFICATION_ELICITATION_COMPLETE, "notifications/elicitation/complete");
}

// ============================================================================
// Integration tests - Round-trip JSON serialization
// ============================================================================
TEST_F(MCPProtocolTest, ClientCapabilitiesRoundTrip) {
    ClientCapabilities original;
    original.logging = json{{"level", "info"}};
    original.prompts = json{{"listChanged", true}};
    
    auto j = original.toJson();
    auto restored = ClientCapabilities::fromJson(j);
    
    EXPECT_EQ(restored.logging["level"], "info");
    EXPECT_EQ(restored.prompts["listChanged"], true);
}

TEST_F(MCPProtocolTest, RequestResponseRoundTrip) {
    // Create request
    auto req = request::create("test/method", json{{"key", "value"}});
    auto req_json = req.toJson();
    auto req_restored = request::fromJson(req_json);
    
    EXPECT_EQ(req_restored.method, "test/method");
    EXPECT_EQ(req_restored.params["key"], "value");
    
    // Create response
    auto res = response::createSuccess(req_restored.id, json{{"result", "ok"}});
    auto res_json = res.toJson();
    auto res_restored = response::fromJson(res_json);
    
    EXPECT_FALSE(res_restored.isError());
    EXPECT_EQ(res_restored.result["result"], "ok");
}

TEST_F(MCPProtocolTest, InitializeParamsRoundTrip) {
    InitializeParams original;
    original.protocolVersion = "2024-11-05";
    original.capabilities.logging = json{{"level", "debug"}};
    original.clientInfo.name = "TestClient";
    original.clientInfo.version = "1.0.0";
    
    auto j = original.toJson();
    auto restored = InitializeParams::fromJson(j);
    
    EXPECT_EQ(restored.protocolVersion, "2024-11-05");
    EXPECT_EQ(restored.capabilities.logging["level"], "debug");
    EXPECT_EQ(restored.clientInfo.name, "TestClient");
    EXPECT_EQ(restored.clientInfo.version, "1.0.0");
}

// ============================================================================
// Tests for mcp_protocol_core.h - MetaData (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, MetaDataToJson) {
    MetaData meta;
    meta.progressToken = "token123";
    
    auto j = meta.toJson();
    EXPECT_EQ(j["progressToken"], "token123");
}

TEST_F(MCPProtocolTest, MetaDataFromJson) {
    json j = {{"progressToken", "token456"}};
    auto meta = MetaData::fromJson(j);
    EXPECT_EQ(meta.progressToken, "token456");
}

TEST_F(MCPProtocolTest, MetaDataEmpty) {
    MetaData meta;
    EXPECT_TRUE(meta.empty());
    
    meta.progressToken = "token";
    EXPECT_FALSE(meta.empty());
}

TEST_F(MCPProtocolTest, MetaDataEmptyJson) {
    json j = json::object();
    auto meta = MetaData::fromJson(j);
    EXPECT_TRUE(meta.progressToken.is_null());
    EXPECT_TRUE(meta.empty());
}

// ============================================================================
// Tests for mcp_protocol_core.h - InitializeParams with _meta (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, InitializeParamsWithMeta) {
    InitializeParams params;
    params.protocolVersion = "2025-11-25";
    params._meta.progressToken = "progress123";
    
    auto j = params.toJson();
    EXPECT_TRUE(j.contains("_meta"));
    EXPECT_EQ(j["_meta"]["progressToken"], "progress123");
}

TEST_F(MCPProtocolTest, InitializeParamsWithoutMeta) {
    InitializeParams params;
    params.protocolVersion = "2025-11-25";
    
    auto j = params.toJson();
    EXPECT_FALSE(j.contains("_meta"));
}

TEST_F(MCPProtocolTest, InitializeParamsFromJsonWithMeta) {
    json j = {
        {"protocolVersion", "2025-11-25"},
        {"_meta", {{"progressToken", "token789"}}}
    };
    
    auto params = InitializeParams::fromJson(j);
    EXPECT_EQ(params._meta.progressToken, "token789");
}

// ============================================================================
// Tests for mcp_protocol_core.h - InitializeResult with instructions (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, InitializeResultWithInstructions) {
    InitializeResult result;
    result.protocolVersion = "2025-11-25";
    result.instructions = "Please follow these instructions";
    
    auto j = result.toJson();
    EXPECT_EQ(j["instructions"], "Please follow these instructions");
}

TEST_F(MCPProtocolTest, InitializeResultWithoutInstructions) {
    InitializeResult result;
    result.protocolVersion = "2025-11-25";
    
    auto j = result.toJson();
    EXPECT_FALSE(j.contains("instructions"));
}

TEST_F(MCPProtocolTest, InitializeResultFromJsonWithInstructions) {
    json j = {
        {"protocolVersion", "2025-11-25"},
        {"instructions", "Test instructions"}
    };
    
    auto result = InitializeResult::fromJson(j);
    EXPECT_EQ(result.instructions, "Test instructions");
}

// ============================================================================
// Tests for mcp_protocol_core.h - ToolCallParams with _meta and task (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, ToolCallParamsWithMeta) {
    ToolCallParams params;
    params.name = "test_tool";
    params.arguments = json{{"arg", "value"}};
    params._meta.progressToken = "progress456";
    
    auto j = params.toJson();
    EXPECT_TRUE(j.contains("_meta"));
    EXPECT_EQ(j["_meta"]["progressToken"], "progress456");
}

TEST_F(MCPProtocolTest, ToolCallParamsWithTask) {
    ToolCallParams params;
    params.name = "test_tool";
    params.arguments = json{{"arg", "value"}};
    params.task.ttl = 3600;
    
    auto j = params.toJson();
    EXPECT_TRUE(j.contains("task"));
    EXPECT_EQ(j["task"]["ttl"], 3600);
}

TEST_F(MCPProtocolTest, ToolCallParamsWithMetaAndTask) {
    ToolCallParams params;
    params.name = "test_tool";
    params.arguments = json{{"arg", "value"}};
    params._meta.progressToken = "token999";
    params.task.ttl = 1800;
    
    auto j = params.toJson();
    EXPECT_TRUE(j.contains("_meta"));
    EXPECT_TRUE(j.contains("task"));
    EXPECT_EQ(j["_meta"]["progressToken"], "token999");
    EXPECT_EQ(j["task"]["ttl"], 1800);
}

TEST_F(MCPProtocolTest, ToolCallParamsFromJsonWithMetaAndTask) {
    json j = {
        {"name", "test_tool"},
        {"arguments", {{"arg", "value"}}},
        {"_meta", {{"progressToken", "token111"}}},
        {"task", {{"ttl", 7200}}}
    };
    
    auto params = ToolCallParams::fromJson(j);
    EXPECT_EQ(params._meta.progressToken, "token111");
    EXPECT_EQ(params.task.ttl, 7200);
}

TEST_F(MCPProtocolTest, ToolCallParamsTaskMetadataEmpty) {
    ToolCallParams::TaskMetadataParams task;
    EXPECT_TRUE(task.empty());
    
    task.ttl = 100;
    EXPECT_FALSE(task.empty());
}

// ============================================================================
// Tests for mcp_protocol_core.h - ToolCallResult with structuredContent (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, ToolCallResultWithStructuredContent) {
    ToolCallResult result;
    result.isError = false;
    result.content = json{{"output", "success"}};
    result.structuredContent = json{{"type", "object"}, {"data", "structured"}};
    
    auto j = result.toJson();
    EXPECT_TRUE(j.contains("structuredContent"));
    EXPECT_EQ(j["structuredContent"]["type"], "object");
}

TEST_F(MCPProtocolTest, ToolCallResultWithoutStructuredContent) {
    ToolCallResult result;
    result.isError = false;
    result.content = json{{"output", "success"}};
    
    auto j = result.toJson();
    EXPECT_FALSE(j.contains("structuredContent"));
}

TEST_F(MCPProtocolTest, ToolCallResultFromJsonWithStructuredContent) {
    json j = {
        {"content", {{"output", "success"}}},
        {"structuredContent", {{"type", "array"}, {"items", json::array()}}}
    };
    
    auto result = ToolCallResult::fromJson(j);
    EXPECT_FALSE(result.isError);
    EXPECT_EQ(result.structuredContent["type"], "array");
}

// ============================================================================
// Tests for mcp_protocol_core.h - Role enum (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, RoleToString) {
    EXPECT_EQ(roleToString(Role::assistant), "assistant");
    EXPECT_EQ(roleToString(Role::user), "user");
}

TEST_F(MCPProtocolTest, RoleFromString) {
    EXPECT_EQ(roleFromString("assistant"), Role::assistant);
    EXPECT_EQ(roleFromString("user"), Role::user);
    // Default to user for invalid strings
    EXPECT_EQ(roleFromString("invalid"), Role::user);
}

// ============================================================================
// Tests for mcp_protocol_core.h - Content Types (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, TextContentToJson) {
    TextContent content;
    content.text = "Hello, world!";
    
    auto j = content.toJson();
    EXPECT_EQ(j["type"], "text");
    EXPECT_EQ(j["text"], "Hello, world!");
}

TEST_F(MCPProtocolTest, TextContentFromJson) {
    json j = {{"type", "text"}, {"text", "Test text"}};
    auto content = TextContent::fromJson(j);
    EXPECT_EQ(content.type, "text");
    EXPECT_EQ(content.text, "Test text");
}

TEST_F(MCPProtocolTest, ImageContentToJson) {
    ImageContent content;
    content.data = "base64data";
    content.mimeType = "image/png";
    
    auto j = content.toJson();
    EXPECT_EQ(j["type"], "image");
    EXPECT_EQ(j["data"], "base64data");
    EXPECT_EQ(j["mimeType"], "image/png");
}

TEST_F(MCPProtocolTest, ImageContentFromJson) {
    json j = {{"type", "image"}, {"data", "data123"}, {"mimeType", "image/jpeg"}};
    auto content = ImageContent::fromJson(j);
    EXPECT_EQ(content.type, "image");
    EXPECT_EQ(content.data, "data123");
    EXPECT_EQ(content.mimeType, "image/jpeg");
}

TEST_F(MCPProtocolTest, AudioContentToJson) {
    AudioContent content;
    content.data = "audiodata";
    content.mimeType = "audio/mpeg";
    
    auto j = content.toJson();
    EXPECT_EQ(j["type"], "audio");
    EXPECT_EQ(j["data"], "audiodata");
    EXPECT_EQ(j["mimeType"], "audio/mpeg");
}

TEST_F(MCPProtocolTest, AudioContentFromJson) {
    json j = {{"type", "audio"}, {"data", "audio123"}, {"mimeType", "audio/wav"}};
    auto content = AudioContent::fromJson(j);
    EXPECT_EQ(content.type, "audio");
    EXPECT_EQ(content.data, "audio123");
    EXPECT_EQ(content.mimeType, "audio/wav");
}

// ============================================================================
// Tests for mcp_protocol_core.h - ResourceLink (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, ResourceLinkToJson) {
    ResourceLink link;
    link.uri = "file:///test.txt";
    link.text = "Test Resource";
    
    auto j = link.toJson();
    EXPECT_EQ(j["type"], "resource");
    EXPECT_EQ(j["uri"], "file:///test.txt");
    EXPECT_EQ(j["text"], "Test Resource");
}

TEST_F(MCPProtocolTest, ResourceLinkToJsonWithoutText) {
    ResourceLink link;
    link.uri = "file:///test.txt";
    
    auto j = link.toJson();
    EXPECT_EQ(j["type"], "resource");
    EXPECT_EQ(j["uri"], "file:///test.txt");
    EXPECT_FALSE(j.contains("text"));
}

TEST_F(MCPProtocolTest, ResourceLinkFromJson) {
    json j = {{"type", "resource"}, {"uri", "file:///test.txt"}, {"text", "Test"}};
    auto link = ResourceLink::fromJson(j);
    EXPECT_EQ(link.type, "resource");
    EXPECT_EQ(link.uri, "file:///test.txt");
    EXPECT_EQ(link.text, "Test");
}

// ============================================================================
// Tests for mcp_protocol_core.h - TaskStatus enum (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, TaskStatusToString) {
    EXPECT_EQ(taskStatusToString(TaskStatus::working), "working");
    EXPECT_EQ(taskStatusToString(TaskStatus::completed), "completed");
    EXPECT_EQ(taskStatusToString(TaskStatus::failed), "failed");
    EXPECT_EQ(taskStatusToString(TaskStatus::cancelled), "cancelled");
    EXPECT_EQ(taskStatusToString(TaskStatus::input_required), "input_required");
}

TEST_F(MCPProtocolTest, TaskStatusFromString) {
    EXPECT_EQ(taskStatusFromString("working"), TaskStatus::working);
    EXPECT_EQ(taskStatusFromString("completed"), TaskStatus::completed);
    EXPECT_EQ(taskStatusFromString("failed"), TaskStatus::failed);
    EXPECT_EQ(taskStatusFromString("cancelled"), TaskStatus::cancelled);
    EXPECT_EQ(taskStatusFromString("input_required"), TaskStatus::input_required);
    // Default to working for invalid strings
    EXPECT_EQ(taskStatusFromString("invalid"), TaskStatus::working);
}

// ============================================================================
// Tests for mcp_protocol_core.h - TaskMetadata (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, TaskMetadataToJson) {
    TaskMetadata metadata;
    metadata.ttl = 3600;
    
    auto j = metadata.toJson();
    EXPECT_EQ(j["ttl"], 3600);
}

TEST_F(MCPProtocolTest, TaskMetadataToJsonEmpty) {
    TaskMetadata metadata;
    auto j = metadata.toJson();
    EXPECT_TRUE(j.empty());
}

TEST_F(MCPProtocolTest, TaskMetadataFromJson) {
    json j = {{"ttl", 7200}};
    auto metadata = TaskMetadata::fromJson(j);
    EXPECT_EQ(metadata.ttl, 7200);
}

TEST_F(MCPProtocolTest, TaskMetadataEmpty) {
    TaskMetadata metadata;
    EXPECT_TRUE(metadata.empty());
    
    metadata.ttl = 100;
    EXPECT_FALSE(metadata.empty());
}

// ============================================================================
// Tests for mcp_protocol_core.h - Task (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, TaskToJson) {
    Task task;
    task.taskId = "task123";
    task.status = TaskStatus::working;
    task.createdAt = "2025-01-01T00:00:00Z";
    task.lastUpdatedAt = "2025-01-01T01:00:00Z";
    task.statusMessage = "Processing";
    task.ttl = 3600;
    task.pollInterval = 5;
    
    auto j = task.toJson();
    EXPECT_EQ(j["taskId"], "task123");
    EXPECT_EQ(j["status"], "working");
    EXPECT_EQ(j["createdAt"], "2025-01-01T00:00:00Z");
    EXPECT_EQ(j["lastUpdatedAt"], "2025-01-01T01:00:00Z");
    EXPECT_EQ(j["statusMessage"], "Processing");
    EXPECT_EQ(j["ttl"], 3600);
    EXPECT_EQ(j["pollInterval"], 5);
}

TEST_F(MCPProtocolTest, TaskToJsonOptionalFields) {
    Task task;
    task.taskId = "task456";
    task.status = TaskStatus::completed;
    task.createdAt = "2025-01-01T00:00:00Z";
    task.lastUpdatedAt = "2025-01-01T01:00:00Z";
    
    auto j = task.toJson();
    EXPECT_FALSE(j.contains("statusMessage"));
    EXPECT_FALSE(j.contains("ttl"));
    EXPECT_FALSE(j.contains("pollInterval"));
}

TEST_F(MCPProtocolTest, TaskFromJson) {
    json j = {
        {"taskId", "task789"},
        {"status", "failed"},
        {"createdAt", "2025-01-01T00:00:00Z"},
        {"lastUpdatedAt", "2025-01-01T01:00:00Z"},
        {"statusMessage", "Error occurred"},
        {"ttl", 1800},
        {"pollInterval", 10}
    };
    
    auto task = Task::fromJson(j);
    EXPECT_EQ(task.taskId, "task789");
    EXPECT_EQ(task.status, TaskStatus::failed);
    EXPECT_EQ(task.statusMessage, "Error occurred");
    EXPECT_EQ(task.ttl, 1800);
    EXPECT_EQ(task.pollInterval, 10);
}

// ============================================================================
// Tests for mcp_protocol_core.h - Elicitation Types (2025-11-25)
// ============================================================================
TEST_F(MCPProtocolTest, ElicitRequestURLParamsToJson) {
    ElicitRequestURLParams params;
    params.url = "https://example.com";
    params.message = "Please visit this URL";
    
    auto j = params.toJson();
    EXPECT_EQ(j["mode"], "url");
    EXPECT_EQ(j["url"], "https://example.com");
    EXPECT_EQ(j["message"], "Please visit this URL");
}

TEST_F(MCPProtocolTest, ElicitRequestURLParamsFromJson) {
    json j = {
        {"mode", "url"},
        {"url", "https://test.com"},
        {"message", "Test message"}
    };
    
    auto params = ElicitRequestURLParams::fromJson(j);
    EXPECT_EQ(params.mode, "url");
    EXPECT_EQ(params.url, "https://test.com");
    EXPECT_EQ(params.message, "Test message");
}

TEST_F(MCPProtocolTest, ElicitRequestFormParamsToJson) {
    ElicitRequestFormParams params;
    params.message = "Please fill the form";
    params.fields = json{{"field1", {{"type", "text"}}}, {"field2", {{"type", "number"}}}};
    
    auto j = params.toJson();
    EXPECT_EQ(j["mode"], "form");
    EXPECT_EQ(j["message"], "Please fill the form");
    EXPECT_TRUE(j.contains("fields"));
}

TEST_F(MCPProtocolTest, ElicitRequestFormParamsFromJson) {
    json j = {
        {"mode", "form"},
        {"message", "Form message"},
        {"fields", {{"name", {{"type", "string"}}}}}
    };
    
    auto params = ElicitRequestFormParams::fromJson(j);
    EXPECT_EQ(params.mode, "form");
    EXPECT_EQ(params.message, "Form message");
    EXPECT_TRUE(params.fields.contains("name"));
}

TEST_F(MCPProtocolTest, ElicitCreateResultToJsonAccept) {
    ElicitCreateResult result;
    result.action = "accept";
    result.content = json{{"field1", "value1"}};
    
    auto j = result.toJson();
    EXPECT_EQ(j["action"], "accept");
    EXPECT_TRUE(j.contains("content"));
    EXPECT_EQ(j["content"]["field1"], "value1");
}

TEST_F(MCPProtocolTest, ElicitCreateResultToJsonDecline) {
    ElicitCreateResult result;
    result.action = "decline";
    
    auto j = result.toJson();
    EXPECT_EQ(j["action"], "decline");
    EXPECT_FALSE(j.contains("content"));
}

TEST_F(MCPProtocolTest, ElicitCreateResultFromJson) {
    json j = {{"action", "accept"}, {"content", {{"data", "test"}}}};
    auto result = ElicitCreateResult::fromJson(j);
    EXPECT_EQ(result.action, "accept");
    EXPECT_EQ(result.content["data"], "test");
}

// ============================================================================
// Tests for mcp_message.h - response::createError2025 and createErrorWithoutId
// ============================================================================
TEST_F(MCPProtocolTest, ResponseCreateError2025) {
    json req_id = 123;
    auto res = response::createError2025(req_id, error_code::invalid_params, "Invalid params");
    
    EXPECT_EQ(res.jsonrpc, "2.0");
    EXPECT_EQ(res.id, 123);
    EXPECT_TRUE(res.isError());
    EXPECT_EQ(res.error["code"], static_cast<int>(error_code::invalid_params));
}

TEST_F(MCPProtocolTest, ResponseCreateErrorWithoutId) {
    auto res = response::createErrorWithoutId(error_code::internal_error, "Internal error");
    
    EXPECT_EQ(res.jsonrpc, "2.0");
    EXPECT_TRUE(res.id.is_null());
    EXPECT_TRUE(res.isError());
    EXPECT_EQ(res.error["code"], static_cast<int>(error_code::internal_error));
}

TEST_F(MCPProtocolTest, ResponseToJson2025Version) {
    json req_id = 456;
    auto res = response::createError2025(req_id, error_code::method_not_found, "Not found");
    
    // Test with 2025-11-25 version
    auto j = res.toJson(MCP_VERSION_2025_11_25);
    EXPECT_TRUE(j.contains("error"));
    EXPECT_TRUE(j.contains("id"));
}

TEST_F(MCPProtocolTest, ResponseToJson2025VersionNullId) {
    auto res = response::createErrorWithoutId(error_code::parse_error, "Parse error");
    
    // Test with 2025-11-25 version - null id should not be included
    auto j = res.toJson(MCP_VERSION_2025_11_25);
    EXPECT_TRUE(j.contains("error"));
    EXPECT_FALSE(j.contains("id"));
}

// ============================================================================
// Integration tests - 2025-11-25 features
// ============================================================================
TEST_F(MCPProtocolTest, InitializeParamsWithMetaRoundTrip) {
    InitializeParams original;
    original.protocolVersion = "2025-11-25";
    original._meta.progressToken = "token123";
    
    auto j = original.toJson();
    auto restored = InitializeParams::fromJson(j);
    
    EXPECT_EQ(restored._meta.progressToken, "token123");
}

TEST_F(MCPProtocolTest, ToolCallParamsWithMetaAndTaskRoundTrip) {
    ToolCallParams original;
    original.name = "test_tool";
    original.arguments = json{{"arg", "value"}};
    original._meta.progressToken = "token456";
    original.task.ttl = 3600;
    
    auto j = original.toJson();
    auto restored = ToolCallParams::fromJson(j);
    
    EXPECT_EQ(restored._meta.progressToken, "token456");
    EXPECT_EQ(restored.task.ttl, 3600);
}

TEST_F(MCPProtocolTest, TaskRoundTrip) {
    Task original;
    original.taskId = "task123";
    original.status = TaskStatus::working;
    original.createdAt = "2025-01-01T00:00:00Z";
    original.lastUpdatedAt = "2025-01-01T01:00:00Z";
    original.ttl = 3600;
    
    auto j = original.toJson();
    auto restored = Task::fromJson(j);
    
    EXPECT_EQ(restored.taskId, "task123");
    EXPECT_EQ(restored.status, TaskStatus::working);
    EXPECT_EQ(restored.ttl, 3600);
}

// ============================================================================
// Main function for running tests
// ============================================================================
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

