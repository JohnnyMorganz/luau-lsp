#include "Debug/DAPServer.hpp"

#include <iostream>
#include <string>

#include "LSP/JsonRpc.hpp"

namespace debug
{

DAPServer::DAPServer(Transport* aTransport, std::unique_ptr<IDebugRuntime> aRuntime)
    : transport(aTransport)
    , runtime(std::move(aRuntime))
{
    // Wire up runtime → server event callbacks
    this->runtime->onStopped = [this](const dap::StoppedEvent& e)
    {
        sendStoppedEvent(e);
    };
    this->runtime->onContinued = [this](int threadId)
    {
        sendContinuedEvent(threadId);
    };
    this->runtime->onTerminated = [this]()
    {
        sendTerminatedEvent();
    };
    this->runtime->onOutput = [this](const dap::OutputEvent& e)
    {
        sendOutputEvent(e);
    };

    messageProcessorThread = Thread(
        [this]
        {
            while (auto msg = popMessage())
                handleMessage(*msg);
        });
}

bool DAPServer::requestedShutdown() const
{
    return shutdownRequested.load();
}

void DAPServer::processInputLoop()
{
    std::string jsonString;
    while (!shutdownRequested.load())
    {
        if (json_rpc::readRawMessage(transport, jsonString))
        {
            try
            {
                auto msg = dap::parseMessage(jsonString);
                {
                    std::lock_guard<std::mutex> lock(messagesMutex);
                    messages.push(msg);
                }
                messagesCv.notify_one();
            }
            catch (const std::exception& e)
            {
                std::cerr << "DAP parse error: " << e.what() << '\n';
            }
        }
    }
}

std::optional<dap::Message> DAPServer::popMessage()
{
    std::unique_lock<std::mutex> lock(messagesMutex);
    messagesCv.wait(lock, [this]
        {
            return !messages.empty() || shutdownRequested.load();
        });

    if (shutdownRequested.load() && messages.empty())
        return std::nullopt;

    auto msg = messages.front();
    messages.pop();
    return msg;
}

void DAPServer::handleMessage(const dap::Message& msg)
{
    if (msg.type == "request")
        processRequest(msg);
}

void DAPServer::processRequest(const dap::Message& msg)
{
    if (!msg.command)
        return;

    const auto& command = *msg.command;
    const auto& args = msg.arguments;
    const int requestSeq = msg.seq;

    try
    {
        if (command == "initialize")
        {
            dap::Capabilities caps;
            sendResponse(requestSeq, command, caps);
            // Fire "initialized" event so the client sends setBreakpoints + configurationDone
            sendEvent("initialized", json::object());
        }
        else if (command == "attach")
        {
            dap::AttachArgs attachArgs;
            if (args)
                attachArgs = args->get<dap::AttachArgs>();
            runtime->attach(attachArgs);
            sendResponse(requestSeq, command, json::object());
        }
        else if (command == "configurationDone")
        {
            runtime->configurationDone();
            sendResponse(requestSeq, command, json::object());
        }
        else if (command == "setBreakpoints")
        {
            if (!args)
            {
                sendErrorResponse(requestSeq, command, "arguments required");
                return;
            }
            auto bpArgs = args->get<dap::SetBreakpointsArgs>();
            auto result = runtime->setBreakpoints(bpArgs);
            sendResponse(requestSeq, command, result);
        }
        else if (command == "setExceptionBreakpoints")
        {
            dap::SetExceptionBreakpointsArgs ebArgs;
            if (args)
                ebArgs = args->get<dap::SetExceptionBreakpointsArgs>();
            runtime->setExceptionBreakpoints(ebArgs);
            sendResponse(requestSeq, command, json::object());
        }
        else if (command == "continue")
        {
            int tid = args ? args->get<dap::ThreadArgs>().threadId : 0;
            runtime->continue_(tid);
            sendResponse(requestSeq, command, json{{"allThreadsContinued", true}});
        }
        else if (command == "next")
        {
            int tid = args ? args->get<dap::ThreadArgs>().threadId : 0;
            runtime->next(tid);
            sendResponse(requestSeq, command, json::object());
        }
        else if (command == "stepIn")
        {
            int tid = args ? args->get<dap::ThreadArgs>().threadId : 0;
            runtime->stepIn(tid);
            sendResponse(requestSeq, command, json::object());
        }
        else if (command == "stepOut")
        {
            int tid = args ? args->get<dap::ThreadArgs>().threadId : 0;
            runtime->stepOut(tid);
            sendResponse(requestSeq, command, json::object());
        }
        else if (command == "pause")
        {
            int tid = args ? args->get<dap::ThreadArgs>().threadId : 0;
            runtime->pause(tid);
            sendResponse(requestSeq, command, json::object());
        }
        else if (command == "stackTrace")
        {
            if (!args)
            {
                sendErrorResponse(requestSeq, command, "arguments required");
                return;
            }
            auto stArgs = args->get<dap::StackTraceArgs>();
            auto result = runtime->stackTrace(stArgs);
            sendResponse(requestSeq, command, result);
        }
        else if (command == "scopes")
        {
            if (!args)
            {
                sendErrorResponse(requestSeq, command, "arguments required");
                return;
            }
            auto sArgs = args->get<dap::ScopesArgs>();
            auto result = runtime->scopes(sArgs);
            sendResponse(requestSeq, command, result);
        }
        else if (command == "variables")
        {
            if (!args)
            {
                sendErrorResponse(requestSeq, command, "arguments required");
                return;
            }
            auto vArgs = args->get<dap::VariablesArgs>();
            auto result = runtime->variables(vArgs);
            sendResponse(requestSeq, command, result);
        }
        else if (command == "evaluate")
        {
            if (!args)
            {
                sendErrorResponse(requestSeq, command, "arguments required");
                return;
            }
            auto eArgs = args->get<dap::EvaluateArgs>();
            auto result = runtime->evaluate(eArgs);
            sendResponse(requestSeq, command, result);
        }
        else if (command == "disconnect")
        {
            runtime->disconnect();
            sendResponse(requestSeq, command, json::object());
            shutdownRequested.store(true);
            messagesCv.notify_all();
        }
        else
        {
            sendErrorResponse(requestSeq, command, "command not supported: " + command);
        }
    }
    catch (const std::exception& e)
    {
        sendErrorResponse(requestSeq, command, e.what());
    }
}

// ── Event senders (called from runtime thread) ────────────────────────────────

void DAPServer::sendStoppedEvent(const dap::StoppedEvent& e)
{
    sendEvent("stopped", e);
}

void DAPServer::sendContinuedEvent(int threadId)
{
    sendEvent("continued", json{{"threadId", threadId}, {"allThreadsContinued", true}});
}

void DAPServer::sendTerminatedEvent()
{
    sendEvent("terminated", json::object());
    shutdownRequested.store(true);
    messagesCv.notify_all();
}

void DAPServer::sendOutputEvent(const dap::OutputEvent& e)
{
    sendEvent("output", e);
}

// ── Wire helpers ──────────────────────────────────────────────────────────────

void DAPServer::sendResponse(int requestSeq, const std::string& command, const json& body)
{
    sendRaw(json{
        {"type", "response"},
        {"request_seq", requestSeq},
        {"success", true},
        {"command", command},
        {"body", body},
    });
}

void DAPServer::sendErrorResponse(int requestSeq, const std::string& command, const std::string& message)
{
    sendRaw(json{
        {"type", "response"},
        {"request_seq", requestSeq},
        {"success", false},
        {"command", command},
        {"message", message},
    });
}

void DAPServer::sendEvent(const std::string& eventName, const json& body)
{
    sendRaw(json{
        {"type", "event"},
        {"event", eventName},
        {"body", body},
    });
}

void DAPServer::sendRaw(json msg)
{
    std::lock_guard<std::mutex> lock(seqMutex);
    msg["seq"] = nextSeq++;
    json_rpc::sendRawMessage(transport, msg);
}

} // namespace debug
