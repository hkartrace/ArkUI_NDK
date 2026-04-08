#include "core/ComponentRegistry.h"

ComponentRegistry& ComponentRegistry::Instance() {
    static ComponentRegistry instance;
    return instance;
}

ComponentRegistry::ComponentRegistry() {
    RegisterColumn();
    RegisterRow();
    RegisterText();
    RegisterButton();
    RegisterImage();
}

const ComponentSchema* ComponentRegistry::GetSchema(const std::string& type) const {
    auto it = schemas_.find(type);
    return it != schemas_.end() ? &it->second : nullptr;
}

const std::vector<std::string>& ComponentRegistry::GetRegisteredTypes() const {
    return typeOrder_;
}

void ComponentRegistry::AddCommonProperties(ComponentSchema& schema) {
    schema.properties.push_back({"width", PropertyType::FLOAT, 0, 0.0f, "vp", 0.0f, 100000.0f, {}, false});
    schema.properties.push_back({"height", PropertyType::FLOAT, 1, 0.0f, "vp", 0.0f, 100000.0f, {}, false});
    schema.properties.push_back({"widthPercent", PropertyType::FLOAT, 71, 0.0f, "%", 0.0f, 100.0f, {}, false});
    schema.properties.push_back({"heightPercent", PropertyType::FLOAT, 72, 0.0f, "%", 0.0f, 100.0f, {}, false});
    schema.properties.push_back({"backgroundColor", PropertyType::COLOR, 2, static_cast<uint32_t>(0x00000000), "", 0.0f, 0.0f, {}});
    schema.properties.push_back({"padding", PropertyType::EDGE, 4, EdgeValues{}, "vp", 0.0f, 100000.0f, {}});
    schema.properties.push_back({"margin", PropertyType::EDGE, 7, EdgeValues{}, "vp", 0.0f, 100000.0f, {}});
    schema.properties.push_back({"opacity", PropertyType::FLOAT, 16, 1.0f, "", 0.0f, 1.0f, {}});
    schema.properties.push_back({"borderWidth", PropertyType::EDGE, 17, EdgeValues{}, "vp", 0.0f, 100000.0f, {}});
    schema.properties.push_back({"borderRadius", PropertyType::FLOAT, 18, 0.0f, "vp", 0.0f, 100000.0f, {}});
    schema.properties.push_back({"borderColor", PropertyType::COLOR, 19, static_cast<uint32_t>(0xFF000000), "", 0.0f, 0.0f, {}});
    schema.properties.push_back({"visibility", PropertyType::ENUM, 22, static_cast<int32_t>(0), "", 0.0f, 0.0f,
        {{"VISIBLE", 0, "Visible"}, {"HIDDEN", 1, "Hidden"}, {"NONE", 2, "None"}}
    });
    schema.properties.push_back({"enabled", PropertyType::BOOL, 6, true, "", 0.0f, 0.0f, {}});
}

void ComponentRegistry::RegisterColumn() {
    ComponentSchema schema;
    schema.name = "Column";
    schema.ndkType = 1006;
    schema.canHaveChildren = true;
    AddCommonProperties(schema);
    schema.properties.push_back({"alignItems", PropertyType::ENUM, 1006000, static_cast<int32_t>(0), "", 0.0f, 0.0f,
        {{"START", 0, "Start"}, {"CENTER", 1, "Center"}, {"END", 2, "End"}}
    });
    schema.properties.push_back({"justifyContent", PropertyType::ENUM, 1006001, static_cast<int32_t>(1), "", 0.0f, 0.0f,
        {{"START", 1, "Start"}, {"CENTER", 2, "Center"}, {"END", 3, "End"},
         {"SPACE_BETWEEN", 6, "Space Between"}, {"SPACE_AROUND", 7, "Space Around"},
         {"SPACE_EVENLY", 8, "Space Evenly"}}
    });
    schemas_["Column"] = std::move(schema);
    typeOrder_.push_back("Column");
}

void ComponentRegistry::RegisterRow() {
    ComponentSchema schema;
    schema.name = "Row";
    schema.ndkType = 1007;
    schema.canHaveChildren = true;
    AddCommonProperties(schema);
    schema.properties.push_back({"alignItems", PropertyType::ENUM, 1007000, static_cast<int32_t>(1), "", 0.0f, 0.0f,
        {{"TOP", 0, "Top"}, {"CENTER", 1, "Center"}, {"BOTTOM", 2, "Bottom"}}
    });
    schema.properties.push_back({"justifyContent", PropertyType::ENUM, 1007001, static_cast<int32_t>(1), "", 0.0f, 0.0f,
        {{"START", 1, "Start"}, {"CENTER", 2, "Center"}, {"END", 3, "End"},
         {"SPACE_BETWEEN", 6, "Space Between"}, {"SPACE_AROUND", 7, "Space Around"},
         {"SPACE_EVENLY", 8, "Space Evenly"}}
    });
    schemas_["Row"] = std::move(schema);
    typeOrder_.push_back("Row");
}

void ComponentRegistry::RegisterText() {
    ComponentSchema schema;
    schema.name = "Text";
    schema.ndkType = 1;
    schema.canHaveChildren = false;
    AddCommonProperties(schema);
    schema.properties.push_back({"content", PropertyType::STRING, 1000, std::string(""), "", 0.0f, 0.0f, {}});
    schema.properties.push_back({"fontSize", PropertyType::FLOAT, 1002, 16.0f, "fp", 1.0f, 1000.0f, {}});
    schema.properties.push_back({"fontColor", PropertyType::COLOR, 1001, static_cast<uint32_t>(0xFF000000), "", 0.0f, 0.0f, {}});
    schema.properties.push_back({"fontWeight", PropertyType::ENUM, 1004, static_cast<int32_t>(10), "", 0.0f, 0.0f,
        {{"W100", 0, "100"}, {"W200", 1, "200"}, {"W300", 2, "300"}, {"W400", 3, "400"},
         {"W500", 4, "500"}, {"W600", 5, "600"}, {"W700", 6, "700"}, {"W800", 7, "800"},
         {"W900", 8, "900"}, {"BOLD", 9, "Bold"}, {"NORMAL", 10, "Normal"},
         {"BOLDER", 11, "Bolder"}, {"LIGHTER", 12, "Lighter"}}
    });
    schema.properties.push_back({"textAlign", PropertyType::ENUM, 1010, static_cast<int32_t>(0), "", 0.0f, 0.0f,
        {{"START", 0, "Start"}, {"CENTER", 1, "Center"}, {"END", 2, "End"}, {"JUSTIFY", 3, "Justify"}}
    });
    schema.properties.push_back({"maxLines", PropertyType::INT, 1009, static_cast<int32_t>(0), "", 0.0f, 10000.0f, {}});
    schema.properties.push_back({"overflow", PropertyType::ENUM, 1011, static_cast<int32_t>(0), "", 0.0f, 0.0f,
        {{"NONE", 0, "None"}, {"CLIP", 1, "Clip"}, {"ELLIPSIS", 2, "Ellipsis"}, {"MARQUEE", 3, "Marquee"}}
    });
    schemas_["Text"] = std::move(schema);
    typeOrder_.push_back("Text");
}

void ComponentRegistry::RegisterButton() {
    ComponentSchema schema;
    schema.name = "Button";
    schema.ndkType = 9;
    schema.canHaveChildren = false;
    AddCommonProperties(schema);
    schema.properties.push_back({"label", PropertyType::STRING, 9000, std::string(""), "", 0.0f, 0.0f, {}});
    schema.properties.push_back({"buttonType", PropertyType::ENUM, 9001, static_cast<int32_t>(0), "", 0.0f, 0.0f,
        {{"CAPSULE", 0, "Capsule"}, {"CIRCLE", 1, "Circle"}, {"NORMAL", 2, "Normal"}}
    });
    schemas_["Button"] = std::move(schema);
    typeOrder_.push_back("Button");
}

void ComponentRegistry::RegisterImage() {
    ComponentSchema schema;
    schema.name = "Image";
    schema.ndkType = 4;
    schema.canHaveChildren = false;
    AddCommonProperties(schema);
    schema.properties.push_back({"src", PropertyType::STRING, 4000, std::string(""), "", 0.0f, 0.0f, {}});
    schema.properties.push_back({"objectFit", PropertyType::ENUM, 4001, static_cast<int32_t>(1), "", 0.0f, 0.0f,
        {{"CONTAIN", 0, "Contain"}, {"COVER", 1, "Cover"}, {"AUTO", 2, "Auto"},
         {"FILL", 3, "Fill"}, {"SCALE_DOWN", 4, "Scale Down"}, {"NONE", 5, "None"}}
    });
    schema.properties.push_back({"alt", PropertyType::STRING, 4006, std::string(""), "", 0.0f, 0.0f, {}});
    schemas_["Image"] = std::move(schema);
    typeOrder_.push_back("Image");
}
