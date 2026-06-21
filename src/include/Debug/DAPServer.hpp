#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <atomic>

#include "Protocol/DAP.hpp"
#include "Debug/DebugRuntime.hpp"
#include "LSP/Transport/Transport.hpp"
#include "Thread.hpp"

namespace debug
{

// DAPServer handles the DAP wire protocol (Content-Length framing over stdio/pipe)
// and delegates debug operations to an IDebugRuntime.
// Mirrors the LanguageServer pattern closely.
class DAPServer
{
public:
    explicit DAPServer(Transport* transport, std::unique_ptr<IDebugRuntime> runtime);

    void processInputLoop();
    bool requestedShutdown() const;

    // Called by IDebugRuntime to push events to the editor
    void sendStoppedEvent(const dap::StoppedEvent& e);
    void sendContinuedEvent(int threadId);
    void sendTerminatedEvent();
    void sendOutputEvent(const dap::OutputEvent& e);

private:
    Transport* transport;
    std::unique_ptr<IDebugRuntime> runtime;
    std::atomic<bool> shutdownRequested{false};

    int nextSeq = 1;
    std::mutex seqMutex; // guards nextSeq and transport writes

    std::queue<dap::Message> messages;
    std::mutex messagesMutex;
    std::condition_variable messagesCv;
    Thread messageProcessorThread;

    void handleMessage(const dap::Message& msg);
    void processRequest(const dap::Message& msg);

    // Response helpers
    void sendResponse(int requestSeq, const std::string& command, const json& body);
    void sendErrorResponse(int requestSeq, const std::string& command, const std::string& message);
    void sendEvent(const std::string& eventName, const json& body);

    void sendRaw(json msg);

    std::optional<dap::Message> popMessage();
};

} // namespace debug
