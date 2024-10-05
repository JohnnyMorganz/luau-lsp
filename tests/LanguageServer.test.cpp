#include "doctest.h"
#include "LSP/LanguageServer.hpp"
#include "Protocol/Lifecycle.hpp"

TEST_SUITE_BEGIN("LanguageServer");

LUAU_FASTFLAG(DebugLuauTimeTracing);

TEST_CASE("language_server_handles_fflags_in_initialization_options")
{
    auto client = std::make_shared<Client>();
    LanguageServer server(client, std::nullopt);

    InitializationOptions initializationOptions{};
    initializationOptions.fflags.insert_or_assign("DebugLuauTimeTracing", "True");

    lsp::InitializeParams params;
    params.initializationOptions = initializationOptions;
    server.onRequest(0, "initialize", params);

    CHECK_EQ(FFlag::DebugLuauTimeTracing.value, true);

    // NOTE: Setting FFlags can virally affect other tests! We must reset here
    FFlag::DebugLuauTimeTracing.value = false;
}

TEST_SUITE_END();
