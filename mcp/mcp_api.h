#pragma once

#include "editor/render_graph/forward.h"
#include "editor/base_node.h"

#include <nlohmann/json.hpp>
#include <string>

class EditorWindow;

class McpApi {
public:
    McpApi(EditorWindow* editorWindow);

    nlohmann::json ListNodes();
    nlohmann::json GetGraph();
    bool LoadGraph(const std::string& json);
    nlohmann::json AddShaderNode(const std::string& name, float x, float y);
    nlohmann::json AddFilterNode(const std::string& filterName, float x, float y);
    nlohmann::json AddSubGraphNode(const std::string& name, float x, float y);
    bool DeleteNode(int nodeId);
    bool SelectNode(int nodeId);
    nlohmann::json GetSelectedNode();
    std::string GetNodeShader(int nodeId);
    bool UpdateNodeShader(int nodeId, const std::string& glslCode);
    bool ConnectNodes(int startPinId, int endPinId);
    bool DisconnectLink(int linkId);
    nlohmann::json GetNodeProperties(int nodeId);
    bool SetNodeProperty(int nodeId, const std::string& name,
                         const nlohmann::json& value);

    nlohmann::json ListLibrary();
    bool CreateEffect(const std::string& name, const std::string& glslCode);

    nlohmann::json SaveScene();

private:
    nlohmann::json SerializeNode(const ShaderNodePtr& node);

    EditorWindow* editorWnd_;
};
