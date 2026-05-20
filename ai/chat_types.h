#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

enum class ChatRole {
    User,
    Assistant,
    ToolResult
};

struct ToolCallInfo {
    std::string id;
    std::string name;
    nlohmann::json arguments;
    std::string result;
    bool isError = false;
};

struct ChatMessage {
    ChatRole role;
    std::string content;
    std::string thinking;
    std::vector<ToolCallInfo> toolCalls;
    bool isStreaming = false;
};
