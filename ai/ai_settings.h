#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

struct AiModelPreset {
    std::string name;
    std::string modelId;
};

struct AiProviderPreset {
    std::string id;
    std::string displayName;
    std::string defaultEndpoint;
    std::vector<AiModelPreset> models;
};

struct AiSettings {
    std::string provider = "openai";
    std::string model = "gpt-4o-mini";
    std::string apiKey;
    std::string customEndpoint;

    static AiSettings Load();
    static void Save(const AiSettings& settings);
    static fs::path GetSettingsPath();

    static const std::vector<AiProviderPreset>& GetProviderPresets();
    static std::string GetEndpoint(const AiSettings& settings);
};
