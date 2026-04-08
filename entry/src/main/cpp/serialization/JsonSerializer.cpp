#include "serialization/JsonSerializer.h"
#include <sstream>

static std::string EscapeJson(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

static std::string PropertyValueToJson(const PropertyValue& val, PropertyType type) {
    switch (type) {
        case PropertyType::FLOAT: {
            char buf[64];
            snprintf(buf, sizeof(buf), "%g", std::get<float>(val));
            return buf;
        }
        case PropertyType::INT:
            return std::to_string(std::get<int32_t>(val));
        case PropertyType::COLOR:
            return std::to_string(std::get<uint32_t>(val));
        case PropertyType::STRING:
            return "\"" + EscapeJson(std::get<std::string>(val)) + "\"";
        case PropertyType::ENUM:
            return std::to_string(std::get<int32_t>(val));
        case PropertyType::BOOL:
            return std::get<bool>(val) ? "true" : "false";
        case PropertyType::EDGE: {
            const auto& e = std::get<EdgeValues>(val);
            char buf[128];
            snprintf(buf, sizeof(buf), "[%g,%g,%g,%g]", e.top, e.right, e.bottom, e.left);
            return buf;
        }
    }
    return "null";
}

static std::string Indent(int level) {
    return std::string(level * 2, ' ');
}

std::string JsonSerializer::Serialize(const ComponentDescriptor& root) {
    auto schema = ComponentRegistry::Instance().GetSchema(root.GetType());
    if (!schema) return "{}";

    std::ostringstream ss;
    ss << "{\n";
    ss << Indent(1) << "\"id\": \"" << root.GetId() << "\",\n";
    ss << Indent(1) << "\"type\": \"" << root.GetType() << "\",\n";
    ss << Indent(1) << "\"properties\": {\n";

    bool first = true;
    for (const auto& propSchema : schema->properties) {
        auto val = root.GetProperty(propSchema.name);
        if (val) {
            if (!first) ss << ",\n";
            first = false;
            ss << Indent(2) << "\"" << propSchema.name << "\": " << PropertyValueToJson(*val, propSchema.type);
        }
    }
    ss << "\n" << Indent(1) << "}";

    if (root.ChildCount() > 0) {
        ss << ",\n" << Indent(1) << "\"children\": [\n";
        for (int32_t i = 0; i < root.ChildCount(); i++) {
            if (i > 0) ss << ",\n";
            std::string childJson = Serialize(*root.GetChild(i));
            std::string indented;
            std::istringstream cis(childJson);
            std::string line;
            bool firstLine = true;
            while (std::getline(cis, line)) {
                if (!firstLine) indented += "\n" + Indent(2) + line;
                else { indented = line; firstLine = false; }
            }
            ss << Indent(2) << indented;
        }
        ss << "\n" << Indent(1) << "]";
    }

    ss << "\n}";
    return ss.str();
}

std::unique_ptr<ComponentDescriptor> JsonSerializer::Deserialize(const std::string& json) {
    return nullptr;
}
