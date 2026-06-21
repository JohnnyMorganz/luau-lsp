#ifndef _WIN32

#include "Debug/RobloxStudioBridge.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>

#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace debug
{

RobloxStudioBridge::~RobloxStudioBridge()
{
    disconnect();
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void RobloxStudioBridge::attach(const dap::AttachArgs& args)
{
    if (args.sourcemap)
        loadSourcemap(*args.sourcemap);

    wss = std::make_unique<WebSocketServer>(args.debugWsPort);
    if (!wss->listen())
        throw std::runtime_error("Failed to start WebSocket server on port " + std::to_string(args.debugWsPort));

    // Start the receive loop on a background thread
    wsReceiveThread = Thread([this] { receiveLoop(); });
}

void RobloxStudioBridge::disconnect()
{
    disconnected.store(true);

    // Unblock any threads waiting for responses
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& [id, promise] : pendingResponses)
        {
            try
            {
                promise.set_exception(std::make_exception_ptr(std::runtime_error("disconnected")));
            }
            catch (...) {}
        }
        pendingResponses.clear();
    }

    if (wss)
        wss->close();
}

void RobloxStudioBridge::configurationDone()
{
    // No special action needed; Studio begins executing when it's ready.
}

// ── Sourcemap ─────────────────────────────────────────────────────────────────

void RobloxStudioBridge::loadSourcemap(const std::string& path)
{
    std::ifstream f(path);
    if (!f)
    {
        std::cerr << "DAP: Could not open sourcemap at " << path << "\n";
        return;
    }

    try
    {
        auto j = json::parse(f);
        std::string rootName = j.value("name", "game");
        buildPathMaps(j, rootName, fileToVirtual, virtualToFile);
    }
    catch (const std::exception& e)
    {
        std::cerr << "DAP: Sourcemap parse error: " << e.what() << "\n";
    }
}

void RobloxStudioBridge::buildPathMaps(const json& node, const std::string& virtualPath,
    std::unordered_map<std::string, std::string>& fileToVirtual,
    std::unordered_map<std::string, std::string>& virtualToFile)
{
    if (node.contains("filePaths") && node["filePaths"].is_array())
    {
        for (const auto& fp : node["filePaths"])
        {
            if (!fp.is_string())
                continue;
            std::string filePath = fp.get<std::string>();
            if (filePath.size() >= 4)
            {
                auto ext = filePath.substr(filePath.rfind('.') + 1);
                if (ext == "lua" || ext == "luau")
                {
                    fileToVirtual.insert_or_assign(filePath, virtualPath);
                    virtualToFile.insert_or_assign(virtualPath, filePath);
                }
            }
        }
    }

    if (node.contains("children") && node["children"].is_array())
    {
        for (const auto& child : node["children"])
        {
            if (!child.contains("name"))
                continue;
            std::string childName = child["name"].get<std::string>();
            buildPathMaps(child, virtualPath + "/" + childName, fileToVirtual, virtualToFile);
        }
    }
}

std::string RobloxStudioBridge::toVirtualPath(const std::string& filePath) const
{
    auto it = fileToVirtual.find(filePath);
    if (it != fileToVirtual.end())
        return it->second;
    return filePath;
}

std::optional<std::string> RobloxStudioBridge::toFilePath(const std::string& virtualPath) const
{
    auto it = virtualToFile.find(virtualPath);
    if (it != virtualToFile.end())
        return it->second;
    return std::nullopt;
}

// ── WebSocket receive loop ────────────────────────────────────────────────────

void RobloxStudioBridge::receiveLoop()
{
    std::string msgStr;
    while (!disconnected.load() && wss && wss->isConnected())
    {
        if (!wss->receive(msgStr))
            break;

        try
        {
            auto j = json::parse(msgStr);
            handleIncoming(j);
        }
        catch (const std::exception& e)
        {
            std::cerr << "DAP bridge: JSON parse error: " << e.what() << "\n";
        }
    }

    // Connection closed — notify any waiters and fire terminated
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        for (auto& [id, p] : pendingResponses)
        {
            try { p.set_exception(std::make_exception_ptr(std::runtime_error("ws closed"))); }
            catch (...) {}
        }
        pendingResponses.clear();
    }

    if (onTerminated && !disconnected.load())
        onTerminated();
}

void RobloxStudioBridge::cacheSnapshot(const json& msg)
{
    StopSnapshot snap;
    snap.active = true;

    if (msg.contains("snapshot") && msg["snapshot"].is_object())
    {
        const auto& s = msg["snapshot"];
        snap.threadId = s.value("threadId", 0);

        if (s.contains("frames") && s["frames"].is_array())
        {
            for (const auto& f : s["frames"])
            {
                FrameEntry fe;
                fe.id = f.value("id", 0);
                fe.name = f.value("name", "?");
                fe.scriptPath = f.value("scriptPath", "");
                fe.line = f.value("line", 0);
                fe.column = f.value("column", 0);
                snap.frames.push_back(fe);
            }
        }

        if (s.contains("variables") && s["variables"].is_object())
        {
            for (const auto& [frameIdStr, varArray] : s["variables"].items())
            {
                int frameId = std::stoi(frameIdStr);
                std::vector<VariableEntry> vars;
                if (varArray.is_array())
                {
                    for (const auto& v : varArray)
                    {
                        VariableEntry ve;
                        ve.name = v.value("name", "?");
                        ve.value = v.value("value", "nil");
                        ve.variablesReference = v.value("variablesReference", 0);
                        if (v.contains("type") && v["type"].is_string())
                            ve.type = v["type"].get<std::string>();
                        vars.push_back(ve);
                    }
                }
                snap.variables[frameId] = std::move(vars);
            }
        }
    }
    else
    {
        // Older plugin without snapshot: no frame data available
        snap.threadId = msg.value("threadId", 0);
    }

    std::lock_guard<std::mutex> lock(snapshotMutex);
    snapshot = std::move(snap);
}

void RobloxStudioBridge::handleIncoming(const json& msg)
{
    if (msg.contains("response"))
    {
        // Response to a setBreakpoints command
        int reqId = msg.value("requestId", -1);
        std::lock_guard<std::mutex> lock(pendingMutex);
        auto it = pendingResponses.find(reqId);
        if (it != pendingResponses.end())
        {
            it->second.set_value(msg);
            pendingResponses.erase(it);
        }
        return;
    }

    std::string event = msg.value("event", "");

    if (event == "ready")
    {
        if (onOutput)
            onOutput({"Roblox Studio debug bridge connected\n", "console"});
    }
    else if (event == "stopped")
    {
        if (!onStopped)
            return;

        // Cache the snapshot before firing the event
        cacheSnapshot(msg);

        dap::StoppedEvent e;
        e.reason = msg.value("reason", "breakpoint");
        if (msg.contains("exceptionText"))
            e.text = msg["exceptionText"].get<std::string>();
        {
            std::lock_guard<std::mutex> lock(snapshotMutex);
            e.threadId = snapshot.threadId ? std::make_optional(snapshot.threadId) : std::nullopt;
        }
        if (msg.contains("threadIds") && msg["threadIds"].is_array() && !msg["threadIds"].empty())
            e.threadId = msg["threadIds"][0].get<int>();
        e.allThreadsStopped = true;
        onStopped(e);
    }
    else if (event == "continued")
    {
        if (onContinued)
            onContinued(msg.value("threadId", 0));
    }
    else if (event == "terminated")
    {
        if (onTerminated)
            onTerminated();
    }
    else if (event == "output")
    {
        if (onOutput)
        {
            dap::OutputEvent e;
            e.output = msg.value("output", "");
            e.category = msg.value("category", "stdout");
            onOutput(e);
        }
    }
}

// ── Command helpers ───────────────────────────────────────────────────────────

json RobloxStudioBridge::executeCommand(const std::string& command, json params,
    std::chrono::seconds timeout)
{
    int id = nextReqId.fetch_add(1);
    params["command"] = command;
    params["requestId"] = id;

    std::promise<json> promise;
    auto future = promise.get_future();
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingResponses[id] = std::move(promise);
    }

    {
        std::lock_guard<std::mutex> lock(wsSendMutex);
        if (wss)
            wss->send(params.dump());
    }

    auto status = future.wait_for(timeout);
    if (status == std::future_status::timeout)
    {
        std::lock_guard<std::mutex> lock(pendingMutex);
        pendingResponses.erase(id);
        throw std::runtime_error("Timeout waiting for Studio response to '" + command + "'");
    }
    return future.get();
}

void RobloxStudioBridge::sendCommand(const std::string& command, json params)
{
    params["command"] = command;
    std::lock_guard<std::mutex> lock(wsSendMutex);
    if (wss)
        wss->send(params.dump());
}

// ── IDebugRuntime implementation ─────────────────────────────────────────────

dap::SetBreakpointsResult RobloxStudioBridge::setBreakpoints(const dap::SetBreakpointsArgs& args)
{
    std::string filePath = args.source.path.value_or("");
    std::string scriptPath = toVirtualPath(filePath);

    json bps = json::array();
    for (const auto& bp : args.breakpoints)
    {
        json j = {{"line", bp.line}};
        if (bp.condition)
            j["condition"] = *bp.condition;
        if (bp.logMessage)
            j["logMessage"] = *bp.logMessage;
        bps.push_back(j);
    }

    json response = executeCommand("setBreakpoints", {{"scriptPath", scriptPath}, {"breakpoints", bps}});

    dap::SetBreakpointsResult result;
    if (response.contains("breakpoints") && response["breakpoints"].is_array())
    {
        for (size_t i = 0; i < response["breakpoints"].size(); ++i)
        {
            const auto& b = response["breakpoints"][i];
            dap::Breakpoint bp;
            bp.id = (int)i;
            bp.verified = b.value("verified", false);
            bp.line = b.contains("line") ? std::make_optional(b["line"].get<int>()) : std::nullopt;
            bp.source = args.source;
            if (b.contains("message"))
                bp.message = b["message"].get<std::string>();
            result.breakpoints.push_back(bp);
        }
    }
    else
    {
        for (const auto& bp : args.breakpoints)
        {
            dap::Breakpoint b;
            b.verified = true;
            b.line = bp.line;
            b.source = args.source;
            result.breakpoints.push_back(b);
        }
    }
    return result;
}

void RobloxStudioBridge::setExceptionBreakpoints(const dap::SetExceptionBreakpointsArgs& args)
{
    bool breakOnAll = false;
    for (const auto& f : args.filters)
        if (f == "all")
            breakOnAll = true;

    sendCommand("setExceptionBreakpoints", {{"breakOnAll", breakOnAll}});
}

// Execution control: ScriptDebuggerService.OnStopped cannot yield, so the plugin
// immediately returns Resume after sending the snapshot.  By the time these
// methods are called, the game is already running again — just notify the editor.
void RobloxStudioBridge::continue_(int threadId)
{
    {
        std::lock_guard<std::mutex> lock(snapshotMutex);
        snapshot.active = false;
    }
    if (onContinued)
        onContinued(threadId);
}

void RobloxStudioBridge::next(int threadId)
{
    continue_(threadId);
}

void RobloxStudioBridge::stepIn(int threadId)
{
    continue_(threadId);
}

void RobloxStudioBridge::stepOut(int threadId)
{
    continue_(threadId);
}

void RobloxStudioBridge::pause(int threadId)
{
    // Sends a pause request to the Studio plugin, which calls ScriptDebuggerService:Pause().
    // That will trigger OnStopped which will fire a new snapshot + stopped event.
    sendCommand("pause", {{"threadId", threadId}});
}

// ── Snapshot-based inspection ─────────────────────────────────────────────────

dap::StackTraceResult RobloxStudioBridge::stackTrace(const dap::StackTraceArgs& /*args*/)
{
    std::lock_guard<std::mutex> lock(snapshotMutex);

    dap::StackTraceResult result;
    if (!snapshot.active)
        return result;

    for (const auto& f : snapshot.frames)
    {
        dap::StackFrame frame;
        frame.id = f.id;
        frame.name = f.name;
        frame.line = f.line;
        frame.column = f.column;

        if (auto filePath = toFilePath(f.scriptPath))
        {
            dap::Source src;
            src.path = *filePath;
            auto slash = f.scriptPath.rfind('/');
            src.name = (slash != std::string::npos) ? f.scriptPath.substr(slash + 1) : f.scriptPath;
            frame.source = src;
        }
        else if (!f.scriptPath.empty())
        {
            dap::Source src;
            src.name = f.scriptPath;
            frame.source = src;
        }
        result.stackFrames.push_back(frame);
    }

    result.totalFrames = (int)snapshot.frames.size();
    return result;
}

dap::ScopesResult RobloxStudioBridge::scopes(const dap::ScopesArgs& args)
{
    dap::Scope locals;
    locals.name = "Locals";
    locals.variablesReference = args.frameId + 1; // +1 so 0 means "no children"
    locals.expensive = false;
    return dap::ScopesResult{{locals}};
}

dap::VariablesResult RobloxStudioBridge::variables(const dap::VariablesArgs& args)
{
    // variablesReference is frameId+1 (from scopes above)
    int frameId = args.variablesReference - 1;

    std::lock_guard<std::mutex> lock(snapshotMutex);

    dap::VariablesResult result;
    if (!snapshot.active)
        return result;

    auto it = snapshot.variables.find(frameId);
    if (it == snapshot.variables.end())
        return result;

    for (const auto& v : it->second)
    {
        dap::Variable var;
        var.name = v.name;
        var.value = v.value;
        var.type = v.type;
        var.variablesReference = v.variablesReference;
        result.variables.push_back(var);
    }
    return result;
}

dap::EvaluateResult RobloxStudioBridge::evaluate(const dap::EvaluateArgs& args)
{
    // Live evaluation is not supported: ScriptDebuggerService.OnStopped cannot
    // yield, so the game resumes before the editor sends this request.
    (void)args;
    dap::EvaluateResult result;
    result.result = "<evaluation not available: game has resumed>";
    return result;
}

} // namespace debug

#endif // _WIN32
