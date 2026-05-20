#include "mcp/mcp_server.h"
#include "mcp/mcp_api.h"
#include "mcp/mcp_tools.h"

#include "editor/editor_window.h"
#include "utils/log.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <random>
#include <sstream>
#include <iomanip>

McpServer::McpServer(EditorWindow* editorWindow, int port)
    : port_(port)
{
    api_ = std::make_unique<McpApi>(editorWindow);
    sessionId_ = GenerateSessionId();
}

McpServer::~McpServer()
{
    Stop();
}

void McpServer::Start()
{
    if (running_) return;

    server_ = std::make_unique<httplib::Server>();

    server_->Post("/mcp", [this](const httplib::Request& req, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "127.0.0.1");
        res.set_header("Mcp-Session-Id", sessionId_);

        std::string response = HandleRequest(req.body);
        res.set_content(response, "application/json");
    });

    server_->Delete("/mcp", [this](const httplib::Request&, httplib::Response& res) {
        initialized_ = false;
        sessionId_ = GenerateSessionId();
        res.set_content("", "application/json");
    });

    server_->Options("/mcp", [](const httplib::Request&, httplib::Response& res) {
        res.set_header("Access-Control-Allow-Origin", "127.0.0.1");
        res.set_header("Access-Control-Allow-Methods", "GET, POST, DELETE, OPTIONS");
        res.set_header("Access-Control-Allow-Headers", "Content-Type, Mcp-Session-Id, MCP-Protocol-Version");
        res.set_header("Access-Control-Expose-Headers", "Mcp-Session-Id");
    });

    running_ = true;
    serverThread_ = std::thread([this]() { Run(); });

    LOGI("MCP server started on port %d (session: %s)", port_, sessionId_.c_str());
}

void McpServer::Stop()
{
    if (!running_) return;
    running_ = false;
    if (server_) {
        server_->stop();
    }
    if (serverThread_.joinable()) {
        serverThread_.join();
    }
    LOGI("MCP server stopped");
}

bool McpServer::IsRunning() const
{
    return running_;
}

void McpServer::Run()
{
    if (!server_->listen("127.0.0.1", port_)) {
        LOGE("MCP server failed to listen on port %d", port_);
        running_ = false;
    }
}

std::string McpServer::GenerateSessionId()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    oss << std::setw(8) << dist(gen) << "-" << std::setw(4) << (dist(gen) & 0xFFFF);
    return oss.str();
}

std::string McpServer::HandleRequest(const std::string& body)
{
    try {
        auto msg = nlohmann::json::parse(body, nullptr, false);
        if (msg.is_discarded()) {
            LOGE("MCP: parse error for request body");
            nlohmann::json err = {
                {"jsonrpc", "2.0"},
                {"error", {{"code", -32700}, {"message", "Parse error"}}},
                {"id", nullptr}
            };
            return err.dump();
        }

        std::string method = msg.value("method", "");
        auto params = msg.value("params", nlohmann::json::object());
        auto id = msg.value("id", nlohmann::json(nullptr));

        LOGD("MCP HandleRequest: method=%s id=%s", method.c_str(),
            id.is_null() ? "null" : id.dump().c_str());

        if (method == "notifications/initialized") {
            initialized_ = true;
            return "";
        }

        if (!id.is_null()) {
            auto result = Route(method, params, id);
            nlohmann::json response = {
                {"jsonrpc", "2.0"},
                {"id", id}
            };
            if (result.contains("error")) {
                response["error"] = result["error"];
            } else {
                response["result"] = result;
            }
            return response.dump();
        }

        return "";
    } catch (const std::exception& e) {
        nlohmann::json err = {
            {"jsonrpc", "2.0"},
            {"error", {{"code", -32603}, {"message", std::string("Internal error: ") + e.what()}}},
            {"id", nullptr}
        };
        return err.dump();
    }
}

nlohmann::json McpServer::Route(const std::string& method, const nlohmann::json& params, const nlohmann::json& id)
{
    if (method == "initialize") {
        return HandleInitialize(params);
    }
    if (method == "initialized") {
        initialized_ = true;
        return nlohmann::json::object();
    }
    if (method == "ping") {
        return HandlePing();
    }
    if (method == "tools/list") {
        return HandleToolsList();
    }
    if (method == "tools/call") {
        return HandleToolsCall(params);
    }

    return {{"error", {{"code", -32601}, {"message", "Method not found: " + method}}}};
}

nlohmann::json McpServer::HandleInitialize(const nlohmann::json& params)
{
    initialized_ = true;
    return {
        {"protocolVersion", "2025-03-26"},
        {"capabilities", {
            {"tools", {{"listChanged", false}}}
        }},
        {"serverInfo", {
            {"name", "AuroraGraph MCP Server"},
            {"version", "0.1.0"}
        }},
        {"instructions", GetMcpInstructions()}
    };
}

nlohmann::json McpServer::HandleToolsList()
{
    return {
        {"tools", GetToolDefinitions()}
    };
}

nlohmann::json McpServer::HandleToolsCall(const nlohmann::json& params)
{
    std::string toolName = params.value("name", "");
    auto arguments = params.value("arguments", nlohmann::json::object());

    if (toolName.empty()) {
        return {
            {"content", nlohmann::json::array({{{"type", "text"}, {"text", "Missing tool name"}}})},
            {"isError", true}
        };
    }

    return ExecuteTool(toolName, arguments, *api_);
}

nlohmann::json McpServer::HandlePing()
{
    return nlohmann::json::object();
}
