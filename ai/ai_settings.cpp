#include "ai_settings.h"
#include "utils/file_system.h"
#include "utils/log.h"

#include <nlohmann/json.hpp>
#include <fstream>

fs::path AiSettings::GetSettingsPath()
{
    return fs::path(GetLocalDataDirectory()) / "ai_settings.json";
}

AiSettings AiSettings::Load()
{
    AiSettings settings;
    auto path = GetSettingsPath();

    if (!fs::exists(path)) {
        return settings;
    }

    try {
        std::ifstream file(path);
        if (!file) return settings;

        auto j = nlohmann::json::parse(file);
        if (j.contains("provider")) settings.provider = j["provider"].get<std::string>();
        if (j.contains("model")) settings.model = j["model"].get<std::string>();
        if (j.contains("apiKey")) settings.apiKey = j["apiKey"].get<std::string>();
        if (j.contains("customEndpoint")) settings.customEndpoint = j["customEndpoint"].get<std::string>();
    } catch (const std::exception& e) {
        LOGE("Failed to load AI settings: %s", e.what());
    }

    return settings;
}

void AiSettings::Save(const AiSettings& settings)
{
    auto path = GetSettingsPath();

    try {
        std::ofstream file(path);
        if (!file) {
            LOGE("Failed to save AI settings: cannot open %s", path.string().c_str());
            return;
        }

        nlohmann::json j;
        j["provider"] = settings.provider;
        j["model"] = settings.model;
        j["apiKey"] = settings.apiKey;
        j["customEndpoint"] = settings.customEndpoint;
        file << j.dump(2);
    } catch (const std::exception& e) {
        LOGE("Failed to save AI settings: %s", e.what());
    }
}

const std::vector<AiProviderPreset>& AiSettings::GetProviderPresets()
{
    static const std::vector<AiProviderPreset> presets = {
        {
            "openai", "OpenAI",
            "https://api.openai.com/v1/chat/completions",
            {
                {"GPT-4o", "gpt-4o"},
                {"GPT-4o Mini", "gpt-4o-mini"},
                {"GPT-4.1", "gpt-4.1"},
                {"GPT-4.1 Mini", "gpt-4.1-mini"},
                {"GPT-4.1 Nano", "gpt-4.1-nano"}
            }
        },
        {
            "anthropic", "Anthropic",
            "https://api.anthropic.com/v1/messages",
            {
                {"Claude Sonnet 4", "claude-sonnet-4-20250514"},
                {"Claude Haiku 4", "claude-haiku-4-20250514"}
            }
        },
        {
            "zai", "Z.AI Coding Plan",
            "https://api.z.ai/api/coding/paas/v4/chat/completions",
            {
                {"GLM-5.1", "glm-5.1"}
            }
        },
        {
            "custom", "Custom (OpenAI-compatible)",
            "",
            {
                {"Custom Model", "custom"}
            }
        }
    };
    return presets;
}

std::string AiSettings::GetEndpoint(const AiSettings& settings)
{
    for (const auto& preset : GetProviderPresets()) {
        if (preset.id == settings.provider && !preset.defaultEndpoint.empty()) {
            return preset.defaultEndpoint;
        }
    }

    if (!settings.customEndpoint.empty()) {
        return settings.customEndpoint;
    }

    return "";
}
