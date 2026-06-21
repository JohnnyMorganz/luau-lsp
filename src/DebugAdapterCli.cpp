#include "DebugAdapterCli.hpp"

#include "Debug/DAPServer.hpp"
#include "Debug/DebugRuntime.hpp"

#include "LSP/Transport/StdioTransport.hpp"
#ifndef _WIN32
#include "LSP/Transport/PipeTransport.hpp"
#include "Debug/RobloxStudioBridge.hpp"
#endif

#include <iostream>
#include <memory>

int startDebugAdapter(const argparse::ArgumentParser& program)
{
    // Select transport (stdio or pipe)
    std::unique_ptr<Transport> transport;

    auto transportPipeFile = program.present<std::string>("--pipe");
    if (transportPipeFile)
    {
        if (program.is_used("--stdio"))
        {
            std::cerr << "both --stdio and --pipe cannot be specified at the same time\n";
            return 1;
        }
#ifdef _WIN32
        std::cerr << "--pipe is not supported on windows\n";
        return 1;
#else
        transport = std::make_unique<PipeTransport>(*transportPipeFile);
#endif
    }
    else
    {
        transport = std::make_unique<StdioTransport>();
    }

    // Select debug runtime backend
    std::string platform = program.present<std::string>("--platform").value_or("roblox");
    std::unique_ptr<debug::IDebugRuntime> runtime;

    if (platform == "roblox")
    {
#ifndef _WIN32
        runtime = std::make_unique<debug::RobloxStudioBridge>();
#else
        std::cerr << "Roblox debug bridge is not yet supported on Windows\n";
        return 1;
#endif
    }
    else
    {
        std::cerr << "Unknown platform '" << platform << "'. Supported: roblox\n";
        return 1;
    }

    debug::DAPServer server(transport.get(), std::move(runtime));
    server.processInputLoop();

    return 0;
}
