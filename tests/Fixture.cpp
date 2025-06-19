// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Fixture.h"

#include "Platform/RobloxPlatform.hpp"
#include "Luau/Parser.h"
#include "Luau/BuiltinDefinitions.h"
#include "LuauFileUtils.hpp"
#include "LSP/LuauExt.hpp"
#include "Flags.hpp"

#include "TestClient.h"

#include "doctest.h"
#include <string_view>

static const char* mainModuleName = "MainModule";

LUAU_FASTFLAG(LuauSolverV2)

namespace Luau::LanguageServer
{
ClientConfiguration defaultTestClientConfiguration()
{
    ClientConfiguration config;
    config.sourcemap.enabled = false;
    config.index.enabled = false;
    return config;
}

Uri newDocument(WorkspaceFolder& workspace, const std::string& name, const std::string& source)
{
    Uri uri = workspace.rootUri.resolvePath(name);
    workspace.openTextDocument(uri, {{uri, "luau", 0, source}});
    workspace.frontend.parse(workspace.fileResolver.getModuleName(uri));
    return uri;
}

void updateDocument(WorkspaceFolder& workspace, const Uri& uri, const std::string& newSource)
{
    lsp::DidChangeTextDocumentParams params;
    params.textDocument = {{uri}, 0};
    params.contentChanges = {{std::nullopt, newSource}};

    workspace.updateTextDocument(uri, params);
}
} // namespace Luau::LanguageServer

Fixture::Fixture()
    : client(std::make_unique<TestClient>(TestClient{}))
    , workspace(client.get(), "$TEST_WORKSPACE", Uri::file(*Luau::FileUtils::getCurrentWorkingDirectory()), std::nullopt)
{
    client->globalConfig = Luau::LanguageServer::defaultTestClientConfiguration();
    workspace.fileResolver.defaultConfig.mode = Luau::Mode::Strict;
    client->definitionsFiles.push_back("./tests/testdata/standard_definitions.d.luau");
    workspace.setupWithConfiguration(client->globalConfig);
    workspace.isReady = true;

    Luau::setPrintLine([](auto s) {});
}

Fixture::~Fixture()
{
    Luau::resetPrintLine();
}

Luau::ModuleName fromString(std::string_view name)
{
    return Luau::ModuleName(name);
}

Uri Fixture::newDocument(const std::string& name, const std::string& source)
{
    return Luau::LanguageServer::newDocument(workspace, name, source);
}

/// A hacky way to get cross-module resolution working.
/// We create a dummy sourcemap node for the particular Uri, which then allows
/// requires to resolve. e.g. registering "game/Testing/A" will allow `require(game.Testing.A`) to work
void Fixture::registerDocumentForVirtualPath(const Uri& uri, const Luau::ModuleName& virtualPath)
{
    auto platform = dynamic_cast<RobloxPlatform*>(workspace.platform.get());
    LUAU_ASSERT(platform);
    auto sourceNode = platform->sourceNodeAllocator.allocate(SourceNode(uri.filename(), "ModuleScript", {uri.fsPath()}, {}));
    platform->writePathsToMap(sourceNode, virtualPath);
}

void Fixture::updateDocument(const Uri& uri, const std::string& newSource)
{
    return Luau::LanguageServer::updateDocument(workspace, uri, newSource);
}

static Luau::ModuleName getMainModuleName(const WorkspaceFolder& workspace)
{
    return workspace.fileResolver.getModuleName(workspace.rootUri.resolvePath(mainModuleName));
}

Luau::AstStatBlock* Fixture::parse(const std::string& source, const Luau::ParseOptions& parseOptions)
{
    sourceModule.reset(new Luau::SourceModule);

    Luau::ParseResult result = Luau::Parser::parse(source.c_str(), source.length(), *sourceModule->names, *sourceModule->allocator, parseOptions);

    sourceModule->name = getMainModuleName(workspace);
    sourceModule->root = result.root;
    sourceModule->mode = parseMode(result.hotcomments);
    sourceModule->hotcomments = std::move(result.hotcomments);

    return result.root;
}

Luau::CheckResult Fixture::check(Luau::Mode mode, std::string source)
{
    newDocument(mainModuleName, source);
    return workspace.frontend.check(getMainModuleName(workspace));
}

Luau::CheckResult Fixture::check(const std::string& source)
{
    return check(Luau::Mode::Strict, source);
}

Luau::ModulePtr Fixture::getModule(const Luau::ModuleName& moduleName)
{
    return workspace.frontend.moduleResolver.getModule(moduleName);
}

Luau::ModulePtr Fixture::getMainModule()
{
    return getModule(getMainModuleName(workspace));
}

Luau::SourceModule* Fixture::getMainSourceModule()
{
    return workspace.frontend.getSourceModule(getMainModuleName(workspace));
}

std::vector<std::string> Fixture::getComments(const Luau::Location& node)
{
    return workspace.getComments(getMainModuleName(workspace), node);
}

std::optional<Luau::TypeId> lookupName(Luau::ScopePtr scope, const std::string& name)
{
    auto binding = scope->linearSearchForBinding(name);
    if (binding)
        return binding->typeId;
    else
        return std::nullopt;
}

std::optional<Luau::TypeId> Fixture::getType(Luau::ModulePtr module, const std::string& name)
{
    REQUIRE(module);
    REQUIRE(module->hasModuleScope());

    return lookupName(module->getModuleScope(), name);
}

Luau::TypeId Fixture::requireType(Luau::ModulePtr module, const std::string& name)
{
    std::optional<Luau::TypeId> ty = getType(module, name);
    REQUIRE_MESSAGE(bool(ty), "Unable to requireType \"" << name << "\"");
    return Luau::follow(*ty);
}

Luau::LoadDefinitionFileResult Fixture::loadDefinition(const std::string& source, bool forAutocomplete)
{
    RobloxPlatform platform;

    forAutocomplete = forAutocomplete && !FFlag::LuauSolverV2;
    auto& globals = forAutocomplete ? workspace.frontend.globalsForAutocomplete : workspace.frontend.globals;

    Luau::unfreeze(globals.globalTypes);
    Luau::LoadDefinitionFileResult result = types::registerDefinitions(workspace.frontend, globals, source);
    platform.mutateRegisteredDefinitions(globals, std::nullopt);
    Luau::freeze(globals.globalTypes);

    REQUIRE_MESSAGE(result.success, "loadDefinition: unable to load definition file");
    return result;
}

void Fixture::dumpErrors(std::ostream& os, const std::vector<Luau::TypeError>& errors)
{
    for (const auto& error : errors)
    {
        os << std::endl;
        os << "Error: " << error << std::endl;

        // TODO: show errors in source
    }
}

std::string Fixture::getErrors(const Luau::CheckResult& cr)
{
    std::stringstream ss;
    dumpErrors(ss, cr.errors);
    return ss.str();
}

void Fixture::loadSourcemap(const std::string& contents)
{
    dynamic_cast<RobloxPlatform*>(workspace.platform.get())->updateSourceMapFromContents(contents);
}

void Fixture::loadLuaurc(const std::string& source)
{
    REQUIRE(!WorkspaceFileResolver::parseConfig(workspace.rootUri.resolvePath(Luau::kConfigName), source, workspace.fileResolver.defaultConfig)
                 .has_value());
}

SourceNode* Fixture::getRootSourceNode()
{
    auto sourceNode = dynamic_cast<RobloxPlatform*>(workspace.platform.get())->rootSourceNode;
    REQUIRE(sourceNode);
    return sourceNode;
}

std::pair<std::string, lsp::Position> sourceWithMarker(std::string source)
{
    auto marker = source.find('|');
    REQUIRE(marker != std::string::npos);

    source.replace(marker, 1, "");

    size_t line = 0;
    size_t column = 0;

    for (size_t i = 0; i < source.size(); i++)
    {
        auto ch = source[i];
        if (ch == '\r' || ch == '\n')
        {
            if (ch == '\r' && i + 1 < source.size() && source[i + 1] == '\n')
            {
                i++;
            }
            line += 1;
            column = 0;
        }
        else
            column += 1;

        if (i == marker - 1)
            break;
    }

    return std::make_pair(source, lsp::Position{line, column});
}
