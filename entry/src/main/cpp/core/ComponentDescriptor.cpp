#include "core/ComponentDescriptor.h"
#include <algorithm>
#include <random>

static std::string GenerateId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;
    uint64_t val = dist(gen);
    char buf[17];
    snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(val));
    return std::string(buf);
}

ComponentDescriptor::ComponentDescriptor(const std::string& type)
    : id_(GenerateId()), componentType_(type) {}

std::unique_ptr<ComponentDescriptor> ComponentDescriptor::Create(const std::string& type) {
    auto schema = ComponentRegistry::Instance().GetSchema(type);
    if (!schema) return nullptr;
    auto desc = std::unique_ptr<ComponentDescriptor>(new ComponentDescriptor(type));
    for (const auto& prop : schema->properties) {
        if (prop.initializeDefault) {
            desc->properties_[prop.name] = prop.defaultValue;
        }
    }
    return desc;
}

void ComponentDescriptor::SetProperty(const std::string& name, PropertyValue value) {
    properties_[name] = std::move(value);
    NotifyListeners(name);
}

const PropertyValue* ComponentDescriptor::GetProperty(const std::string& name) const {
    auto it = properties_.find(name);
    return it != properties_.end() ? &it->second : nullptr;
}

PropertyValue ComponentDescriptor::GetPropertyOrDefault(const std::string& name) const {
    auto it = properties_.find(name);
    if (it != properties_.end()) return it->second;
    auto schema = ComponentRegistry::Instance().GetSchema(componentType_);
    if (schema) {
        auto propSchema = schema->FindProperty(name);
        if (propSchema) return propSchema->defaultValue;
    }
    return PropertyValue{};
}

void ComponentDescriptor::AddChild(std::unique_ptr<ComponentDescriptor> child, int32_t index) {
    child->parent_ = this;
    if (index < 0 || index >= static_cast<int32_t>(children_.size())) {
        children_.push_back(std::move(child));
    } else {
        children_.insert(children_.begin() + index, std::move(child));
    }
    NotifyListeners("children");
}

std::unique_ptr<ComponentDescriptor> ComponentDescriptor::RemoveChild(int32_t index) {
    if (index < 0 || index >= static_cast<int32_t>(children_.size())) return nullptr;
    auto child = std::move(children_[index]);
    child->parent_ = nullptr;
    children_.erase(children_.begin() + index);
    NotifyListeners("children");
    return child;
}

ComponentDescriptor* ComponentDescriptor::GetChild(int32_t index) {
    if (index < 0 || index >= static_cast<int32_t>(children_.size())) return nullptr;
    return children_[index].get();
}

const ComponentDescriptor* ComponentDescriptor::GetChild(int32_t index) const {
    if (index < 0 || index >= static_cast<int32_t>(children_.size())) return nullptr;
    return children_[index].get();
}

void ComponentDescriptor::AddListener(Listener listener) {
    listeners_.push_back(std::move(listener));
}

void ComponentDescriptor::NotifyListeners(const std::string& propertyName) {
    for (auto& listener : listeners_) {
        listener(this, propertyName);
    }
}
