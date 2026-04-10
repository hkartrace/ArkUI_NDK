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

    ArkUI_NodeHandle rootCol = nodeApi->createNode(ARKUI_NODE_COLUMN);
    if (!rootCol) {
        OH_LOG_ERROR(LOG_APP, "Failed to create root Column");
        return;
    }
    rootNdkNode_ = rootCol;

    ArkUI_NumberValue pctW = {.f32 = 1.0f};
    ArkUI_AttributeItem pctWItem = {&pctW, 1};
    nodeApi->setAttribute(rootCol, NODE_WIDTH_PERCENT, &pctWItem);

    ArkUI_NumberValue pctH = {.f32 = 1.0f};
    ArkUI_AttributeItem pctHItem = {&pctH, 1};
    nodeApi->setAttribute(rootCol, NODE_HEIGHT_PERCENT, &pctHItem);

    ArkUI_NumberValue bgVal = {.u32 = 0xFF484848};
    ArkUI_AttributeItem bgItem = {&bgVal, 1};
    nodeApi->setAttribute(rootCol, NODE_BACKGROUND_COLOR, &bgItem);
    
    ArkUI_NumberValue justifyVal = {.i32 = ARKUI_FLEX_ALIGNMENT_CENTER};
    ArkUI_AttributeItem justifyItem = {&justifyVal, 1};
    nodeApi->setAttribute(rootCol, NODE_COLUMN_JUSTIFY_CONTENT, &justifyItem);
    
    ArkUI_NumberValue alignVal = {.i32 = ARKUI_HORIZONTAL_ALIGNMENT_CENTER};
    ArkUI_AttributeItem alignItem = {&alignVal, 1};
    nodeApi->setAttribute(rootCol, NODE_COLUMN_ALIGN_ITEMS, &alignItem);

    ArkUI_NumberValue padVal = {.f32 = 20.0f};
    ArkUI_AttributeItem padItem = {&padVal, 1};
    nodeApi->setAttribute(rootCol, NODE_PADDING, &padItem);
    
    ArkUI_NodeHandle textNode = nodeApi->createNode(ARKUI_NODE_TEXT);
    if (textNode) {
        ArkUI_AttributeItem contentItem = {nullptr, 0, "Hello from ArkUI NDK!", nullptr};
        nodeApi->setAttribute(textNode, NODE_TEXT_CONTENT, &contentItem);

        ArkUI_NumberValue fsVal = {.f32 = 24.0f};
        ArkUI_AttributeItem fsItem = {&fsVal, 1};
        nodeApi->setAttribute(textNode, NODE_FONT_SIZE, &fsItem);

        ArkUI_NumberValue fcVal = {.u32 = 0xFF333333};
        ArkUI_AttributeItem fcItem = {&fcVal, 1};
        nodeApi->setAttribute(textNode, NODE_FONT_COLOR, &fcItem);

        ArkUI_NumberValue twVal = {.f32 = 300.0f};
        ArkUI_AttributeItem twItem = {&twVal, 1};
        nodeApi->setAttribute(textNode, NODE_WIDTH, &twItem);

        ArkUI_NumberValue thVal = {.f32 = 60.0f};
        ArkUI_AttributeItem thItem = {&thVal, 1};
        nodeApi->setAttribute(textNode, NODE_HEIGHT, &thItem);

        ArkUI_NumberValue tbgVal = {.u32 = 0xFFE0E0E0};
        ArkUI_AttributeItem tbgItem = {&tbgVal, 1};
        nodeApi->setAttribute(textNode, NODE_BACKGROUND_COLOR, &tbgItem);

        ArkUI_NumberValue alignCenter = {.i32 = ARKUI_TEXT_ALIGNMENT_CENTER};
        ArkUI_AttributeItem alignCenterItem = {&alignCenter, 1};
        nodeApi->setAttribute(textNode, NODE_TEXT_ALIGN, &alignCenterItem);

        nodeApi->addChild(rootCol, textNode);
        OH_LOG_INFO(LOG_APP, "Text node added");
    }

    ArkUI_NodeHandle btnNode = nodeApi->createNode(ARKUI_NODE_BUTTON);
    if (btnNode) {
        ArkUI_AttributeItem labelItem = {nullptr, 0, "Click Me", nullptr};
        nodeApi->setAttribute(btnNode, NODE_BUTTON_LABEL, &labelItem);

        ArkUI_NumberValue btnBg = {.u32 = 0xFF007DFF};
        ArkUI_AttributeItem btnBgItem = {&btnBg, 1};
        nodeApi->setAttribute(btnNode, NODE_BACKGROUND_COLOR, &btnBgItem);

        ArkUI_NumberValue bwVal = {.f32 = 200.0f};
        ArkUI_AttributeItem bwItem = {&bwVal, 1};
        nodeApi->setAttribute(btnNode, NODE_WIDTH, &bwItem);

        ArkUI_NumberValue bhVal = {.f32 = 50.0f};
        ArkUI_AttributeItem bhItem = {&bhVal, 1};
        nodeApi->setAttribute(btnNode, NODE_HEIGHT, &bhItem);

        nodeApi->addChild(rootCol, btnNode);
        OH_LOG_INFO(LOG_APP, "Button node added");
    }


    OH_ArkUI_NodeContent_AddNode(contentHandle_, rootCol);

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
