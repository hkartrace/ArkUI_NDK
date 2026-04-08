#pragma once

#include "PropertySchema.h"
#include <map>
#include <string>
#include <vector>

class ComponentRegistry {
public:
    static ComponentRegistry& Instance();

    const ComponentSchema* GetSchema(const std::string& type) const;
    const std::vector<std::string>& GetRegisteredTypes() const;

private:
    ComponentRegistry();
    ComponentRegistry(const ComponentRegistry&) = delete;
    ComponentRegistry& operator=(const ComponentRegistry&) = delete;

    std::map<std::string, ComponentSchema> schemas_;
    std::vector<std::string> typeOrder_;

    void AddCommonProperties(ComponentSchema& schema);
    void RegisterColumn();
    void RegisterRow();
    void RegisterText();
    void RegisterButton();
    void RegisterImage();
};
