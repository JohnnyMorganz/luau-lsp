#include "doctest.h"
#include "LSP/LanguageServer.hpp"
#include "Protocol/Lifecycle.hpp"
#include "Fixture.h"

TEST_SUITE_BEGIN("LanguageServer");

LUAU_FASTFLAG(DebugLuauDeferredConstraintResolution);

TEST_CASE("language_server_handles_fflags_in_initialization_options")
{
    auto client = Fixture::makeClient();
    LanguageServer server(client, std::nullopt);

    InitializationOptions initializationOptions{};
    initializationOptions.fflags.insert_or_assign("DebugLuauDeferredConstraintResolution", "True");

    lsp::InitializeParams params;
    params.initializationOptions = initializationOptions;
    server.onRequest(0, "initialize", params);

    CHECK_EQ(FFlag::DebugLuauDeferredConstraintResolution.value, true);

    // NOTE: Setting FFlags can virally affect other tests! We must reset here
    FFlag::DebugLuauDeferredConstraintResolution.value = false;
}

TEST_SUITE_END();
