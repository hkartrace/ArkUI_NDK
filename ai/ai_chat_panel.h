#pragma once

#include "ai/ai_client.h"
#include "ai/ai_settings.h"
#include "ai/chat_types.h"

#include <vector>
#include <memory>
#include <string>

class EditorWindow;

class AiChatPanel {
public:
    AiChatPanel();
    ~AiChatPanel();

    void Init(EditorWindow* editorWindow);
    void Draw();
    void ShowSettings() { showSettings_ = true; }
    bool HasApiKey() const { return !settings_.apiKey.empty(); }

private:
    void DrawMessages();
    void DrawInputBar();
    void DrawSettingsPopup();
    void SendMessage();

    std::vector<ChatMessage> messages_;
    char inputBuffer_[2048] = {};
    bool scrollToBottom_ = false;
    size_t consumedCount_ = 0;

    std::unique_ptr<class McpApi> api_;
    std::unique_ptr<AiClient> client_;
    AiSettings settings_;
    AiSettings settingsTemp_;
    bool showSettings_ = false;
    bool hasUnsavedSettings_ = false;
};
