#pragma once

#include <nlohmann/json.hpp>
#include <string>

class McpApi;

nlohmann::json GetToolDefinitions();
nlohmann::json ExecuteTool(const std::string& name, const nlohmann::json& params, McpApi& api);
std::string GetMcpInstructions();
