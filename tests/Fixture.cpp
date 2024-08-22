// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Fixture.h"

#include "Platform/RobloxPlatform.hpp"
#include "Luau/Parser.h"
#include "Luau/BuiltinDefinitions.h"
#include "LSP/LuauExt.hpp"

#include "doctest.h"
#include <string_view>

static const char* mainModuleName = "MainModule";

Fixture::Fixture()
    : client(std::make_shared<Client>(Client{}))
    , workspace(client, "$TEST_WORKSPACE", Uri(), std::nullopt)
{
    workspace.fileResolver.defaultConfig.mode = Luau::Mode::Strict;
    client->definitionsFiles.push_back("./tests/testdata/standard_definitions.d.luau");

    workspace.initialize();

    ClientConfiguration config;
    config.sourcemap.enabled = false;
    config.index.enabled = false;
    workspace.setupWithConfiguration(config);

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
    Uri uri("file", "", name);
    workspace.openTextDocument(uri, {{uri, "luau", 0, source}});
    return uri;
}

Luau::AstStatBlock* Fixture::parse(const std::string& source, const Luau::ParseOptions& parseOptions)
{
    sourceModule.reset(new Luau::SourceModule);

    Luau::ParseResult result = Luau::Parser::parse(source.c_str(), source.length(), *sourceModule->names, *sourceModule->allocator, parseOptions);

    sourceModule->name = fromString(mainModuleName);
    sourceModule->root = result.root;
    sourceModule->mode = parseMode(result.hotcomments);
    sourceModule->hotcomments = std::move(result.hotcomments);

    return result.root;
}

Luau::CheckResult Fixture::check(Luau::Mode mode, std::string source)
{
    newDocument(mainModuleName, source);
    return workspace.frontend.check(mainModuleName);
}

Luau::CheckResult Fixture::check(const std::string& source)
{
    return check(Luau::Mode::Strict, source);
}

Luau::ModulePtr Fixture::getMainModule()
{
    return workspace.frontend.moduleResolver.getModule(fromString(mainModuleName));
}

Luau::SourceModule* Fixture::getMainSourceModule()
{
    return workspace.frontend.getSourceModule(fromString(mainModuleName));
}

std::vector<std::string> Fixture::getComments(const Luau::Location& node)
{
    return workspace.getComments(fromString(mainModuleName), node);
}

std::optional<Luau::TypeId> lookupName(Luau::ScopePtr scope, const std::string& name)
{
    auto binding = scope->linearSearchForBinding(name);
    if (binding)
        return binding->typeId;
    else
        return std::nullopt;
}

std::optional<Luau::TypeId> Fixture::getType(const std::string& name)
{
    Luau::ModulePtr module = getMainModule();
    REQUIRE(module);
    REQUIRE(module->hasModuleScope());

    return lookupName(module->getModuleScope(), name);
}

Luau::TypeId Fixture::requireType(const std::string& name)
{
    std::optional<Luau::TypeId> ty = getType(name);
    REQUIRE_MESSAGE(bool(ty), "Unable to requireType \"" << name << "\"");
    return Luau::follow(*ty);
}

Luau::LoadDefinitionFileResult Fixture::loadDefinition(const std::string& source, bool forAutocomplete)
{
    RobloxPlatform platform;

    auto& globals = forAutocomplete ? workspace.frontend.globalsForAutocomplete : workspace.frontend.globals;

    Luau::unfreeze(globals.globalTypes);
    Luau::LoadDefinitionFileResult result = types::registerDefinitions(workspace.frontend, globals, source, forAutocomplete);
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

SourceNodePtr Fixture::getRootSourceNode()
{
    auto sourceNode = dynamic_cast<RobloxPlatform*>(workspace.platform.get())->rootSourceNode;
    REQUIRE(sourceNode);
    return sourceNode;
}
