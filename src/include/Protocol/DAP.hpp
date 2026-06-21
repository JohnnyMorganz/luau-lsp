#pragma once

#include <string>
#include <vector>
#include <optional>
#include "nlohmann/json.hpp"

using json = nlohmann::json;

// Debug Adapter Protocol types.
// Wire format: Content-Length: N\r\n\r\n{json}  (same framing as LSP)
// Message types: "request", "response", "event"
namespace dap
{

// ── Core structures ────────────────────────────────────────────────────────────

struct Source
{
    std::optional<std::string> name;
    std::optional<std::string> path;
};

inline void to_json(json& j, const Source& s)
{
    j = json{};
    if (s.name)
        j["name"] = *s.name;
    if (s.path)
        j["path"] = *s.path;
}

inline void from_json(const json& j, Source& s)
{
    if (j.contains("name") && j["name"].is_string())
        s.name = j["name"].get<std::string>();
    if (j.contains("path") && j["path"].is_string())
        s.path = j["path"].get<std::string>();
}

struct SourceBreakpoint
{
    int line = 0;
    std::optional<std::string> condition;
    std::optional<std::string> logMessage; // logpoint
};

inline void from_json(const json& j, SourceBreakpoint& b)
{
    b.line = j.at("line").get<int>();
    if (j.contains("condition") && !j["condition"].is_null())
        b.condition = j["condition"].get<std::string>();
    if (j.contains("logMessage") && !j["logMessage"].is_null())
        b.logMessage = j["logMessage"].get<std::string>();
}

struct Breakpoint
{
    int id = 0;
    bool verified = false;
    std::optional<std::string> message;
    std::optional<Source> source;
    std::optional<int> line;
};

inline void to_json(json& j, const Breakpoint& b)
{
    j = json{{"id", b.id}, {"verified", b.verified}};
    if (b.message)
        j["message"] = *b.message;
    if (b.source)
        j["source"] = *b.source;
    if (b.line)
        j["line"] = *b.line;
}

struct StackFrame
{
    int id = 0;
    std::string name;
    std::optional<Source> source;
    int line = 0;
    int column = 0;
};

inline void to_json(json& j, const StackFrame& f)
{
    j = json{{"id", f.id}, {"name", f.name}, {"line", f.line}, {"column", f.column}};
    if (f.source)
        j["source"] = *f.source;
}

struct Scope
{
    std::string name;
    int variablesReference = 0;
    bool expensive = false;
};

inline void to_json(json& j, const Scope& s)
{
    j = json{{"name", s.name}, {"variablesReference", s.variablesReference}, {"expensive", s.expensive}};
}

struct Variable
{
    std::string name;
    std::string value;
    std::optional<std::string> type;
    int variablesReference = 0; // 0 means no children
};

inline void to_json(json& j, const Variable& v)
{
    j = json{{"name", v.name}, {"value", v.value}, {"variablesReference", v.variablesReference}};
    if (v.type)
        j["type"] = *v.type;
}

// ── Request argument structs ───────────────────────────────────────────────────

struct InitializeArgs
{
    std::optional<std::string> clientID;
    std::optional<std::string> clientName;
    std::optional<std::string> adapterID;
};

inline void from_json(const json& j, InitializeArgs& a)
{
    if (j.contains("clientID"))
        a.clientID = j["clientID"].get<std::string>();
    if (j.contains("clientName"))
        a.clientName = j["clientName"].get<std::string>();
    if (j.contains("adapterID"))
        a.adapterID = j["adapterID"].get<std::string>();
}

struct AttachArgs
{
    int debugWsPort = 7868;
    std::optional<std::string> sourcemap;
};

inline void from_json(const json& j, AttachArgs& a)
{
    if (j.contains("debugWsPort"))
        a.debugWsPort = j["debugWsPort"].get<int>();
    if (j.contains("sourcemap") && !j["sourcemap"].is_null())
        a.sourcemap = j["sourcemap"].get<std::string>();
}

struct SetBreakpointsArgs
{
    Source source;
    std::vector<SourceBreakpoint> breakpoints;
};

inline void from_json(const json& j, SetBreakpointsArgs& a)
{
    a.source = j.at("source").get<Source>();
    if (j.contains("breakpoints"))
        a.breakpoints = j["breakpoints"].get<std::vector<SourceBreakpoint>>();
}

struct SetExceptionBreakpointsArgs
{
    std::vector<std::string> filters;
};

inline void from_json(const json& j, SetExceptionBreakpointsArgs& a)
{
    if (j.contains("filters"))
        a.filters = j["filters"].get<std::vector<std::string>>();
}

struct ContinueArgs
{
    int threadId = 0;
};

inline void from_json(const json& j, ContinueArgs& a)
{
    a.threadId = j.at("threadId").get<int>();
}

struct NextArgs
{
    int threadId = 0;
};

inline void from_json(const json& j, NextArgs& a)
{
    a.threadId = j.at("threadId").get<int>();
}

struct StepInArgs
{
    int threadId = 0;
};

inline void from_json(const json& j, StepInArgs& a)
{
    a.threadId = j.at("threadId").get<int>();
}

struct StepOutArgs
{
    int threadId = 0;
};

inline void from_json(const json& j, StepOutArgs& a)
{
    a.threadId = j.at("threadId").get<int>();
}

struct PauseArgs
{
    int threadId = 0;
};

inline void from_json(const json& j, PauseArgs& a)
{
    a.threadId = j.at("threadId").get<int>();
}

struct StackTraceArgs
{
    int threadId = 0;
    int startFrame = 0;
    std::optional<int> levels;
};

inline void from_json(const json& j, StackTraceArgs& a)
{
    a.threadId = j.at("threadId").get<int>();
    if (j.contains("startFrame"))
        a.startFrame = j["startFrame"].get<int>();
    if (j.contains("levels"))
        a.levels = j["levels"].get<int>();
}

struct ScopesArgs
{
    int frameId = 0;
};

inline void from_json(const json& j, ScopesArgs& a)
{
    a.frameId = j.at("frameId").get<int>();
}

struct VariablesArgs
{
    int variablesReference = 0;
};

inline void from_json(const json& j, VariablesArgs& a)
{
    a.variablesReference = j.at("variablesReference").get<int>();
}

struct EvaluateArgs
{
    std::string expression;
    std::optional<int> frameId;
    std::optional<std::string> context;
};

inline void from_json(const json& j, EvaluateArgs& a)
{
    a.expression = j.at("expression").get<std::string>();
    if (j.contains("frameId") && !j["frameId"].is_null())
        a.frameId = j["frameId"].get<int>();
    if (j.contains("context"))
        a.context = j["context"].get<std::string>();
}

struct DisconnectArgs
{
    bool terminateDebuggee = false;
};

inline void from_json(const json& j, DisconnectArgs& a)
{
    if (j.contains("terminateDebuggee"))
        a.terminateDebuggee = j["terminateDebuggee"].get<bool>();
}

// ── Response body structs ─────────────────────────────────────────────────────

struct Capabilities
{
    bool supportsConditionalBreakpoints = true;
    bool supportsLogPoints = true;
    bool supportsEvaluateForHovers = false;

    struct ExceptionBreakpointFilter
    {
        std::string filter;
        std::string label;
        bool defaultValue = false;
    };

    std::vector<ExceptionBreakpointFilter> exceptionBreakpointFilters{
        {"all", "All Exceptions", false},
    };
};

inline void to_json(json& j, const Capabilities& c)
{
    j = json{
        {"supportsConditionalBreakpoints", c.supportsConditionalBreakpoints},
        {"supportsLogPoints", c.supportsLogPoints},
        {"supportsEvaluateForHovers", c.supportsEvaluateForHovers},
    };

    json filters = json::array();
    for (const auto& f : c.exceptionBreakpointFilters)
    {
        filters.push_back({{"filter", f.filter}, {"label", f.label}, {"default", f.defaultValue}});
    }
    j["exceptionBreakpointFilters"] = filters;
}

struct SetBreakpointsResult
{
    std::vector<Breakpoint> breakpoints;
};

inline void to_json(json& j, const SetBreakpointsResult& r)
{
    j = json{{"breakpoints", r.breakpoints}};
}

struct StackTraceResult
{
    std::vector<StackFrame> stackFrames;
    std::optional<int> totalFrames;
};

inline void to_json(json& j, const StackTraceResult& r)
{
    j = json{{"stackFrames", r.stackFrames}};
    if (r.totalFrames)
        j["totalFrames"] = *r.totalFrames;
}

struct ScopesResult
{
    std::vector<Scope> scopes;
};

inline void to_json(json& j, const ScopesResult& r)
{
    j = json{{"scopes", r.scopes}};
}

struct VariablesResult
{
    std::vector<Variable> variables;
};

inline void to_json(json& j, const VariablesResult& r)
{
    j = json{{"variables", r.variables}};
}

struct EvaluateResult
{
    std::string result;
    std::optional<std::string> type;
    int variablesReference = 0;
};

inline void to_json(json& j, const EvaluateResult& r)
{
    j = json{{"result", r.result}, {"variablesReference", r.variablesReference}};
    if (r.type)
        j["type"] = *r.type;
}

// ── Event body structs ────────────────────────────────────────────────────────

struct StoppedEvent
{
    std::string reason; // "breakpoint", "step", "exception", "pause"
    std::optional<std::string> description;
    std::optional<int> threadId;
    bool allThreadsStopped = true;
    std::optional<std::string> text; // exception text
};

inline void to_json(json& j, const StoppedEvent& e)
{
    j = json{{"reason", e.reason}, {"allThreadsStopped", e.allThreadsStopped}};
    if (e.description)
        j["description"] = *e.description;
    if (e.threadId)
        j["threadId"] = *e.threadId;
    if (e.text)
        j["text"] = *e.text;
}

struct OutputEvent
{
    std::string output;
    std::string category = "console"; // "console", "stdout", "stderr"
};

inline void to_json(json& j, const OutputEvent& e)
{
    j = json{{"output", e.output}, {"category", e.category}};
}

// ── Wire-level message ────────────────────────────────────────────────────────

struct Message
{
    int seq = 0;
    std::string type; // "request", "response", "event"

    // For requests
    std::optional<std::string> command;
    std::optional<json> arguments;

    // For responses
    std::optional<int> request_seq;
    std::optional<bool> success;
    std::optional<json> body;
    std::optional<std::string> message;

    // For events
    std::optional<std::string> event;
};

inline Message parseMessage(const std::string& jsonString)
{
    auto j = json::parse(jsonString);
    Message msg;
    msg.seq = j.at("seq").get<int>();
    msg.type = j.at("type").get<std::string>();

    if (j.contains("command"))
        msg.command = j["command"].get<std::string>();
    if (j.contains("arguments") && !j["arguments"].is_null())
        msg.arguments = j["arguments"];
    if (j.contains("request_seq"))
        msg.request_seq = j["request_seq"].get<int>();
    if (j.contains("success"))
        msg.success = j["success"].get<bool>();
    if (j.contains("body"))
        msg.body = j["body"];
    if (j.contains("message"))
        msg.message = j["message"].get<std::string>();
    if (j.contains("event"))
        msg.event = j["event"].get<std::string>();

    return msg;
}

} // namespace dap
