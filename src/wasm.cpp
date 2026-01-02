#include <stdio.h>
#include <string>
#include <memory>
#include <algorithm>

#include <emscripten/emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "include/LSP/ServerIOWasm.hpp"
#include "include/LSP/LanguageServer.hpp"
#include "include/LSP/Client.hpp"
#include "include/Platform/LSPPlatform.hpp"
#include "include/Platform/MonacoPlatform.hpp"

using emscripten::val;

// probably a better way to do this
std::vector<std::filesystem::path> translatePathVec(const std::vector<std::string> & inputs) {
    std::vector<std::filesystem::path> output(inputs.begin(), inputs.end());
    return output;
}

std::unique_ptr<LSPPlatform> forceMonacoPlatform(const ClientConfiguration& config, WorkspaceFileResolver* fileResolver, WorkspaceFolder* workspaceFolder) {
    return std::make_unique<MonacoPlatform>(fileResolver, workspaceFolder);
}

std::shared_ptr<LanguageServer> createWasmLanguageServer(const val & output, const std::string & packageName, const val & definitionsFiles, const val & documentationFiles) {
    // Older versions of the Monaco-LSP library do not adequately support configuring the platform.
    // This is sad, but we can handle this problem by providing an overload which is invoked here,
    // where we know the output is wasm.
    // Unfortunately, LSPPlatform determination is pretty deep in the callstack, and presumes that
    // platform information can only come from configuration. So, we introduce a static method as
    // a de facto LSPPlatform factory. We can and should better parameterize this.
    LSPPlatform::forcePlatform(forceMonacoPlatform);
    auto io = std::make_shared<ServerIOWasm>(output);
    // there has to be a better way to do this
    auto defs = emscripten::vecFromJSArray<std::string>(definitionsFiles);
    auto docs = emscripten::vecFromJSArray<std::string>(documentationFiles);
    auto defsPtr = std::make_shared<std::vector<std::filesystem::path>>(translatePathVec(defs));
    auto client = std::make_shared<Client>(io, defsPtr, translatePathVec(docs));
    auto server = std::make_shared<LanguageServer>(client, std::nullopt, packageName);
    return server;
}

EMSCRIPTEN_BINDINGS(languageServerBindings) {
    emscripten::function("createWasmLanguageServer", &createWasmLanguageServer, emscripten::return_value_policy::take_ownership());

    emscripten::class_<LanguageServer>("LanguageServer")
            .smart_ptr<std::shared_ptr<LanguageServer>>("LanguageServer")
            .function("processInput", &LanguageServer::processInput);
}