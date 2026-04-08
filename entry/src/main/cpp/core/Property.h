#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <variant>

enum class PropertyType {
    FLOAT,
    INT,
    COLOR,
    STRING,
    ENUM,
    BOOL,
    EDGE,
};

struct EdgeValues {
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    float left = 0.0f;

    bool operator==(const EdgeValues& other) const {
        return top == other.top && right == other.right && bottom == other.bottom && left == other.left;
    }
    bool operator!=(const EdgeValues& other) const { return !(*this == other); }
};

using PropertyValue = std::variant<float, int32_t, uint32_t, std::string, bool, EdgeValues>;

struct EnumOption {
    std::string name;
    int32_t value;
    std::string displayName;
};
