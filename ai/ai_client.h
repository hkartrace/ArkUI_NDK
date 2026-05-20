#pragma once

#include "ai/chat_types.h"
#include "ai/ai_settings.h"

#include <nlohmann/json.hpp>
#include <functional>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

class McpApi;

class AiClient {
public:
    using DoneCallback = std::function<void()>;

    AiClient(McpApi* api);
    ~AiClient();

    void SendChatMessage(const std::vector<ChatMessage>& history,
                     const AiSettings& settings,
                     DoneCallback onDone);

    void Cancel();

    bool IsProcessing() const;

    std::vector<ChatMessage> DrainCompletedMessages(size_t& consumedCount);
    ChatMessage GetStreamingMessage() const;

private:
    struct ToolCallAccumulator {
        std::string id;
        std::string name;
        std::string argumentsJson;
    };

    size_t PushPendingMessage(ChatMessage msg);
    ChatMessage& GetPendingMessage(size_t idx);
    void ClearPendingMessages();

    void ProcessLoop(std::vector<ChatMessage> history,
                     AiSettings settings,
                     DoneCallback onDone);

    void ProcessLoopInner(std::vector<ChatMessage>& history,
                          AiSettings& settings);

    nlohmann::json BuildOpenAIRequest(const std::vector<ChatMessage>& history,
                                       const AiSettings& settings,
                                       const nlohmann::json& tools);
    nlohmann::json BuildAnthropicRequest(const std::vector<ChatMessage>& history,
                                          const AiSettings& settings,
                                          const nlohmann::json& tools);

    nlohmann::json CallOpenAI(const std::string& endpoint, const std::string& apiKey,
                           const std::string& body, size_t streamingIdx);
    nlohmann::json CallAnthropic(const std::string& endpoint, const std::string& apiKey,
                               const std::string& body, size_t streamingIdx);

    nlohmann::json ConvertToolsToOpenAI() const;
    nlohmann::json ConvertToolsToAnthropic() const;

    std::string ExecuteTool(const std::string& name, const nlohmann::json& args);

    McpApi* api_;
    std::atomic<bool> processing_{false};
    std::atomic<bool> cancel_{false};
    mutable std::mutex messagesMutex_;
    std::vector<ChatMessage> pendingMessages_;
};
