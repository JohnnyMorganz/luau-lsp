// This file is part of the Luau programming language and is licensed under MIT License; see LICENSE.txt for details
#pragma once

#include "Luau/Config.h"
#include "Luau/FileResolver.h"
#include "Luau/Frontend.h"
#include "Luau/IostreamHelpers.h"
#include "Luau/Linter.h"
#include "Luau/Location.h"
#include "Luau/ModuleResolver.h"
#include "Luau/Scope.h"
#include "Luau/ToString.h"
#include "Luau/TypeInfer.h"
#include "Luau/TypeVar.h"

#include <iostream>
#include <string>
#include <unordered_map>

#include <optional>

struct TestFileResolver
    : Luau::FileResolver
    , Luau::ModuleResolver
{
    std::optional<Luau::ModuleInfo> resolveModuleInfo(const Luau::ModuleName& currentModuleName, const Luau::AstExpr& pathExpr) override
    {
        if (auto name = Luau::pathExprToModuleName(currentModuleName, pathExpr))
            return {{*name, false}};

        return std::nullopt;
    }

    const Luau::ModulePtr getModule(const Luau::ModuleName& moduleName) const override
    {
        LUAU_ASSERT(false);
        return nullptr;
    }

    bool moduleExists(const Luau::ModuleName& moduleName) const override
    {
        auto it = source.find(moduleName);
        return (it != source.end());
    }

    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
    {
        auto it = source.find(name);
        if (it == source.end())
            return std::nullopt;

        Luau::SourceCode::Type sourceType = Luau::SourceCode::Module;

        auto it2 = sourceTypes.find(name);
        if (it2 != sourceTypes.end())
            sourceType = it2->second;

        return Luau::SourceCode{it->second, sourceType};
    }

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* expr) override;

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override
    {
        return name;
    }

    std::unordered_map<Luau::ModuleName, std::string> source;
    std::unordered_map<Luau::ModuleName, Luau::SourceCode::Type> sourceTypes;
    std::unordered_map<Luau::ModuleName, std::string> environments;
};

struct Fixture
{
    std::unique_ptr<Luau::SourceModule> sourceModule;
    TestFileResolver fileResolver;
    Luau::NullConfigResolver configResolver;
    Luau::Frontend frontend;

    explicit Fixture();
    ~Fixture();

    Luau::AstStatBlock* parse(const std::string& source, const Luau::ParseOptions& parseOptions = {});
    Luau::CheckResult check(Luau::Mode mode, std::string source);
    Luau::CheckResult check(const std::string& source);

    Luau::ModulePtr getMainModule();
    Luau::SourceModule* getMainSourceModule();
};
