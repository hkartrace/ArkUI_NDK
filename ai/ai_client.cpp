#include "ai/ai_client.h"
#include "mcp/mcp_api.h"
#include "mcp/mcp_tools.h"
#include "utils/main_thread_dispatcher.h"
#include "utils/log.h"

#include <httplib.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <sstream>

AiClient::AiClient(McpApi* api)
    : api_(api)
{}

AiClient::~AiClient()
{
    Cancel();
}

void AiClient::Cancel()
{
    cancel_ = true;
}

bool AiClient::IsProcessing() const
{
    return processing_;
}

size_t AiClient::PushPendingMessage(ChatMessage msg)
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    pendingMessages_.push_back(std::move(msg));
    return pendingMessages_.size() - 1;
}

ChatMessage& AiClient::GetPendingMessage(size_t idx)
{
    return pendingMessages_[idx];
}

void AiClient::ClearPendingMessages()
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    pendingMessages_.clear();
}

std::vector<ChatMessage> AiClient::DrainCompletedMessages(size_t& consumedCount)
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    std::vector<ChatMessage> result;

    if (consumedCount > pendingMessages_.size()) {
        consumedCount = 0;
    }

    size_t drainUpTo = pendingMessages_.size();
    if (!pendingMessages_.empty() && pendingMessages_.back().isStreaming) {
        drainUpTo = pendingMessages_.size() - 1;
    }

    if (consumedCount < drainUpTo) {
        result.assign(
            pendingMessages_.begin() + consumedCount,
            pendingMessages_.begin() + drainUpTo
        );
        consumedCount = drainUpTo;
    }
    return result;
}

ChatMessage AiClient::GetStreamingMessage() const
{
    std::lock_guard<std::mutex> lock(messagesMutex_);
    if (!pendingMessages_.empty() && pendingMessages_.back().isStreaming) {
        return pendingMessages_.back();
    }
    return {};
}

void AiClient::SendChatMessage(const std::vector<ChatMessage>& history,
                            const AiSettings& settings,
                            DoneCallback onDone)
{
    if (processing_) return;
    cancel_ = false;
    processing_ = true;

    std::thread([this, history, settings, onDone = std::move(onDone)]() {
        ProcessLoop(history, settings, std::move(onDone));
    }).detach();
}

static void SplitEndpoint(const std::string& endpoint, std::string& baseUrl, std::string& path)
{
    auto schemeEnd = endpoint.find("://");
    if (schemeEnd != std::string::npos) {
        auto afterScheme = endpoint.substr(schemeEnd + 3);
        auto pathStart = afterScheme.find('/');
        if (pathStart != std::string::npos) {
            baseUrl = endpoint.substr(0, schemeEnd + 3 + pathStart);
            path = afterScheme.substr(pathStart);
        } else {
            baseUrl = endpoint;
            path = "/";
        }
    } else {
        baseUrl = endpoint;
        path = "/";
    }
}

void AiClient::ProcessLoop(std::vector<ChatMessage> history,
                            AiSettings settings,
                            DoneCallback onDone)
{
    ClearPendingMessages();

    try {
        ProcessLoopInner(history, settings);
    } catch (const std::exception& e) {
        ChatMessage errMsg;
        errMsg.role = ChatRole::Assistant;
        errMsg.content = std::string("[Error: ") + e.what() + "]";
        {
            std::lock_guard<std::mutex> lock(messagesMutex_);
            pendingMessages_.push_back(errMsg);
        }
    } catch (...) {
        ChatMessage errMsg;
        errMsg.role = ChatRole::Assistant;
        errMsg.content = "[Unknown error in AI processing]";
        {
            std::lock_guard<std::mutex> lock(messagesMutex_);
            pendingMessages_.push_back(errMsg);
        }
    }

    processing_ = false;
    if (onDone) {
        MainThreadDispatcher::Enqueue([onDone = std::move(onDone)]() {
            onDone();
        });
    }
}

void AiClient::ProcessLoopInner(std::vector<ChatMessage>& history,
                                 AiSettings& settings)
{
    const int maxToolRounds = 10;

    for (int round = 0; round < maxToolRounds && !cancel_; ++round) {
        std::string endpoint = AiSettings::GetEndpoint(settings);

        ChatMessage streamingMsg;
        streamingMsg.role = ChatRole::Assistant;
        streamingMsg.content = "";
        streamingMsg.thinking = "";
        streamingMsg.isStreaming = true;
        size_t streamingIdx = PushPendingMessage(std::move(streamingMsg));

        nlohmann::json response;

        if (settings.provider == "anthropic") {
            auto tools = ConvertToolsToAnthropic();
            auto reqBody = BuildAnthropicRequest(history, settings, tools);
            response = CallAnthropic(endpoint, settings.apiKey, reqBody.dump(), streamingIdx);
        } else {
            auto tools = ConvertToolsToOpenAI();
            auto reqBody = BuildOpenAIRequest(history, settings, tools);
            response = CallOpenAI(endpoint, settings.apiKey, reqBody.dump(), streamingIdx);
        }

        {
            std::lock_guard<std::mutex> lock(messagesMutex_);
            GetPendingMessage(streamingIdx).isStreaming = false;
        }

        if (cancel_) break;

        if (response.is_null()) {
            ChatMessage errMsg;
            errMsg.role = ChatRole::Assistant;
            errMsg.content = "[Error: Empty response from API. Check your API key and endpoint.]";
            {
                std::lock_guard<std::mutex> lock(messagesMutex_);
                pendingMessages_.push_back(errMsg);
            }
            break;
        }

        if (response.contains("error")) {
            ChatMessage errMsg;
            errMsg.role = ChatRole::Assistant;
            errMsg.content = "[API Error " + std::to_string(response.value("status", 0)) + ": " +
                             response["error"].get<std::string>().substr(0, 500) + "]";
            {
                std::lock_guard<std::mutex> lock(messagesMutex_);
                pendingMessages_.push_back(errMsg);
            }
            break;
        }

        ChatMessage assistantMsg;
        assistantMsg.role = ChatRole::Assistant;

        std::vector<ToolCallAccumulator> toolCalls;

        if (settings.provider == "anthropic") {
            for (const auto& block : response.value("content", nlohmann::json::array())) {
                if (block.value("type", "") == "text") {
                    assistantMsg.content += block.value("text", "");
                } else if (block.value("type", "") == "tool_use") {
                    ToolCallAccumulator tc;
                    tc.id = block.value("id", "");
                    tc.name = block.value("name", "");
                    tc.argumentsJson = block.value("input", nlohmann::json::object()).dump();
                    toolCalls.push_back(tc);
                }
            }
        } else {
            auto choices = response.value("choices", nlohmann::json::array());
            if (!choices.empty()) {
                auto msg = choices[0].value("message", nlohmann::json::object());
                assistantMsg.content = msg.value("content", "");
                assistantMsg.thinking = msg.value("reasoning_content", "");
                if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
                    for (const auto& tc : msg["tool_calls"]) {
                        ToolCallAccumulator acc;
                        auto func = tc.value("function", nlohmann::json::object());
                        acc.id = tc.value("id", "");
                        acc.name = func.value("name", "");
                        acc.argumentsJson = func.value("arguments", "");
                        toolCalls.push_back(acc);
                    }
                }
            }
        }

        if (toolCalls.empty()) {
            break;
        }

        {
            std::lock_guard<std::mutex> lock(messagesMutex_);
            auto& displayMsg = GetPendingMessage(streamingIdx);
            for (const auto& tc : toolCalls) {
                ToolCallInfo tci;
                tci.id = tc.id;
                tci.name = tc.name;
                try {
                    tci.arguments = nlohmann::json::parse(tc.argumentsJson);
                } catch (...) {
                    tci.arguments = nlohmann::json::object();
                }
                displayMsg.toolCalls.push_back(tci);
            }
        }

        history.push_back(assistantMsg);

        for (const auto& tc : toolCalls) {
            if (cancel_) break;

            ToolCallInfo info;
            info.id = tc.id;
            info.name = tc.name;
            try {
                info.arguments = nlohmann::json::parse(tc.argumentsJson);
            } catch (...) {
                info.arguments = nlohmann::json::object();
            }

            LOGD("AI Tool Call: %s(%s)", tc.name.c_str(), tc.argumentsJson.c_str());

            info.result = ExecuteTool(tc.name, info.arguments);
            info.isError = false;

            {
                std::lock_guard<std::mutex> lock(messagesMutex_);
                ChatMessage toolMsg;
                toolMsg.role = ChatRole::ToolResult;
                toolMsg.content = tc.name;
                toolMsg.toolCalls.push_back(info);
                pendingMessages_.push_back(toolMsg);
            }

            ChatMessage toolResultMsg;
            toolResultMsg.role = ChatRole::ToolResult;
            toolResultMsg.content = tc.name;
            toolResultMsg.toolCalls.push_back(info);
            history.push_back(toolResultMsg);
        }
    }
}

nlohmann::json AiClient::ConvertToolsToOpenAI() const
{
    auto mcpTools = GetToolDefinitions();
    nlohmann::json tools = nlohmann::json::array();
    for (const auto& t : mcpTools) {
        tools.push_back({
            {"type", "function"},
            {"function", {
                {"name", t["name"]},
                {"description", t["description"]},
                {"parameters", t.value("inputSchema", nlohmann::json::object())}
            }}
        });
    }
    return tools;
}

nlohmann::json AiClient::ConvertToolsToAnthropic() const
{
    auto mcpTools = GetToolDefinitions();
    nlohmann::json tools = nlohmann::json::array();
    for (const auto& t : mcpTools) {
        tools.push_back({
            {"name", t["name"]},
            {"description", t["description"]},
            {"input_schema", t.value("inputSchema", nlohmann::json::object())}
        });
    }
    return tools;
}

nlohmann::json AiClient::BuildOpenAIRequest(const std::vector<ChatMessage>& history,
                                              const AiSettings& settings,
                                              const nlohmann::json& tools)
{
    nlohmann::json messages = nlohmann::json::array();
    messages.push_back({{"role", "system"}, {"content", GetMcpInstructions()}});

    for (const auto& msg : history) {
        if (msg.role == ChatRole::User) {
            messages.push_back({{"role", "user"}, {"content", msg.content}});
        } else if (msg.role == ChatRole::Assistant) {
            nlohmann::json m = {{"role", "assistant"}, {"content", msg.content}};
            if (!msg.toolCalls.empty()) {
                nlohmann::json tcs = nlohmann::json::array();
                for (const auto& tc : msg.toolCalls) {
                    tcs.push_back({
                        {"id", tc.id},
                        {"type", "function"},
                        {"function", {{"name", tc.name}, {"arguments", tc.arguments.dump()}}}
                    });
                }
                m["tool_calls"] = tcs;
            }
            messages.push_back(m);
        } else if (msg.role == ChatRole::ToolResult) {
            for (const auto& tc : msg.toolCalls) {
                messages.push_back({
                    {"role", "tool"},
                    {"tool_call_id", tc.id},
                    {"content", tc.result}
                });
            }
        }
    }

    nlohmann::json req;
    req["model"] = settings.model;
    req["messages"] = messages;
    req["tools"] = tools;
    req["stream"] = true;
    return req;
}

nlohmann::json AiClient::BuildAnthropicRequest(const std::vector<ChatMessage>& history,
                                                 const AiSettings& settings,
                                                 const nlohmann::json& tools)
{
    nlohmann::json messages = nlohmann::json::array();

    for (const auto& msg : history) {
        if (msg.role == ChatRole::User) {
            messages.push_back({{"role", "user"}, {"content", msg.content}});
        } else if (msg.role == ChatRole::Assistant) {
            nlohmann::json content = nlohmann::json::array();
            if (!msg.content.empty()) {
                content.push_back({{"type", "text"}, {"text", msg.content}});
            }
            for (const auto& tc : msg.toolCalls) {
                content.push_back({
                    {"type", "tool_use"},
                    {"id", tc.id},
                    {"name", tc.name},
                    {"input", tc.arguments}
                });
            }
            messages.push_back({{"role", "assistant"}, {"content", content}});
        } else if (msg.role == ChatRole::ToolResult) {
            for (const auto& tc : msg.toolCalls) {
                nlohmann::json resultContent;
                try {
                    resultContent = nlohmann::json::parse(tc.result);
                } catch (...) {
                    resultContent = tc.result;
                }
                messages.push_back({
                    {"role", "user"},
                    {"content", nlohmann::json::array({
                        {
                            {"type", "tool_result"},
                            {"tool_use_id", tc.id},
                            {"content", resultContent}
                        }
                    })}
                });
            }
        }
    }

    nlohmann::json req;
    req["model"] = settings.model;
    req["max_tokens"] = 4096;
    req["system"] = GetMcpInstructions();
    req["messages"] = messages;
    req["tools"] = tools;
    req["stream"] = true;
    return req;
}

nlohmann::json AiClient::CallOpenAI(const std::string& endpoint, const std::string& apiKey,
                                      const std::string& body, size_t streamingIdx)
{
    std::string baseUrl, path;
    SplitEndpoint(endpoint, baseUrl, path);

    httplib::Client cli(baseUrl);
    cli.set_follow_location(true);
    cli.set_read_timeout(120);
    cli.enable_server_certificate_verification(false);

    LOGD("CallOpenAI: baseUrl='%s' path='%s'", baseUrl.c_str(), path.c_str());

    httplib::Request req;
    req.method = "POST";
    req.path = path;
    req.headers = {
        {"Authorization", "Bearer " + apiKey},
        {"Content-Type", "application/json"}
    };
    req.body = body;

    nlohmann::json assembled;
    assembled["choices"] = nlohmann::json::array();
    auto& choice = assembled["choices"].emplace_back(nlohmann::json::object());
    choice["message"] = nlohmann::json::object();
    choice["message"]["content"] = "";
    choice["message"]["reasoning_content"] = "";
    choice["message"]["tool_calls"] = nlohmann::json::array();
    bool hasToolCalls = false;
    std::map<int, ToolCallAccumulator> tcMap;
    std::string sseBuffer;

    req.content_receiver = [&](const char* data, size_t len, uint64_t, uint64_t) -> bool {
        if (cancel_) return false;
        sseBuffer.append(data, len);

        size_t pos;
        while ((pos = sseBuffer.find('\n')) != std::string::npos) {
            std::string line = sseBuffer.substr(0, pos);
            sseBuffer.erase(0, pos + 1);

            while (!line.empty() && (line.back() == '\r')) {
                line.pop_back();
            }

            if (line.substr(0, 6) != "data: ") continue;
            auto sseData = line.substr(6);
            if (sseData == "[DONE]") return true;

            try {
                auto chunk = nlohmann::json::parse(sseData);
                auto choices = chunk.value("choices", nlohmann::json::array());
                if (choices.empty()) continue;
                auto delta = choices[0].value("delta", nlohmann::json::object());

                if (delta.contains("content") && !delta["content"].is_null()) {
                    std::string text = delta["content"].get<std::string>();
                    choice["message"]["content"] = choice["message"]["content"].get<std::string>() + text;
                    std::lock_guard<std::mutex> lock(messagesMutex_);
                    GetPendingMessage(streamingIdx).content += text;
                }

                if (delta.contains("reasoning_content") && !delta["reasoning_content"].is_null()) {
                    std::string text = delta["reasoning_content"].get<std::string>();
                    choice["message"]["reasoning_content"] = choice["message"].value("reasoning_content", "") + text;
                    std::lock_guard<std::mutex> lock(messagesMutex_);
                    GetPendingMessage(streamingIdx).thinking += text;
                }

                if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
                    hasToolCalls = true;
                    for (const auto& tc : delta["tool_calls"]) {
                        int idx = tc.value("index", 0);
                        auto& acc = tcMap[idx];
                        if (tc.contains("id")) acc.id = tc["id"].get<std::string>();
                        if (tc.contains("function")) {
                            auto func = tc["function"];
                            if (func.contains("name") && !func["name"].is_null())
                                acc.name = func["name"].get<std::string>();
                            if (func.contains("arguments") && !func["arguments"].is_null())
                                acc.argumentsJson += func["arguments"].get<std::string>();
                        }
                    }
                }
            } catch (...) {}
        }
        return true;
    };

    auto res = cli.send(req);

    if (!res) {
        LOGE("OpenAI API request failed: %s", httplib::to_string(res.error()).c_str());
        return nlohmann::json();
    }

    if (res->status != 200) {
        LOGE("OpenAI API error %d: %s", res->status, res->body.substr(0, 1000).c_str());
        LOGD("OpenAI request body (first 500): %s", body.substr(0, 500).c_str());
        return nlohmann::json({{"error", res->body}, {"status", res->status}});
    }

    if (hasToolCalls) {
        for (auto& [idx, acc] : tcMap) {
            choice["message"]["tool_calls"].push_back({
                {"id", acc.id},
                {"type", "function"},
                {"function", {{"name", acc.name}, {"arguments", acc.argumentsJson}}}
            });
        }
    } else {
        choice["message"].erase("tool_calls");
    }

    if (!choice["message"].value("reasoning_content", "").empty()) {
    } else {
        choice["message"].erase("reasoning_content");
    }

    return assembled;
}

nlohmann::json AiClient::CallAnthropic(const std::string& endpoint, const std::string& apiKey,
                                         const std::string& body, size_t streamingIdx)
{
    std::string baseUrl, path;
    SplitEndpoint(endpoint, baseUrl, path);

    httplib::Client cli(baseUrl);
    cli.set_follow_location(true);
    cli.set_read_timeout(120);
    cli.enable_server_certificate_verification(false);

    LOGD("CallAnthropic: baseUrl='%s' path='%s'", baseUrl.c_str(), path.c_str());

    httplib::Request req;
    req.method = "POST";
    req.path = path;
    req.headers = {
        {"x-api-key", apiKey},
        {"anthropic-version", "2023-06-01"},
        {"Content-Type", "application/json"}
    };
    req.body = body;

    nlohmann::json assembled;
    assembled["content"] = nlohmann::json::array();
    std::string textContent;
    std::map<int, ToolCallAccumulator> tcMap;
    int currentToolIndex = -1;
    bool hasToolUse = false;
    std::string eventType;
    std::string sseBuffer;

    req.content_receiver = [&](const char* data, size_t len, uint64_t, uint64_t) -> bool {
        if (cancel_) return false;
        sseBuffer.append(data, len);

        size_t pos;
        while ((pos = sseBuffer.find('\n')) != std::string::npos) {
            std::string line = sseBuffer.substr(0, pos);
            sseBuffer.erase(0, pos + 1);

            while (!line.empty() && (line.back() == '\r')) {
                line.pop_back();
            }

            if (line.empty()) {
                eventType.clear();
                continue;
            }

            if (line.substr(0, 7) == "event: ") {
                eventType = line.substr(7);
                continue;
            }
            if (line.substr(0, 6) != "data: ") continue;
            auto sseData = line.substr(6);

            try {
                auto chunk = nlohmann::json::parse(sseData);

                if (eventType == "content_block_delta") {
                    auto delta = chunk.value("delta", nlohmann::json::object());
                    if (delta.value("type", "") == "text_delta") {
                        std::string text = delta.value("text", "");
                        textContent += text;
                        std::lock_guard<std::mutex> lock(messagesMutex_);
                        GetPendingMessage(streamingIdx).content += text;
                    } else if (delta.value("type", "") == "thinking_delta") {
                        std::string text = delta.value("thinking", "");
                        std::lock_guard<std::mutex> lock(messagesMutex_);
                        GetPendingMessage(streamingIdx).thinking += text;
                    } else if (delta.value("type", "") == "input_json_delta") {
                        std::string partialJson = delta.value("partial_json", "");
                        if (currentToolIndex >= 0) {
                            tcMap[currentToolIndex].argumentsJson += partialJson;
                        }
                    }
                } else if (eventType == "content_block_start") {
                    auto contentBlock = chunk.value("content_block", nlohmann::json::object());
                    if (contentBlock.value("type", "") == "tool_use") {
                        hasToolUse = true;
                        currentToolIndex = chunk.value("index", 0);
                        auto& acc = tcMap[currentToolIndex];
                        acc.id = contentBlock.value("id", "");
                        acc.name = contentBlock.value("name", "");
                        acc.argumentsJson = "";
                    }
                }
            } catch (...) {}
        }
        return true;
    };

    auto res = cli.send(req);

    if (!res) {
        LOGE("Anthropic API request failed: %s", httplib::to_string(res.error()).c_str());
        return nlohmann::json();
    }

    if (res->status != 200) {
        LOGE("Anthropic API error %d: %s", res->status, res->body.c_str());
        return nlohmann::json({{"error", res->body}, {"status", res->status}});
    }

    if (!textContent.empty()) {
        assembled["content"].push_back({{"type", "text"}, {"text", textContent}});
    }
    for (auto& [idx, acc] : tcMap) {
        nlohmann::json input;
        try {
            input = nlohmann::json::parse(acc.argumentsJson);
        } catch (...) {
            input = nlohmann::json::object();
        }
        assembled["content"].push_back({
            {"type", "tool_use"},
            {"id", acc.id},
            {"name", acc.name},
            {"input", input}
        });
    }

    return assembled;
}

std::string AiClient::ExecuteTool(const std::string& name, const nlohmann::json& args)
{
    auto result = ::ExecuteTool(name, args, *api_);

    if (result.contains("content") && result["content"].is_array() && !result["content"].empty()) {
        return result["content"][0].value("text", "");
    }
    return result.dump();
}
