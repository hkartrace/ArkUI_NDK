#pragma once

#include "core/ComponentDescriptor.h"
#include "rendering/NodeFactory.h"
#include "arkui/native_type.h"
#include "arkui/native_node.h"
#include "arkui/native_node_napi.h"
#include <memory>

class EditorApp {
public:
    void Initialize(napi_env env, napi_value content);
    void CreateSampleTree();

private:
    void SyncDescriptorToNode(ComponentDescriptor* desc, ArkUI_NodeHandle parentNdkNode);
    void OnDescriptorChanged(ComponentDescriptor* desc, const std::string& property);

    napi_env env_ = nullptr;
    ArkUI_NodeContentHandle contentHandle_ = nullptr;
    ArkUI_NodeHandle rootNdkNode_ = nullptr;
    NodeFactory nodeFactory_;
    std::unique_ptr<ComponentDescriptor> rootDescriptor_;
    std::map<std::string, ArkUI_NodeHandle> descriptorToNode_;
};
