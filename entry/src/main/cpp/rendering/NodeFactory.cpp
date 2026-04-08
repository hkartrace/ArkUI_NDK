#include "rendering/NodeFactory.h"

NodeFactory::NodeFactory() {
    OH_ArkUI_GetModuleInterface(ARKUI_NATIVE_NODE, ArkUI_NativeNodeAPI_1, nodeApi_);
}

ArkUI_NodeHandle NodeFactory::CreateNode(const ComponentDescriptor& desc) {
    auto schema = ComponentRegistry::Instance().GetSchema(desc.GetType());
    if (!schema || !nodeApi_) return nullptr;

    ArkUI_NodeHandle node = nodeApi_->createNode(static_cast<ArkUI_NodeType>(schema->ndkType));
    if (!node) return nullptr;

    ApplyAllProperties(node, desc);
    return node;
}

void NodeFactory::UpdateNode(ArkUI_NodeHandle node, const ComponentDescriptor& desc,
                              const std::string& changedProperty) {
    auto schema = ComponentRegistry::Instance().GetSchema(desc.GetType());
    if (!schema || !nodeApi_) return;

    auto propSchema = schema->FindProperty(changedProperty);
    if (!propSchema) return;

    auto val = desc.GetProperty(changedProperty);
    if (val) {
        ApplyProperty(node, *schema, changedProperty, *val);
    }
}

void NodeFactory::DestroyNode(ArkUI_NodeHandle node) {
    if (nodeApi_ && node) {
        nodeApi_->disposeNode(node);
    }
}

void NodeFactory::AddChild(ArkUI_NodeHandle parent, ArkUI_NodeHandle child, int32_t index) {
    if (!nodeApi_ || !parent || !child) return;

    if (index < 0) {
        nodeApi_->addChild(parent, child);
    } else {
        ArkUI_NodeHandle sibling = nullptr;
        if (index > 0) {
            nodeApi_->addChild(parent, child);
            return;
        }
        nodeApi_->insertChildBefore(parent, child, sibling);
    }
}

void NodeFactory::RemoveChild(ArkUI_NodeHandle parent, ArkUI_NodeHandle child) {
    if (nodeApi_ && parent && child) {
        nodeApi_->removeChild(parent, child);
    }
}

void NodeFactory::ApplyAllProperties(ArkUI_NodeHandle node, const ComponentDescriptor& desc) {
    auto schema = ComponentRegistry::Instance().GetSchema(desc.GetType());
    if (!schema || !nodeApi_) return;

    for (const auto& propSchema : schema->properties) {
        auto val = desc.GetProperty(propSchema.name);
        if (val) {
            ApplyProperty(node, *schema, propSchema.name, *val);
        }
    }
}

void NodeFactory::ApplyProperty(ArkUI_NodeHandle node, const ComponentSchema& schema,
                                 const std::string& name, const PropertyValue& value) {
    auto propSchema = schema.FindProperty(name);
    if (!propSchema) return;

    auto attr = static_cast<ArkUI_NodeAttributeType>(propSchema->ndkAttribute);

    switch (propSchema->type) {
        case PropertyType::FLOAT: {
            float v = std::get<float>(value);
            ArkUI_NumberValue numVal = {.f32 = v};
            ArkUI_AttributeItem item = {.value = &numVal, .size = 1};
            nodeApi_->setAttribute(node, attr, &item);
            break;
        }
        case PropertyType::INT: {
            int32_t v = std::get<int32_t>(value);
            ArkUI_NumberValue numVal = {.i32 = v};
            ArkUI_AttributeItem item = {.value = &numVal, .size = 1};
            nodeApi_->setAttribute(node, attr, &item);
            break;
        }
        case PropertyType::COLOR: {
            uint32_t v = std::get<uint32_t>(value);
            ArkUI_NumberValue numVal = {.u32 = v};
            ArkUI_AttributeItem item = {.value = &numVal, .size = 1};
            nodeApi_->setAttribute(node, attr, &item);
            break;
        }
        case PropertyType::STRING: {
            const std::string& v = std::get<std::string>(value);
            ArkUI_AttributeItem item = {.value = nullptr, .size = 0, .string = v.c_str(), .object = nullptr};
            nodeApi_->setAttribute(node, attr, &item);
            break;
        }
        case PropertyType::ENUM: {
            int32_t v = std::get<int32_t>(value);
            ArkUI_NumberValue numVal = {.i32 = v};
            ArkUI_AttributeItem item = {.value = &numVal, .size = 1};
            nodeApi_->setAttribute(node, attr, &item);
            break;
        }
        case PropertyType::BOOL: {
            bool v = std::get<bool>(value);
            ArkUI_NumberValue numVal = {.i32 = v ? 1 : 0};
            ArkUI_AttributeItem item = {.value = &numVal, .size = 1};
            nodeApi_->setAttribute(node, attr, &item);
            break;
        }
        case PropertyType::EDGE: {
            const EdgeValues& edge = std::get<EdgeValues>(value);
            ArkUI_NumberValue numVals[4] = {
                {.f32 = edge.top},
                {.f32 = edge.right},
                {.f32 = edge.bottom},
                {.f32 = edge.left}
            };
            ArkUI_AttributeItem item = {.value = numVals, .size = 4};
            nodeApi_->setAttribute(node, attr, &item);
            break;
        }
    }
}
