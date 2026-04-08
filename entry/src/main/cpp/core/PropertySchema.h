#pragma once

#include "Property.h"
#include <cfloat>
#include <string>
#include <vector>

struct PropertySchema {
    std::string name;
    PropertyType type;
    int32_t ndkAttribute;
    PropertyValue defaultValue;
    std::string unit;
    float min = 0.0f;
    float max = FLT_MAX;
    std::vector<EnumOption> enumOptions;
    bool initializeDefault = true;

    const EnumOption* FindEnumByValue(int32_t value) const {
        for (const auto& opt : enumOptions) {
            if (opt.value == value) return &opt;
        }
        return nullptr;
    }
};

struct ComponentSchema {
    std::string name;
    int32_t ndkType;
    bool canHaveChildren;
    std::vector<PropertySchema> properties;

    const PropertySchema* FindProperty(const std::string& propName) const {
        for (const auto& p : properties) {
            if (p.name == propName) return &p;
        }
        return nullptr;
    }
};
