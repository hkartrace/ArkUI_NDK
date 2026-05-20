#include "ai/ai_chat_panel.h"
#include "ai/markdown_renderer.h"
#include "mcp/mcp_api.h"
#include "utils/main_thread_dispatcher.h"

#include "imgui.h"

#include <cstring>

AiChatPanel::AiChatPanel()
{
    settings_ = AiSettings::Load();
    hasUnsavedSettings_ = settings_.apiKey.empty();
    inputBuffer_[0] = '\0';
}

AiChatPanel::~AiChatPanel() = default;

void AiChatPanel::Init(EditorWindow* editorWindow)
{
    api_ = std::make_unique<McpApi>(editorWindow);
    client_ = std::make_unique<AiClient>(api_.get());
}

void AiChatPanel::Draw()
{
    if (!client_) return;

    if (showSettings_) {
        ImGui::OpenPopup("AI Settings");
        showSettings_ = false;
    }

    DrawSettingsPopup();

    if (!settings_.apiKey.empty()) {
        DrawMessages();
        DrawInputBar();
    } else {
        ImGui::Spacing();
        ImGui::TextDisabled("Configure your AI provider to get started.");
        ImGui::Spacing();
        if (ImGui::Button("Open AI Settings")) {
            showSettings_ = true;
        }
    }
}

void AiChatPanel::DrawMessages()
{
    auto completed = client_->DrainCompletedMessages(consumedCount_);
    for (auto& msg : completed) {
        messages_.push_back(std::move(msg));
        scrollToBottom_ = true;
    }

    ChatMessage streaming = client_->GetStreamingMessage();

    ImGui::BeginChild("##chat_messages", ImVec2(0, -40), true, ImGuiWindowFlags_None);

    for (size_t i = 0; i < messages_.size(); ++i) {
        const auto& msg = messages_[i];
        if (msg.role == ChatRole::User) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
            ImGui::TextWrapped("You: %s", msg.content.c_str());
            ImGui::PopStyleColor();
        } else if (msg.role == ChatRole::Assistant) {
            if (!msg.thinking.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                if (ImGui::TreeNode(("think_" + std::to_string(i)).c_str(), "Thinking...")) {
                    ImGui::TextWrapped("%s", msg.thinking.c_str());
                    ImGui::TreePop();
                }
                ImGui::PopStyleColor();
            }

            if (!msg.content.empty()) {
                MarkdownRenderer::Render(msg.content);
            }

            if (!msg.toolCalls.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.4f, 1.0f));
                for (const auto& tc : msg.toolCalls) {
                    if (ImGui::TreeNode(("tool_" + tc.id + "_" + std::to_string(i)).c_str(), "Tool: %s", tc.name.c_str())) {
                        ImGui::Text("Args: %s", tc.arguments.dump(2).c_str());
                        ImGui::Separator();
                        ImGui::TextWrapped("Result: %s", tc.result.substr(0, 500).c_str());
                        if (tc.result.size() > 500)
                            ImGui::Text("... (%zu more chars)", tc.result.size() - 500);
                        ImGui::TreePop();
                    }
                }
                ImGui::PopStyleColor();
            }
        } else if (msg.role == ChatRole::ToolResult) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.4f, 1.0f));
            for (const auto& tc : msg.toolCalls) {
                if (ImGui::TreeNode(("tool_" + tc.id + "_" + std::to_string(i)).c_str(), "Tool: %s", tc.name.c_str())) {
                    ImGui::Text("Args: %s", tc.arguments.dump(2).c_str());
                    ImGui::Separator();
                    ImGui::TextWrapped("Result: %s", tc.result.substr(0, 500).c_str());
                    if (tc.result.size() > 500)
                        ImGui::Text("... (%zu more chars)", tc.result.size() - 500);
                    ImGui::TreePop();
                }
            }
            ImGui::PopStyleColor();
        }

        ImGui::Spacing();
    }

    if (streaming.role == ChatRole::Assistant && (streaming.isStreaming || !streaming.content.empty())) {
        if (!streaming.thinking.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
            if (ImGui::TreeNode("think_streaming", "Thinking...")) {
                ImGui::TextWrapped("%s", streaming.thinking.c_str());
                ImGui::TreePop();
            }
            ImGui::PopStyleColor();
        }

        if (!streaming.content.empty()) {
            MarkdownRenderer::Render(streaming.content);
        }

        if (!streaming.toolCalls.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.8f, 0.4f, 1.0f));
            for (const auto& tc : streaming.toolCalls) {
                if (ImGui::TreeNode(("tool_streaming_" + tc.id).c_str(), "Tool: %s", tc.name.c_str())) {
                    ImGui::Text("Args: %s", tc.arguments.dump(2).c_str());
                    ImGui::Separator();
                    ImGui::TextWrapped("Result: %s", tc.result.substr(0, 500).c_str());
                    if (tc.result.size() > 500)
                        ImGui::Text("... (%zu more chars)", tc.result.size() - 500);
                    ImGui::TreePop();
                }
            }
            ImGui::PopStyleColor();
        }

        scrollToBottom_ = true;
    }

    if (client_->IsProcessing()) {
        ImGui::TextDisabled("AI is thinking...");
    }

    if (scrollToBottom_) {
        ImGui::SetScrollHereY(1.0f);
        scrollToBottom_ = false;
    }

    ImGui::EndChild();
}

void AiChatPanel::DrawInputBar()
{
    bool processing = client_->IsProcessing();

    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 80);
    bool enterPressed = ImGui::InputTextWithHint("##chat_input", "Type a message...",
        inputBuffer_, sizeof(inputBuffer_),
        ImGuiInputTextFlags_EnterReturnsTrue | (processing ? ImGuiInputTextFlags_ReadOnly : 0));
    ImGui::PopItemWidth();

    ImGui::SameLine();

    if (processing) {
        if (ImGui::Button("Cancel", ImVec2(60, 0))) {
            client_->Cancel();
        }
    }

    if (enterPressed && !processing && inputBuffer_[0] != '\0') {
        SendMessage();
    }
}

void AiChatPanel::DrawSettingsPopup()
{
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420, 0));

    if (!ImGui::BeginPopupModal("AI Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }

    if (!hasUnsavedSettings_) {
        settingsTemp_ = settings_;
        hasUnsavedSettings_ = true;
    }

    const auto& presets = AiSettings::GetProviderPresets();

    int currentProvider = 0;
    for (int i = 0; i < (int)presets.size(); ++i) {
        if (presets[i].id == settingsTemp_.provider) {
            currentProvider = i;
            break;
        }
    }

    if (ImGui::BeginCombo("Provider", presets[currentProvider].displayName.c_str())) {
        for (int i = 0; i < (int)presets.size(); ++i) {
            if (ImGui::Selectable(presets[i].displayName.c_str(), currentProvider == i)) {
                currentProvider = i;
                settingsTemp_.provider = presets[i].id;
                if (!presets[i].models.empty()) {
                    settingsTemp_.model = presets[i].models[0].modelId;
                }
            }
        }
        ImGui::EndCombo();
    }

    auto& currentPreset = presets[currentProvider];

    int currentModel = 0;
    for (int i = 0; i < (int)currentPreset.models.size(); ++i) {
        if (currentPreset.models[i].modelId == settingsTemp_.model) {
            currentModel = i;
            break;
        }
    }

    if (ImGui::BeginCombo("Model", currentPreset.models[currentModel].name.c_str())) {
        for (int i = 0; i < (int)currentPreset.models.size(); ++i) {
            if (ImGui::Selectable(currentPreset.models[i].name.c_str(), currentModel == i)) {
                currentModel = i;
                settingsTemp_.model = currentPreset.models[i].modelId;
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();

    char apiKeyBuf[256] = {};
    strncpy(apiKeyBuf, settingsTemp_.apiKey.c_str(), sizeof(apiKeyBuf) - 1);
    if (ImGui::InputText("API Key", apiKeyBuf, sizeof(apiKeyBuf), ImGuiInputTextFlags_Password)) {
        settingsTemp_.apiKey = apiKeyBuf;
    }

    if (settingsTemp_.provider == "custom") {
        char endpointBuf[512] = {};
        strncpy(endpointBuf, settingsTemp_.customEndpoint.c_str(), sizeof(endpointBuf) - 1);
        if (ImGui::InputText("Endpoint", endpointBuf, sizeof(endpointBuf))) {
            settingsTemp_.customEndpoint = endpointBuf;
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Apply", ImVec2(120, 0))) {
        settings_ = settingsTemp_;
        if (settings_.provider != "custom") {
            settings_.customEndpoint.clear();
        }
        AiSettings::Save(settings_);
        hasUnsavedSettings_ = false;
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
        hasUnsavedSettings_ = false;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void AiChatPanel::SendMessage()
{
    std::string text(inputBuffer_);
    inputBuffer_[0] = '\0';

    ChatMessage userMsg;
    userMsg.role = ChatRole::User;
    userMsg.content = text;
    messages_.push_back(userMsg);
    scrollToBottom_ = true;

    std::vector<ChatMessage> history = messages_;

    client_->SendChatMessage(history, settings_, [this]() {
        scrollToBottom_ = true;
    });
}
