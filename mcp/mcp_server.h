#pragma once

#include <nlohmann/json.hpp>
#include <memory>
#include <string>
#include <thread>
#include <atomic>

namespace httplib { class Server; }
class McpApi;

class McpServer {
public:
    McpServer(class EditorWindow* editorWindow, int port);
    ~McpServer();

    void Start();
    void Stop();
    bool IsRunning() const;
    int GetPort() const { return port_; }

private:
    void Run();

    std::string GenerateSessionId();
    std::string HandleRequest(const std::string& body);

    nlohmann::json Route(const std::string& method, const nlohmann::json& params, const nlohmann::json& id);

    nlohmann::json HandleInitialize(const nlohmann::json& params);
    nlohmann::json HandleToolsList();
    nlohmann::json HandleToolsCall(const nlohmann::json& params);
    nlohmann::json HandlePing();

    std::unique_ptr<httplib::Server> server_;
    std::unique_ptr<McpApi> api_;
    std::thread serverThread_;
    int port_;
    std::string sessionId_;
    std::atomic<bool> running_{false};
    std::atomic<bool> initialized_{false};
};
