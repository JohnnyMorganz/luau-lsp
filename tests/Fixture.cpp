// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Fixture.h"

#include "Luau/Parser.h"

#include "doctest.h"
#include <string_view>

static const char* mainModuleName = "MainModule";

Fixture::Fixture()
{
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

Luau::AstStatBlock* Fixture::parse(const std::string& source, const Luau::ParseOptions& parseOptions)
{
    sourceModule.reset(new Luau::SourceModule);

    Luau::ParseResult result = Luau::Parser::parse(source.c_str(), source.length(), *sourceModule->names, *sourceModule->allocator, parseOptions);

    sourceModule->name = fromString(mainModuleName);
    sourceModule->root = result.root;
    sourceModule->mode = Luau::parseMode(result.hotcomments);
    sourceModule->hotcomments = std::move(result.hotcomments);

    if (!result.errors.empty())
    {
        throw Luau::ParseErrors(result.errors);
    }

    return result.root;
}
