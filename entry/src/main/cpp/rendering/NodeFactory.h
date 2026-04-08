#pragma once

#include "core/ComponentDescriptor.h"
#include "arkui/native_interface.h"
#include "arkui/native_node.h"
#include "arkui/native_type.h"
#include <map>
#include <string>

class NodeFactory {
public:
    NodeFactory();

    ArkUI_NodeHandle CreateNode(const ComponentDescriptor& desc);
    void UpdateNode(ArkUI_NodeHandle node, const ComponentDescriptor& desc, const std::string& changedProperty);
    void DestroyNode(ArkUI_NodeHandle node);
    void AddChild(ArkUI_NodeHandle parent, ArkUI_NodeHandle child, int32_t index);
    void RemoveChild(ArkUI_NodeHandle parent, ArkUI_NodeHandle child);

    void ApplyAllProperties(ArkUI_NodeHandle node, const ComponentDescriptor& desc);

    ArkUI_NativeNodeAPI_1* GetApi() { return nodeApi_; }

private:
    ArkUI_NativeNodeAPI_1* nodeApi_ = nullptr;

    void ApplyProperty(ArkUI_NodeHandle node, const ComponentSchema& schema,
                       const std::string& name, const PropertyValue& value);
};
