#include "ai/markdown_renderer.h"
#include "imgui.h"

#include <sstream>
#include <vector>
#include <cstring>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Shellapi.h>
#endif

static void RenderCodeBlock(const std::string& code)
{
    std::string trimmed = code;
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r'))
        trimmed.pop_back();

    if (trimmed.empty()) return;

    int lineCount = 1;
    for (char c : trimmed) { if (c == '\n') ++lineCount; }

    float lineHeight = ImGui::GetTextLineHeightWithSpacing();
    float blockHeight = lineHeight * lineCount + 8.0f;
    ImVec2 avail = ImGui::GetContentRegionAvail();

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.14f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 2));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.82f, 0.58f, 1.0f));

    ImGui::BeginChild("##code_block", ImVec2(avail.x, blockHeight), ImGuiChildFlags_Borders);
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);

    std::istringstream stream(trimmed);
    std::string line;
    while (std::getline(stream, line)) {
        ImGui::TextUnformatted(line.c_str());
    }

    ImGui::EndChild();

    ImGui::PopStyleColor();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();
}

struct InlineSegment {
    enum Type { Text, Bold, Italic, InlineCode, LinkText };
    Type type;
    std::string text;
    std::string linkUrl;
};

static void ParseInlineSegments(const char* text, size_t len, std::vector<InlineSegment>& segments)
{
    size_t i = 0;
    std::string current;

    auto flush = [&]() {
        if (!current.empty()) {
            segments.push_back({InlineSegment::Text, current, {}});
            current.clear();
        }
    };

    while (i < len) {
        char c = text[i];

        if (c == '`') {
            flush();
            size_t end = i + 1;
            while (end < len && text[end] != '`') ++end;
            segments.push_back({InlineSegment::InlineCode, std::string(text + i + 1, end - i - 1), {}});
            i = end + 1;
            continue;
        }

        if (c == '*' && i + 1 < len && text[i + 1] == '*') {
            flush();
            size_t end = i + 2;
            while (end + 1 < len && !(text[end] == '*' && text[end + 1] == '*')) ++end;
            if (end + 1 < len) {
                segments.push_back({InlineSegment::Bold, std::string(text + i + 2, end - i - 2), {}});
                i = end + 2;
            } else {
                current += c;
                ++i;
            }
            continue;
        }

        if (c == '*' || c == '_') {
            flush();
            char sym = c;
            size_t end = i + 1;
            while (end < len && text[end] != sym) ++end;
            if (end < len && end > i + 1) {
                segments.push_back({InlineSegment::Italic, std::string(text + i + 1, end - i - 1), {}});
                i = end + 1;
            } else {
                current += c;
                ++i;
            }
            continue;
        }

        if (c == '[') {
            size_t bracketEnd = i + 1;
            while (bracketEnd < len && text[bracketEnd] != ']') ++bracketEnd;
            if (bracketEnd < len && bracketEnd + 1 < len && text[bracketEnd + 1] == '(') {
                flush();
                size_t parenStart = bracketEnd + 2;
                size_t parenEnd = parenStart;
                while (parenEnd < len && text[parenEnd] != ')') ++parenEnd;
                if (parenEnd < len) {
                    InlineSegment seg;
                    seg.type = InlineSegment::LinkText;
                    seg.text = std::string(text + i + 1, bracketEnd - i - 1);
                    seg.linkUrl = std::string(text + parenStart, parenEnd - parenStart);
                    segments.push_back(seg);
                    i = parenEnd + 1;
                    continue;
                }
            }
        }

        current += c;
        ++i;
    }
    flush();
}

static void RenderInline(const char* text, size_t len)
{
    std::vector<InlineSegment> segments;
    ParseInlineSegments(text, len, segments);

    bool first = true;
    for (const auto& seg : segments) {
        if (!first) ImGui::SameLine(0.0f, 0.0f);
        first = false;

        switch (seg.type) {
        case InlineSegment::Text:
            ImGui::TextWrapped("%s", seg.text.c_str());
            break;
        case InlineSegment::Bold:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", seg.text.c_str());
            ImGui::PopStyleColor();
            break;
        case InlineSegment::Italic:
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
            ImGui::TextWrapped("%s", seg.text.c_str());
            ImGui::PopStyleColor();
            break;
        case InlineSegment::InlineCode: {
            ImVec2 textSize = ImGui::CalcTextSize(seg.text.c_str());
            ImVec2 pos = ImGui::GetCursorScreenPos();
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float padding = 4.0f;
            if (textSize.x + padding * 2 > avail.x && !first) {
                ImGui::NewLine();
                pos = ImGui::GetCursorScreenPos();
            }
            ImGui::GetWindowDrawList()->AddRectFilled(
                ImVec2(pos.x - 2, pos.y - 1),
                ImVec2(pos.x + textSize.x + padding * 2 - 2, pos.y + textSize.y + 2),
                IM_COL32(50, 50, 55, 255), 2.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.78f, 0.82f, 0.58f, 1.0f));
            ImGui::TextUnformatted(seg.text.c_str());
            ImGui::PopStyleColor();
            break;
        }
        case InlineSegment::LinkText: {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.7f, 1.0f, 1.0f));
            ImGui::TextWrapped("%s", seg.text.c_str());
            ImGui::PopStyleColor();
            ImVec2 min = ImGui::GetItemRectMin();
            ImVec2 max = ImGui::GetItemRectMax();
            min.y = max.y;
            ImGui::GetWindowDrawList()->AddLine(min, max, IM_COL32(102, 178, 255, 200), 1.0f);

            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
#ifdef _WIN32
                ShellExecuteA(NULL, "open", seg.linkUrl.c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif
            }
            break;
        }
        }
    }

    if (segments.empty()) {
        ImGui::TextWrapped("");
    }
}

static void RenderInline(const std::string& text)
{
    RenderInline(text.c_str(), text.length());
}

void MarkdownRenderer::Render(const std::string& text)
{
    if (text.empty()) return;

    std::istringstream stream(text);
    std::string line;
    bool inCodeBlock = false;
    std::string codeBlockContent;

    while (std::getline(stream, line)) {
        if (line.size() >= 3 && line.compare(0, 3, "```") == 0) {
            if (inCodeBlock) {
                RenderCodeBlock(codeBlockContent);
                codeBlockContent.clear();
                inCodeBlock = false;
            } else {
                inCodeBlock = true;
                codeBlockContent.clear();
            }
            continue;
        }

        if (inCodeBlock) {
            codeBlockContent += line + "\n";
            continue;
        }

        if (line.empty()) {
            ImGui::NewLine();
            continue;
        }

        bool allSame = true;
        char firstChar = line[0];
        for (size_t j = 1; j < line.size(); ++j) {
            if (line[j] != firstChar) { allSame = false; break; }
        }
        if (line.size() >= 3 && (firstChar == '-' || firstChar == '*' || firstChar == '_') && allSame) {
            ImGui::Separator();
            ImGui::NewLine();
            continue;
        }

        int headingLevel = 0;
        if (line.size() > 4 && line[0] == '#' && line[1] == '#' && line[2] == '#' && line[3] == ' ')
            headingLevel = 3;
        else if (line.size() > 3 && line[0] == '#' && line[1] == '#' && line[2] == ' ')
            headingLevel = 2;
        else if (line.size() > 2 && line[0] == '#' && line[1] == ' ')
            headingLevel = 1;

        if (headingLevel > 0) {
            ImGui::NewLine();
            float scale = headingLevel == 1 ? 1.5f : (headingLevel == 2 ? 1.3f : 1.15f);
            ImGui::SetWindowFontScale(scale);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
            RenderInline(line.c_str() + headingLevel + 1, line.length() - headingLevel - 1);
            ImGui::PopStyleColor();
            ImGui::SetWindowFontScale(1.0f);
            if (headingLevel == 1) {
                ImGui::Separator();
            }
            ImGui::NewLine();
            continue;
        }

        if ((line.size() >= 2 && line[0] == '-' && line[1] == ' ') ||
            (line.size() >= 2 && line[0] == '*' && line[1] == ' ')) {
            ImGui::Bullet();
            RenderInline(line.c_str() + 2, line.length() - 2);
            continue;
        }

        if (line.size() >= 3 && line[0] >= '0' && line[0] <= '9' && line[1] == '.' && line[2] == ' ') {
            char label[8];
            snprintf(label, sizeof(label), "%c.", line[0]);
            ImGui::TextUnformatted(label);
            ImGui::SameLine();
            RenderInline(line.c_str() + 3, line.length() - 3);
            continue;
        }

        RenderInline(line);
    }

    if (inCodeBlock && !codeBlockContent.empty()) {
        RenderCodeBlock(codeBlockContent);
    }
}
