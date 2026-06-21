#pragma once

#ifndef _WIN32

#include <string>
#include <unordered_map>
#include <map>
#include <mutex>
#include <future>
#include <atomic>
#include <memory>
#include <vector>

#include "Debug/DebugRuntime.hpp"
#include "Debug/WebSocketServer.hpp"
#include "Thread.hpp"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace debug
{

// RobloxStudioBridge implements IDebugRuntime by:
//   1. Opening a WebSocket server that the Studio plugin connects to
//   2. Translating DAP requests → JSON commands sent over WebSocket
//   3. Translating JSON events from Studio → IDebugRuntime callbacks
//   4. Translating between file system paths and Roblox virtual paths via the Rojo sourcemap
//
// NOTE: ScriptDebuggerService.OnStopped does not support yielding, so the plugin
// captures a full snapshot (frames + variables) synchronously in OnStopped and
// sends it as part of the "stopped" event. The bridge caches this snapshot and
// serves stackTrace/variables requests from it without contacting Studio.
class RobloxStudioBridge : public IDebugRuntime
{
public:
    RobloxStudioBridge() = default;
    ~RobloxStudioBridge() override;

    // IDebugRuntime interface
    void attach(const dap::AttachArgs& args) override;
    void disconnect() override;
    void configurationDone() override;

    dap::SetBreakpointsResult setBreakpoints(const dap::SetBreakpointsArgs& args) override;
    void setExceptionBreakpoints(const dap::SetExceptionBreakpointsArgs& args) override;

    // Execution control: game already resumed when OnStopped returned, so these
    // just fire onContinued (except pause which sends a command to Studio).
    void continue_(int threadId) override;
    void next(int threadId) override;
    void stepIn(int threadId) override;
    void stepOut(int threadId) override;
    void pause(int threadId) override;

    // Inspection: served from the snapshot captured during OnStopped.
    dap::StackTraceResult stackTrace(const dap::StackTraceArgs& args) override;
    dap::ScopesResult scopes(const dap::ScopesArgs& args) override;
    dap::VariablesResult variables(const dap::VariablesArgs& args) override;
    dap::EvaluateResult evaluate(const dap::EvaluateArgs& args) override;

private:
    std::unique_ptr<WebSocketServer> wss;
    Thread wsReceiveThread;
    std::atomic<bool> disconnected{false};

    // Path translation
    std::unordered_map<std::string, std::string> fileToVirtual; // abs file path → game/SS/Foo
    std::unordered_map<std::string, std::string> virtualToFile; // game/SS/Foo → abs file path

    // Snapshot cache — populated from the "stopped" event (which includes all
    // frames and variables captured synchronously inside OnStopped).
    struct VariableEntry
    {
        std::string name;
        std::string value;
        std::optional<std::string> type;
        int variablesReference = 0;
    };

    struct FrameEntry
    {
        int id = 0;
        std::string name;
        std::string scriptPath; // virtual path, e.g. game/SS/Foo
        int line = 0;
        int column = 0;
    };

    struct StopSnapshot
    {
        int threadId = 0;
        std::vector<FrameEntry> frames;
        std::map<int, std::vector<VariableEntry>> variables; // frameId → vars
        bool active = false;
    };

    StopSnapshot snapshot;
    std::mutex snapshotMutex;

    // Async request/response correlation (used only for setBreakpoints)
    std::atomic<int> nextReqId{1};
    std::mutex pendingMutex;
    std::map<int, std::promise<json>> pendingResponses;

    // Serialise WebSocket writes (wss.send is not thread-safe)
    std::mutex wsSendMutex;

    // Build path maps from sourcemap JSON
    void loadSourcemap(const std::string& path);
    static void buildPathMaps(const json& node, const std::string& virtualPath,
        std::unordered_map<std::string, std::string>& fileToVirtual,
        std::unordered_map<std::string, std::string>& virtualToFile);

    // WebSocket receive loop (runs on wsReceiveThread)
    void receiveLoop();

    // Handle an incoming JSON event/response from Studio
    void handleIncoming(const json& msg);

    // Parse and cache snapshot data from a "stopped" event payload
    void cacheSnapshot(const json& msg);

    // Send a command and wait (up to timeout) for a response keyed by requestId.
    // Throws std::runtime_error on timeout or disconnect.
    json executeCommand(const std::string& command, json params, std::chrono::seconds timeout = std::chrono::seconds(10));

    // Fire-and-forget send (no response expected)
    void sendCommand(const std::string& command, json params);

    // Translate a DAP source path → Roblox virtual path (for Studio side)
    std::string toVirtualPath(const std::string& filePath) const;

    // Translate a Roblox virtual path → file system path (for DAP response)
    std::optional<std::string> toFilePath(const std::string& virtualPath) const;
};

} // namespace debug

#endif // _WIN32
