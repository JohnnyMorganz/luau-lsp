#include "doctest.h"
#include "LSP/LanguageServer.hpp"
#include "Protocol/Lifecycle.hpp"

TEST_SUITE_BEGIN("LanguageServer");

LUAU_FASTFLAG(LuauSolverV2);

TEST_CASE("language_server_handles_fflags_in_initialization_options")
{
    auto client = std::make_shared<Client>();
    LanguageServer server(client, std::nullopt);

    InitializationOptions initializationOptions{};
    initializationOptions.fflags.insert_or_assign("LuauSolverV2", "True");

    lsp::InitializeParams params;
    params.initializationOptions = initializationOptions;
    server.onRequest(0, "initialize", params);

    CHECK_EQ(FFlag::LuauSolverV2.value, true);

    // NOTE: Setting FFlags can virally affect other tests! We must reset here
    FFlag::LuauSolverV2.value = false;
}

TEST_SUITE_END();
