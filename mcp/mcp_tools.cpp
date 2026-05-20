#include "mcp/mcp_tools.h"
#include "mcp/mcp_api.h"

#include <functional>
#include <unordered_map>

nlohmann::json GetToolDefinitions()
{
    return nlohmann::json::array({
        {
            {"name", "list_graph_nodes"},
            {"description", "List all nodes in the current effect graph with IDs, types, names, positions, and pin IDs"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        },
        {
            {"name", "get_graph"},
            {"description", "Get the full effect graph as a JSON object including nodes, links, and embedded shaders"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        },
        {
            {"name", "load_graph"},
            {"description", "Load an effect graph from a JSON string, replacing the current graph"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"json", {{"type", "string"}, {"description", "JSON string of the graph to load"}}}
                }},
                {"required", {"json"}}
            }}
        },
        {
            {"name", "add_shader_node"},
            {"description", "Add a shader node to the effect graph by name. The shader must already be registered (either built-in or created via create_effect). Use list_library to see available shaders."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"name", {{"type", "string"}, {"description", "Name of the registered shader to add as a node"}}},
                    {"x", {{"type", "number"}, {"description", "X position on the graph canvas"}, {"default", 0}}},
                    {"y", {{"type", "number"}, {"description", "Y position on the graph canvas"}, {"default", 0}}}
                }},
                {"required", {"name"}}
            }}
        },
        {
            {"name", "add_filter_node"},
            {"description", "Add a built-in filter node to the effect graph (e.g. EdgeLight, MesaBlur, FrostedGlass, WaterRipple, HeatDistortion, Grey, AuroraNoise, DistortionCollapse, SDFFromImage, SDFEdgeLight)"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"filterName", {{"type", "string"}, {"description", "Name of the filter to add"}}},
                    {"x", {{"type", "number"}, {"description", "X position on the graph canvas"}, {"default", 0}}},
                    {"y", {{"type", "number"}, {"description", "Y position on the graph canvas"}, {"default", 0}}}
                }},
                {"required", {"filterName"}}
            }}
        },
        {
            {"name", "add_subgraph_node"},
            {"description", "Add a subgraph node to the effect graph by name. The subgraph must already exist in the SubGraph library. Use list_library to see available subgraphs."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"name", {{"type", "string"}, {"description", "Name of the registered subgraph to add as a node"}}},
                    {"x", {{"type", "number"}, {"description", "X position on the graph canvas"}, {"default", 0}}},
                    {"y", {{"type", "number"}, {"description", "Y position on the graph canvas"}, {"default", 0}}}
                }},
                {"required", {"name"}}
            }}
        },
        {
            {"name", "delete_node"},
            {"description", "Delete a node from the effect graph by its ID"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"nodeId", {{"type", "integer"}, {"description", "ID of the node to delete"}}}
                }},
                {"required", {"nodeId"}}
            }}
        },
        {
            {"name", "select_node"},
            {"description", "Select a node in the effect graph by its ID"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"nodeId", {{"type", "integer"}, {"description", "ID of the node to select"}}}
                }},
                {"required", {"nodeId"}}
            }}
        },
        {
            {"name", "get_selected_node"},
            {"description", "Get details of the currently selected node in the effect graph"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        },
        {
            {"name", "get_node_shader"},
            {"description", "Get the GLSL fragment shader code of a node"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"nodeId", {{"type", "integer"}, {"description", "ID of the node"}}}
                }},
                {"required", {"nodeId"}}
            }}
        },
        {
            {"name", "update_node_shader"},
            {"description", "Update the GLSL shader code of an existing node. The shader is live-compiled immediately. Use this to modify shader parameters (uniforms, @minmax ranges, logic) on a node already in the graph."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"nodeId", {{"type", "integer"}, {"description", "ID of the node"}}},
                    {"glslCode", {{"type", "string"}, {"description", "New GLSL fragment shader code"}}}
                }},
                {"required", {"nodeId", "glslCode"}}
            }}
        },
        {
            {"name", "connect_nodes"},
            {"description", "Create a link between an output pin and an input pin of two nodes"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"startPinId", {{"type", "integer"}, {"description", "ID of the output pin (source)"}}},
                    {"endPinId", {{"type", "integer"}, {"description", "ID of the input pin (destination)"}}}
                }},
                {"required", {"startPinId", "endPinId"}}
            }}
        },
        {
            {"name", "disconnect_link"},
            {"description", "Remove a link between two nodes by link ID"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"linkId", {{"type", "integer"}, {"description", "ID of the link to remove"}}}
                }},
                {"required", {"linkId"}}
            }}
        },
        {
            {"name", "get_node_properties"},
            {"description", "Get the uniform parameters/properties of a node"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"nodeId", {{"type", "integer"}, {"description", "ID of the node"}}}
                }},
                {"required", {"nodeId"}}
            }}
        },
        {
            {"name", "set_node_property"},
            {"description", "Set a uniform parameter value on a node. Supported types: float, int, vec2 (array of 2), vec3 (array of 3), vec4 (array of 4)."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"nodeId", {{"type", "integer"}, {"description", "ID of the node"}}},
                    {"name", {{"type", "string"}, {"description", "Name of the uniform parameter"}}},
                    {"value", {{"description", "New value (number, or array of 2/3/4 numbers for vectors)"}}}
                }},
                {"required", {"nodeId", "name", "value"}}
            }}
        },
        {
            {"name", "list_library"},
            {"description", "List all available shaders and filters in the Content Library that can be used with add_shader_node or add_filter_node."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        },
        {
            {"name", "create_effect"},
            {"description", "Register a new shader effect from GLSL code. This makes the effect available in the content library so it can be added to the graph via add_shader_node. Use update_node_shader to modify an existing node's shader code."},
            {"inputSchema", {
                {"type", "object"},
                {"properties", {
                    {"name", {{"type", "string"}, {"description", "Name for the new effect"}}},
                    {"glslCode", {{"type", "string"}, {"description", "GLSL fragment shader code"}}}
                }},
                {"required", {"name", "glslCode"}}
            }}
        },
        {
            {"name", "save_scene"},
            {"description", "Save the current scene and effect graph to a JSON object"},
            {"inputSchema", {
                {"type", "object"},
                {"properties", nlohmann::json::object()},
                {"required", nlohmann::json::array()}
            }}
        }
    });
}

static nlohmann::json TextResult(const std::string& text, bool isError = false)
{
    return {
        {"content", nlohmann::json::array({{{"type", "text"}, {"text", text}}})},
        {"isError", isError}
    };
}

static nlohmann::json JsonResult(const nlohmann::json& data, bool isError = false)
{
    return TextResult(data.dump(2), isError);
}

using ToolFn = std::function<nlohmann::json(const nlohmann::json&, McpApi&)>;

static const std::unordered_map<std::string, ToolFn>& GetToolMap()
{
    static const std::unordered_map<std::string, ToolFn> tools = {
        {"list_graph_nodes", [](const nlohmann::json&, McpApi& api) {
            return JsonResult(api.ListNodes());
        }},
        {"get_graph", [](const nlohmann::json&, McpApi& api) {
            return JsonResult(api.GetGraph());
        }},
        {"load_graph", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.LoadGraph(params["json"].get<std::string>());
            return TextResult(ok ? "Graph loaded successfully" : "Failed to load graph", !ok);
        }},
        {"add_shader_node", [](const nlohmann::json& params, McpApi& api) {
            float x = params.value("x", 0.0f);
            float y = params.value("y", 0.0f);
            auto result = api.AddShaderNode(params["name"].get<std::string>(), x, y);
            if (result.is_null() || result.empty()) {
                return TextResult("Failed to add shader node", true);
            }
            return JsonResult(result);
        }},
        {"add_filter_node", [](const nlohmann::json& params, McpApi& api) {
            float x = params.value("x", 0.0f);
            float y = params.value("y", 0.0f);
            auto result = api.AddFilterNode(params["filterName"].get<std::string>(), x, y);
            if (result.is_null() || result.empty()) {
                return TextResult("Failed to add filter node. Check that the filter name is valid.", true);
            }
            return JsonResult(result);
        }},
        {"add_subgraph_node", [](const nlohmann::json& params, McpApi& api) {
            float x = params.value("x", 0.0f);
            float y = params.value("y", 0.0f);
            auto result = api.AddSubGraphNode(params["name"].get<std::string>(), x, y);
            if (result.is_null() || result.empty()) {
                return TextResult("Failed to add subgraph node. Check that the subgraph name is valid.", true);
            }
            return JsonResult(result);
        }},
        {"delete_node", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.DeleteNode(params["nodeId"].get<int>());
            return TextResult(ok ? "Node deleted" : "Node not found", !ok);
        }},
        {"select_node", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.SelectNode(params["nodeId"].get<int>());
            return TextResult(ok ? "Node selected" : "Failed to select node", !ok);
        }},
        {"get_selected_node", [](const nlohmann::json&, McpApi& api) {
            return JsonResult(api.GetSelectedNode());
        }},
        {"get_node_shader", [](const nlohmann::json& params, McpApi& api) {
            std::string code = api.GetNodeShader(params["nodeId"].get<int>());
            if (code.empty()) return TextResult("Node not found or has no shader", true);
            return TextResult(code);
        }},
        {"update_node_shader", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.UpdateNodeShader(params["nodeId"].get<int>(), params["glslCode"].get<std::string>());
            return TextResult(ok ? "Shader updated and compiled" : "Failed to update shader", !ok);
        }},
        {"connect_nodes", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.ConnectNodes(params["startPinId"].get<int>(), params["endPinId"].get<int>());
            return TextResult(ok ? "Nodes connected" : "Failed to connect nodes", !ok);
        }},
        {"disconnect_link", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.DisconnectLink(params["linkId"].get<int>());
            return TextResult(ok ? "Link removed" : "Link not found", !ok);
        }},
        {"get_node_properties", [](const nlohmann::json& params, McpApi& api) {
            return JsonResult(api.GetNodeProperties(params["nodeId"].get<int>()));
        }},
        {"set_node_property", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.SetNodeProperty(
                params["nodeId"].get<int>(),
                params["name"].get<std::string>(),
                params["value"]);
            return TextResult(ok ? "Property set" : "Failed to set property", !ok);
        }},
        {"list_library", [](const nlohmann::json&, McpApi& api) {
            return JsonResult(api.ListLibrary());
        }},
        {"create_effect", [](const nlohmann::json& params, McpApi& api) {
            bool ok = api.CreateEffect(params["name"].get<std::string>(), params["glslCode"].get<std::string>());
            return TextResult(ok ? "Effect created" : "Failed to create effect", !ok);
        }},
        {"save_scene", [](const nlohmann::json&, McpApi& api) {
            return JsonResult(api.SaveScene());
        }}
    };
    return tools;
}

nlohmann::json ExecuteTool(const std::string& name, const nlohmann::json& params, McpApi& api)
{
    try {
        const auto& tools = GetToolMap();
        auto it = tools.find(name);
        if (it != tools.end()) {
            return it->second(params, api);
        }
        return TextResult("Unknown tool: " + name, true);
    } catch (const std::exception& e) {
        return TextResult(std::string("Error: ") + e.what(), true);
    }
}

std::string GetMcpInstructions()
{
    return
"AuroraGraph shader effect graph editor. Use tools to create, modify, and query shader nodes, effects, and graphs.\n"
"\n"
"## Shader Authoring\n"
"\n"
"When creating effects with `create_effect`, GLSL fragment shaders must follow this format:\n"
"\n"
"```glsl\n"
"#version 320 es\n"
"// @author YourName\n"
"// @category Effect\n"
"// @description One-line description\n"
"// @revision 2026-01-01 00:00:00\n"
"precision highp float;\n"
"\n"
"layout(location = 0) in highp vec4 uv_coords;\n"
"layout(location = 0) out highp vec4 out_color;\n"
"\n"
"// For each texture input, declare a sampler2D AND a matching mat3 with 'Mat' suffix:\n"
"uniform sampler2D inputTex;     // @typeTexture\n"
"uniform mat3 inputTexMat;       // @imagemat\n"
"\n"
"uniform vec2 iResolution;\n"
"uniform float iTime;\n"
"\n"
"// Expose uniforms to the UI with annotations:\n"
"// uniform float myParam;  // @minmax{ 0.0, 1.0 } @init { 0.5 }\n"
"\n"
"void main() {\n"
"    vec2 uv = uv_coords.xy;\n"
"    vec4 color = texture(inputTex, (inputTexMat * vec3(uv, 1.0)).xy);\n"
"    out_color = color;\n"
"}\n"
"```\n"
"\n"
"## Workflow\n"
"\n"
"1. `create_effect(name, glslCode)` - registers a new shader effect in the library.\n"
"2. `add_shader_node(name, x, y)` - adds the effect as a node. Returns full node JSON with populated pins.\n"
"3. `connect_nodes(startPinId, endPinId)` - link an output pin to an input pin.\n"
"4. Use `list_graph_nodes` to discover existing nodes and their pins.\n"
"5. Use `update_node_shader` to modify a node's GLSL code live.\n"
"6. Use `set_node_property` to change uniform values.\n"
"\n"
"## Pin & Connection Semantics\n"
"\n"
"- Each node has `inputPins` (receives texture data) and `outputPins` (sends result).\n"
"- Every shader has one output pin named `out_color`.\n"
"- Each `sampler2D` uniform creates one input pin named after the uniform.\n"
"- `connect_nodes(startPinId, endPinId)`: startPinId must be an OUTPUT pin, endPinId must be an INPUT pin.\n"
"- Use pin `name` to determine which input to connect when a node has multiple inputs.\n"
"\n"
"## Key Notes\n"
"\n"
"- The `// @typeTexture` and `// @imagemat` annotations are required for texture inputs.\n"
"- The `// @minmax{ min, max }` and `// @init { value }` annotations expose uniforms as UI parameters.\n"
"- Use `list_library` to discover available shaders and filters before adding nodes.\n"
"\n"
"## Built-in Filters\n"
"\n"
"Filters are pre-built multi-pass GPU processing nodes. Add them with `add_filter_node`.\n"
"Unlike custom shader effects, filters have specific input/output channel requirements.\n"
"\n"
"### Common Pipelines\n"
"\n"
"**SDF Edge Light** (most common - 2 nodes):\n"
"  Source image -> [inputTex] -> SDFFromImage -> [sdfImage] -> SDFEdgeLight\n"
"  Source image -> [image] -> SDFEdgeLight (required)\n"
"  Light mask (R=intensity) -> [lightMask] -> SDFEdgeLight (required)\n"
"\n"
"**Frosted Glass** (2 nodes):\n"
"  Source image -> [inputTex] -> SDFFromImage -> [sdfTexture] -> FrostedGlass\n"
"\n"
"**Simple single-input**: MesaBlur / EdgeLight / WaterRipple / HeatDistortion / Grey / DistortionCollapse\n"
"**Generator (no input)**: AuroraNoise\n"
"\n"
"### Pin Channel Formats\n"
"\n"
"- SDFFromImage input: Alpha channel = shape mask (0=outside, 1=inside). RGB ignored.\n"
"- SDFFromImage output: Alpha = normalized distance (0.5=edge).\n"
"- SDFEdgeLight lightMask: R channel = light intensity (0=off, 1=full).\n"
"- FrostedGlass sdfTexture: Must come from SDFFromImage output.\n";
}
