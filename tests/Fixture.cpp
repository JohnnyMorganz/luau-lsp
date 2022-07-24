// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#include "Fixture.h"

#include "Luau/Parser.h"

#include "doctest.h"
#include <string_view>

static const char* mainModuleName = "MainModule";

std::optional<Luau::ModuleInfo> TestFileResolver::resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* expr)
{
    if (Luau::AstExprGlobal* g = expr->as<Luau::AstExprGlobal>())
    {
        if (g->name == "game")
            return Luau::ModuleInfo{"game"};
        if (g->name == "workspace")
            return Luau::ModuleInfo{"workspace"};
        if (g->name == "script")
            return context ? std::optional<Luau::ModuleInfo>(*context) : std::nullopt;
    }
    else if (Luau::AstExprIndexName* i = expr->as<Luau::AstExprIndexName>(); i && context)
    {
        if (i->index == "Parent")
        {
            std::string_view view = context->name;
            size_t lastSeparatorIndex = view.find_last_of('/');

            if (lastSeparatorIndex == std::string_view::npos)
                return std::nullopt;

            return Luau::ModuleInfo{Luau::ModuleName(view.substr(0, lastSeparatorIndex)), context->optional};
        }
        else
        {
            return Luau::ModuleInfo{context->name + '/' + i->index.value, context->optional};
        }
    }
    else if (Luau::AstExprIndexExpr* i = expr->as<Luau::AstExprIndexExpr>(); i && context)
    {
        if (Luau::AstExprConstantString* index = i->index->as<Luau::AstExprConstantString>())
        {
            return Luau::ModuleInfo{context->name + '/' + std::string(index->value.data, index->value.size), context->optional};
        }
    }
    else if (Luau::AstExprCall* call = expr->as<Luau::AstExprCall>(); call && call->self && call->args.size >= 1 && context)
    {
        if (Luau::AstExprConstantString* index = call->args.data[0]->as<Luau::AstExprConstantString>())
        {
            Luau::AstName func = call->func->as<Luau::AstExprIndexName>()->index;

            if (func == "GetService" && context->name == "game")
                return Luau::ModuleInfo{"game/" + std::string(index->value.data, index->value.size)};
        }
    }

    return std::nullopt;
}

Fixture::Fixture()
    : frontend(&fileResolver, &configResolver, {/* retainFullTypeGraphs= */ true})
{
    configResolver.defaultConfig.mode = Luau::Mode::Strict;
    configResolver.defaultConfig.parseOptions.captureComments = true;

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

Luau::CheckResult Fixture::check(Luau::Mode mode, std::string source)
{
    Luau::ModuleName mm = fromString(mainModuleName);
    configResolver.defaultConfig.mode = mode;
    fileResolver.source[mm] = std::move(source);
    frontend.markDirty(mm);

    return frontend.check(mm);
}


Luau::CheckResult Fixture::check(const std::string& source)
{
    return check(Luau::Mode::Strict, source);
}

Luau::ModulePtr Fixture::getMainModule()
{
    return frontend.moduleResolver.getModule(fromString(mainModuleName));
}

Luau::SourceModule* Fixture::getMainSourceModule()
{
    return frontend.getSourceModule(fromString(mainModuleName));
}