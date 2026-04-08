#pragma once

#include "core/ComponentDescriptor.h"
#include <string>

class JsonSerializer {
public:
    static std::string Serialize(const ComponentDescriptor& root);
    static std::unique_ptr<ComponentDescriptor> Deserialize(const std::string& json);
};
