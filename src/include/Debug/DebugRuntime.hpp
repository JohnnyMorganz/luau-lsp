#pragma once

#include <string>
#include <functional>
#include <memory>
#include "Protocol/DAP.hpp"

// IDebugRuntime is the pluggable backend interface that DAPServer uses.
// Each Luau runtime provides one implementation:
//   - RobloxStudioBridge: bridges to Studio via WebSocket + ScriptDebuggerService
//   - (future) LuauDebuggerProxy: proxies to a running luau-debugger DAP server
namespace debug
{

class IDebugRuntime
{
public:
    virtual ~IDebugRuntime() = default;

    // Lifecycle
    virtual void attach(const dap::AttachArgs& args) = 0;
    virtual void disconnect() = 0;
    virtual void configurationDone() = 0;

    // Breakpoints
    virtual dap::SetBreakpointsResult setBreakpoints(const dap::SetBreakpointsArgs& args) = 0;
    virtual void setExceptionBreakpoints(const dap::SetExceptionBreakpointsArgs& args) = 0;

    // Execution control
    virtual void continue_(int threadId) = 0;
    virtual void next(int threadId) = 0;
    virtual void stepIn(int threadId) = 0;
    virtual void stepOut(int threadId) = 0;
    virtual void pause(int threadId) = 0;

    // Inspection
    virtual dap::StackTraceResult stackTrace(const dap::StackTraceArgs& args) = 0;
    virtual dap::ScopesResult scopes(const dap::ScopesArgs& args) = 0;
    virtual dap::VariablesResult variables(const dap::VariablesArgs& args) = 0;
    virtual dap::EvaluateResult evaluate(const dap::EvaluateArgs& args) = 0;

    // Callbacks: the runtime calls these to push events to DAPServer.
    // DAPServer sets these after constructing the runtime.
    std::function<void(const dap::StoppedEvent&)> onStopped;
    std::function<void(int /*threadId*/)> onContinued;
    std::function<void()> onTerminated;
    std::function<void(const dap::OutputEvent&)> onOutput;
};

} // namespace debug
