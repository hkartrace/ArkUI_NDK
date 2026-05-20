#include "mcp/mcp_api.h"

#include "editor/editor_window.h"
#include "editor/render_graph/shader_node_editor.h"
#include "editor/render_graph/node_graph.h"
#include "editor/render_graph/shader_node.h"
#include "editor/render_graph/filters/filter_node.h"
#include "editor/render_graph/filters/filter_library.h"
#include "editor/render_graph/filters/filter_base.h"
#include "editor/render_graph/sub_graph_node.h"
#include "editor/render_graph/sub_graph_library.h"
#include "editor/base_node_renderer.h"
#include "gl_helpers/material.h"
#include "gl_helpers/resource_manager.h"
#include "utils/main_thread_dispatcher.h"
#include "utils/log.h"
#include "utils/file_system.h"

#include <glm/glm.hpp>
#include <chrono>
#include <thread>

McpApi::McpApi(EditorWindow* editorWindow)
    : editorWnd_(editorWindow)
{}

nlohmann::json McpApi::ListNodes()
{
    LOGD("MCP ListNodes");
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this]() {
        nlohmann::json result = nlohmann::json::array();
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return result;
        for (const auto& node : editor->GetNodes()) {
            result.push_back(SerializeNode(node));
        }
        return result;
    });
}

nlohmann::json McpApi::GetGraph()
{
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return nlohmann::json::object();
        std::string jsonStr = editor->SaveGraphToString();
        auto parsed = nlohmann::json::parse(jsonStr, nullptr, false);
        if (parsed.is_discarded()) {
            return nlohmann::json::object();
        }
        return parsed;
    });
}

bool McpApi::LoadGraph(const std::string& json)
{
    return MainThreadDispatcher::DispatchSync<bool>([this, &json]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return false;
        return editor->LoadGraphFromString(json);
    });
}

nlohmann::json McpApi::AddShaderNode(const std::string& name, float x, float y)
{
    LOGD("MCP AddShaderNode: name=%s x=%.0f y=%.0f", name.c_str(), x, y);

    int nodeId = MainThreadDispatcher::DispatchSync<int>([this, &name, x, y]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor || !editor->graph_) return -1;

        auto node = editor->graph_->AddShaderNode(name);
        if (!node) return -1;

        if (auto renderer = editor->AddNodeRenderer(editor->graph_, node)) {
            renderer->position_ = ImVec2(x, y);
        }

        if (node->material_ && !node->material_->IsReady()) {
            editor->MarkNodeDirty(node);
        }

        return node->GetId();
    });

    if (nodeId < 0) return nlohmann::json::object();

    constexpr auto pollInterval = std::chrono::milliseconds(50);
    constexpr auto timeout = std::chrono::seconds(5);
    auto start = std::chrono::steady_clock::now();

    while (true) {
        std::this_thread::sleep_for(pollInterval);

        bool ready = MainThreadDispatcher::DispatchSync<bool>([this, nodeId]() {
            auto editor = editorWnd_->GetShaderNodeEditor();
            auto node = editor ? editor->GetNode(editor->GetNodes(), nodeId) : nullptr;
            return node && node->material_ && node->material_->IsReady();
        });

        if (ready) break;
        if (std::chrono::steady_clock::now() - start > timeout) break;
    }

    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this, nodeId]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        auto node = editor ? editor->GetNode(editor->GetNodes(), nodeId) : nullptr;
        if (!node) return nlohmann::json::object();
        return SerializeNode(node);
    });
}

nlohmann::json McpApi::AddFilterNode(const std::string& filterName, float x, float y)
{
    LOGD("MCP AddFilterNode: filterName=%s x=%.0f y=%.0f", filterName.c_str(), x, y);
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this, &filterName, x, y]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor || !editor->graph_) return nlohmann::json::object();

        auto& filterLib = editor->GetFilterLibrary();
        auto node = editor->graph_->AddFilterNode(filterLib, filterName);
        if (!node) return nlohmann::json::object();

        if (auto renderer = editor->AddNodeRenderer(editor->graph_, node)) {
            renderer->position_ = ImVec2(x, y);
        }

        return SerializeNode(node);
    });
}

nlohmann::json McpApi::AddSubGraphNode(const std::string& name, float x, float y)
{
    LOGD("MCP AddSubGraphNode: name=%s x=%.0f y=%.0f", name.c_str(), x, y);
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this, &name, x, y]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor || !editor->graph_) return nlohmann::json::object();

        auto& sgLib = editor->GetSubGraphLibrary();
        if (!sgLib.GetSubGraph(name)) return nlohmann::json::object();

        auto node = editor->AddSubGraphNode(name, ImVec2(x, y));
        if (!node) return nlohmann::json::object();

        return SerializeNode(std::static_pointer_cast<ShaderNode>(node));
    });
}

bool McpApi::DeleteNode(int nodeId)
{
    LOGD("MCP DeleteNode: nodeId=%d", nodeId);
    return MainThreadDispatcher::DispatchSync<bool>([this, nodeId]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor || !editor->graph_) return false;
        auto node = editor->GetNode(editor->GetNodes(), nodeId);
        if (!node) return false;

        editor->DeferredDeleteNode(nodeId);
        return true;
    });
}

bool McpApi::SelectNode(int nodeId)
{
    return MainThreadDispatcher::DispatchSync<bool>([this, nodeId]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor || !editor->graph_) return false;
        auto node = editor->GetNode(editor->GetNodes(), nodeId);
        if (!node) return false;
        editor->DeferredSelectNode(nodeId);
        return true;
    });
}

nlohmann::json McpApi::GetSelectedNode()
{
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return nlohmann::json::object();
        auto node = editor->GetSelectedNode();
        if (!node) return nlohmann::json::object();
        return SerializeNode(node);
    });
}

std::string McpApi::GetNodeShader(int nodeId)
{
    return MainThreadDispatcher::DispatchSync<std::string>([this, nodeId]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return std::string();
        auto node = editor->GetNode(editor->GetNodes(), nodeId);
        if (!node) return std::string();
        auto& material = node->material_;
        if (!material) return std::string();
        return material->GetSource();
    });
}

bool McpApi::UpdateNodeShader(int nodeId, const std::string& glslCode)
{
    return MainThreadDispatcher::DispatchSync<bool>([this, nodeId, &glslCode]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return false;
        auto node = editor->GetNode(editor->GetNodes(), nodeId);
        if (!node) return false;
        auto& material = node->material_;
        if (!material) return false;
        editor->UpdateMaterial(material, glslCode);
        return true;
    });
}

bool McpApi::ConnectNodes(int startPinId, int endPinId)
{
    LOGD("MCP ConnectNodes: startPin=%d endPin=%d", startPinId, endPinId);
    return MainThreadDispatcher::DispatchSync<bool>([this, startPinId, endPinId]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor || !editor->graph_) return false;

        bool startFound = false, endFound = false;
        for (const auto& node : editor->graph_->nodes_) {
            for (int pin : node->outputPinIds_) {
                if (pin == startPinId) startFound = true;
            }
            for (int pin : node->inputPinIds_) {
                if (pin == endPinId) endFound = true;
            }
        }
        if (!startFound || !endFound) return false;

        editor->graph_->CreateLink(startPinId, endPinId);
        return true;
    });
}

bool McpApi::DisconnectLink(int linkId)
{
    return MainThreadDispatcher::DispatchSync<bool>([this, linkId]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor || !editor->graph_) return false;

        bool found = false;
        for (const auto& link : editor->graph_->links_) {
            if (link.id == linkId) { found = true; break; }
        }
        if (!found) return false;

        editor->graph_->RemoveLink(linkId);
        return true;
    });
}

nlohmann::json McpApi::GetNodeProperties(int nodeId)
{
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this, nodeId]() {
        nlohmann::json result = nlohmann::json::array();
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return result;
        auto node = editor->GetNode(editor->GetNodes(), nodeId);
        if (!node) return result;
        auto& material = node->material_;
        if (!material) return result;

        auto& store = material->GetParamStore();
        for (const auto& [name, variant] : store.GetData()) {
            nlohmann::json prop;
            prop["name"] = name;

            std::visit([&prop](const auto& param) {
                using T = std::decay_t<decltype(param)>;
                if constexpr (std::is_same_v<T, gl::ProgramParam<float>>) {
                    prop["type"] = "float";
                    prop["value"] = param.Get();
                } else if constexpr (std::is_same_v<T, gl::ProgramParam<int32_t>>) {
                    prop["type"] = "int";
                    prop["value"] = param.Get();
                } else if constexpr (std::is_same_v<T, gl::ProgramParam<uint32_t>>) {
                    prop["type"] = "uint";
                    prop["value"] = param.Get();
                } else if constexpr (std::is_same_v<T, gl::ProgramParam<glm::vec2>>) {
                    prop["type"] = "vec2";
                    prop["value"] = {param.Get().x, param.Get().y};
                } else if constexpr (std::is_same_v<T, gl::ProgramParam<glm::vec3>>) {
                    prop["type"] = "vec3";
                    prop["value"] = {param.Get().x, param.Get().y, param.Get().z};
                } else if constexpr (std::is_same_v<T, gl::ProgramParam<glm::vec4>>) {
                    prop["type"] = "vec4";
                    prop["value"] = {param.Get().x, param.Get().y, param.Get().z, param.Get().w};
                } else if constexpr (std::is_same_v<T, gl::ProgramParam<bool>>) {
                    prop["type"] = "bool";
                    prop["value"] = param.Get();
                }
            }, variant);

            if (prop.contains("type")) {
                result.push_back(prop);
            }
        }

        return result;
    });
}

bool McpApi::SetNodeProperty(int nodeId, const std::string& name,
                              const nlohmann::json& value)
{
    return MainThreadDispatcher::DispatchSync<bool>([this, nodeId, &name, &value]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return false;
        auto node = editor->GetNode(editor->GetNodes(), nodeId);
        if (!node) return false;
        auto& material = node->material_;
        if (!material) return false;

        auto& store = material->GetParamStore();

        if (value.is_number()) {
            store.Set(name, value.get<float>());
            store.Set(name, value.get<int32_t>());
            store.Set(name, value.get<uint32_t>());
        } else if (value.is_boolean()) {
            store.Set(name, value.get<bool>());
        } else if (value.is_array() && value.size() == 2) {
            store.Set(name, glm::vec2(value[0].get<float>(), value[1].get<float>()));
        } else if (value.is_array() && value.size() == 3) {
            store.Set(name, glm::vec3(value[0].get<float>(), value[1].get<float>(), value[2].get<float>()));
        } else if (value.is_array() && value.size() == 4) {
            store.Set(name, glm::vec4(value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()));
        } else {
            return false;
        }

        editor->MarkDirty();
        return true;
    });
}

nlohmann::json McpApi::ListLibrary()
{
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this]() {
        nlohmann::json result;

        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return result;

        nlohmann::json shaders = nlohmann::json::array();
        auto& mm = editor->GetRenderer()->GetMaterialManager();
        for (const auto& [url, material] : mm.Resources()) {
            nlohmann::json s;
            s["name"] = material->GetName();
            s["isReadOnly"] = material->isReadOnly;
            shaders.push_back(s);
        }
        result["shaders"] = shaders;

        nlohmann::json filters = nlohmann::json::array();
        auto& filterLib = editor->GetFilterLibrary();
        for (const auto& filter : filterLib.GetAll()) {
            nlohmann::json f;
            f["name"] = filter->name_;
            f["description"] = filter->description_;

            nlohmann::json inputs = nlohmann::json::array();
            for (size_t i = 0; i < filter->GetInputsNum(); ++i) {
                nlohmann::json pin;
                pin["name"] = filter->GetInputName(i);
                pin["description"] = filter->GetInputDescription(i);
                inputs.push_back(pin);
            }
            f["inputs"] = inputs;

            nlohmann::json outputs = nlohmann::json::array();
            for (size_t i = 0; i < filter->GetOutputsNum(); ++i) {
                nlohmann::json pin;
                pin["name"] = filter->GetOutputName(i);
                pin["description"] = filter->GetOutputDescription(i);
                outputs.push_back(pin);
            }
            f["outputs"] = outputs;

            nlohmann::json params = nlohmann::json::array();
            auto& store = filter->filterParameters_;
            for (const auto& [name, variant] : store.GetData()) {
                nlohmann::json p;
                p["name"] = name;
                std::visit([&p](const auto& param) {
                    using T = std::decay_t<decltype(param)>;
                    if constexpr (std::is_same_v<T, ExposedParam<float>>) {
                        p["type"] = "float";
                        p["value"] = param.Get();
                        p["min"] = param.minValue_;
                        p["max"] = param.maxValue_;
                    } else if constexpr (std::is_same_v<T, ExposedParam<int>>) {
                        p["type"] = "int";
                        p["value"] = param.Get();
                        p["min"] = param.minValue_;
                        p["max"] = param.maxValue_;
                    } else if constexpr (std::is_same_v<T, ExposedParam<bool>>) {
                        p["type"] = "bool";
                        p["value"] = param.Get();
                    }
                }, variant);
                if (p.contains("type")) {
                    params.push_back(p);
                }
            }
            f["parameters"] = params;

            filters.push_back(f);
        }
        result["filters"] = filters;

        nlohmann::json subgraphs = nlohmann::json::array();
        auto& sgLib = editor->GetSubGraphLibrary();
        for (const auto& subGraphDef : sgLib.GetAll()) {
            nlohmann::json sgJson;
            sgJson["name"] = subGraphDef->name_;

            nlohmann::json sgInputs = nlohmann::json::array();
            for (const auto& input : subGraphDef->inputs_) {
                sgInputs.push_back({{"name", input.name}});
            }
            sgJson["inputs"] = sgInputs;

            nlohmann::json sgOutputs = nlohmann::json::array();
            for (const auto& output : subGraphDef->outputs_) {
                sgOutputs.push_back({{"name", output.name}});
            }
            sgJson["outputs"] = sgOutputs;

            subgraphs.push_back(sgJson);
        }
        result["subgraphs"] = subgraphs;

        return result;
    });
}

bool McpApi::CreateEffect(const std::string& name, const std::string& glslCode)
{
    return MainThreadDispatcher::DispatchSync<bool>([this, &name, &glslCode]() {
        auto editor = editorWnd_->GetShaderNodeEditor();
        if (!editor) return false;
        return editor->RegisterShaderEffect(name, glslCode);
    });
}

nlohmann::json McpApi::SaveScene()
{
    return MainThreadDispatcher::DispatchSync<nlohmann::json>([this]() {
        nlohmann::json result;
        std::string sceneStr = editorWnd_->SaveSceneToString();
        result["scene"] = sceneStr;

        auto editor = editorWnd_->GetShaderNodeEditor();
        if (editor) {
            result["effectGraph"] = editor->SaveGraphToString();
        }
        return result;
    });
}

nlohmann::json McpApi::SerializeNode(const ShaderNodePtr& node)
{
    nlohmann::json j;
    j["id"] = node->GetId();
    j["name"] = node->GetName();

    auto* subGraphNode = dynamic_cast<SubGraphNode*>(node.get());
    auto* filterNode = dynamic_cast<FilterNode*>(node.get());
    auto& material = node->material_;
    if (subGraphNode) {
        j["type"] = "subgraph";
        j["shaderName"] = node->GetName();
    } else if (material) {
        j["type"] = filterNode ? "filter" : "material";
        j["shaderName"] = material->GetName();
    } else {
        j["type"] = "unknown";
    }

    auto editor = editorWnd_->GetShaderNodeEditor();
    if (editor) {
        auto renderer = editor->GetNodeRenderer(editor->graph_, node);
        if (renderer) {
            j["position"] = {
                {"x", renderer->position_.x},
                {"y", renderer->position_.y}
            };
        }
    }

    nlohmann::json inputPins = nlohmann::json::array();
    for (size_t i = 0; i < node->inputPinIds_.size(); ++i) {
        inputPins.push_back({
            {"id", node->inputPinIds_[i]},
            {"name", node->GetInputName(i)}
        });
    }
    j["inputPins"] = inputPins;

    nlohmann::json outputPins = nlohmann::json::array();
    for (size_t i = 0; i < node->outputPinIds_.size(); ++i) {
        outputPins.push_back({
            {"id", node->outputPinIds_[i]},
            {"name", node->GetOutputName(i)}
        });
    }
    j["outputPins"] = outputPins;

    return j;
}
