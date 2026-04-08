#include "editor/EditorApp.h"
#include "hilog/log.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xFF00
#define LOG_TAG "NDKEditor"

void EditorApp::Initialize(napi_env env, napi_value content) {
    env_ = env;
    int32_t ret = OH_ArkUI_GetNodeContentFromNapiValue(env, content, &contentHandle_);
    if (ret != 0 || !contentHandle_) {
        OH_LOG_ERROR(LOG_APP, "Failed to get NodeContent: %{public}d", ret);
        return;
    }
    OH_LOG_INFO(LOG_APP, "EditorApp initialized");
}

void EditorApp::CreateSampleTree() {
    if (!contentHandle_) {
        OH_LOG_ERROR(LOG_APP, "CreateSampleTree: no contentHandle_");
        return;
    }

    auto nodeApi = nodeFactory_.GetApi();

    ArkUI_NodeHandle list = nodeApi->createNode(ARKUI_NODE_LIST);
    if (!list) {
        OH_LOG_ERROR(LOG_APP, "Failed to create List");
        return;
    }
    rootNdkNode_ = list;

    ArkUI_NumberValue pctW = {.f32 = 100.0f};
    ArkUI_AttributeItem pctWItem = {&pctW, 1};
    nodeApi->setAttribute(list, NODE_WIDTH_PERCENT, &pctWItem);

    ArkUI_NumberValue pctH = {.f32 = 100.0f};
    ArkUI_AttributeItem pctHItem = {&pctH, 1};
    nodeApi->setAttribute(list, NODE_HEIGHT_PERCENT, &pctHItem);

    ArkUI_NumberValue scrollOn = {.i32 = ARKUI_SCROLL_BAR_DISPLAY_MODE_ON};
    ArkUI_AttributeItem scrollItem = {&scrollOn, 1};
    nodeApi->setAttribute(list, NODE_SCROLL_BAR_DISPLAY_MODE, &scrollItem);

    for (int32_t i = 0; i < 30; ++i) {
        ArkUI_NodeHandle listItem = nodeApi->createNode(ARKUI_NODE_LIST_ITEM);

        ArkUI_NodeHandle textNode = nodeApi->createNode(ARKUI_NODE_TEXT);

        std::string text = std::to_string(i);
        ArkUI_AttributeItem contentItem = {nullptr, 0, text.c_str(), nullptr};
        nodeApi->setAttribute(textNode, NODE_TEXT_CONTENT, &contentItem);

        ArkUI_NumberValue fsVal = {.f32 = 16.0f};
        ArkUI_AttributeItem fsItem = {&fsVal, 1};
        nodeApi->setAttribute(textNode, NODE_FONT_SIZE, &fsItem);

        ArkUI_NumberValue fcVal = {.u32 = 0xFFff00ff};
        ArkUI_AttributeItem fcItem = {&fcVal, 1};
        nodeApi->setAttribute(textNode, NODE_FONT_COLOR, &fcItem);

        ArkUI_NumberValue pctW2 = {.f32 = 100.0f};
        ArkUI_AttributeItem pctW2Item = {&pctW2, 1};
        nodeApi->setAttribute(textNode, NODE_WIDTH_PERCENT, &pctW2Item);

        ArkUI_NumberValue wVal = {.f32 = 300.0f};
        ArkUI_AttributeItem wItem = {&wVal, 1};
        nodeApi->setAttribute(textNode, NODE_WIDTH, &wItem);

        ArkUI_NumberValue hVal = {.f32 = 100.0f};
        ArkUI_AttributeItem hItem = {&hVal, 1};
        nodeApi->setAttribute(textNode, NODE_HEIGHT, &hItem);

        ArkUI_NumberValue bgVal = {.u32 = 0xFFfffacd};
        ArkUI_AttributeItem bgItem = {&bgVal, 1};
        nodeApi->setAttribute(textNode, NODE_BACKGROUND_COLOR, &bgItem);

        ArkUI_NumberValue alignVal = {.i32 = ARKUI_TEXT_ALIGNMENT_CENTER};
        ArkUI_AttributeItem alignItem = {&alignVal, 1};
        nodeApi->setAttribute(textNode, NODE_TEXT_ALIGN, &alignItem);

        nodeApi->addChild(listItem, textNode);
        nodeApi->addChild(list, listItem);
    }

    OH_ArkUI_NodeContent_AddNode(contentHandle_, list);

    OH_LOG_INFO(LOG_APP, "Sample tree created");
}

void EditorApp::SyncDescriptorToNode(ComponentDescriptor* desc, ArkUI_NodeHandle parentNdkNode) {
    ArkUI_NodeHandle ndkNode = nodeFactory_.CreateNode(*desc);
    if (!ndkNode) return;

    descriptorToNode_[desc->GetId()] = ndkNode;

    if (parentNdkNode) {
        nodeFactory_.AddChild(parentNdkNode, ndkNode, -1);
    }

    for (int32_t i = 0; i < desc->ChildCount(); i++) {
        SyncDescriptorToNode(desc->GetChild(i), ndkNode);
    }
}

void EditorApp::OnDescriptorChanged(ComponentDescriptor* desc, const std::string& property) {
    auto it = descriptorToNode_.find(desc->GetId());
    if (it == descriptorToNode_.end()) return;
    nodeFactory_.UpdateNode(it->second, *desc, property);
}
