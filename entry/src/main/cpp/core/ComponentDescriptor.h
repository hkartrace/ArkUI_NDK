#pragma once

#include "Property.h"
#include "PropertySchema.h"
#include "ComponentRegistry.h"
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

class ComponentDescriptor {
public:
    using Listener = std::function<void(ComponentDescriptor*, const std::string&)>;

    static std::unique_ptr<ComponentDescriptor> Create(const std::string& type);

    const std::string& GetId() const { return id_; }
    const std::string& GetType() const { return componentType_; }

    void SetProperty(const std::string& name, PropertyValue value);
    const PropertyValue* GetProperty(const std::string& name) const;
    PropertyValue GetPropertyOrDefault(const std::string& name) const;

    void AddChild(std::unique_ptr<ComponentDescriptor> child, int32_t index = -1);
    std::unique_ptr<ComponentDescriptor> RemoveChild(int32_t index);
    ComponentDescriptor* GetChild(int32_t index);
    const ComponentDescriptor* GetChild(int32_t index) const;
    int32_t ChildCount() const { return static_cast<int32_t>(children_.size()); }

    ComponentDescriptor* GetParent() { return parent_; }
    const ComponentDescriptor* GetParent() const { return parent_; }

    void AddListener(Listener listener);

private:
    explicit ComponentDescriptor(const std::string& type);

    std::string id_;
    std::string componentType_;
    std::map<std::string, PropertyValue> properties_;
    std::vector<std::unique_ptr<ComponentDescriptor>> children_;
    std::vector<Listener> listeners_;
    ComponentDescriptor* parent_ = nullptr;

    void NotifyListeners(const std::string& propertyName);
};
