#include "Platform/RotrieverResolver.hpp"
#include "LuauFileUtils.hpp"

#include "Luau/Ast.h"
#include "Luau/Parser.h"

#include "toml11/toml.hpp"

#include <iostream>
#include <sstream>

namespace Luau::LanguageServer
{

std::optional<RotrieverPackage> RotrieverResolver::parseManifest(const Uri& manifestPath)
{
    // Read the file
    auto contents = Luau::FileUtils::readFile(manifestPath.fsPath());
    if (!contents)
    {
        std::cerr << "RotrieverResolver: Failed to read " << manifestPath.fsPath() << std::endl;
        return std::nullopt;
    }

    try
    {
        // Parse TOML
        std::istringstream stream(*contents);
        auto data = toml::parse(stream, manifestPath.fsPath());

        RotrieverPackage package;
        package.packageRoot = manifestPath.parent().value_or(manifestPath);

        // Parse [package] section
        if (data.contains("package"))
        {
            auto& pkg = data.at("package");

            if (pkg.contains("name"))
                package.name = toml::find<std::string>(pkg, "name");

            if (pkg.contains("version"))
                package.version = toml::find<std::string>(pkg, "version");

            if (pkg.contains("content_root"))
                package.contentRoot = toml::find<std::string>(pkg, "content_root");
            else
                package.contentRoot = "src"; // Default
        }

        // Parse [dependencies]
        if (data.contains("dependencies"))
        {
            auto& deps = data.at("dependencies");
            if (deps.is_table())
            {
                for (const auto& [name, value] : deps.as_table())
                {
                    RotrieverDependency dep;
                    dep.name = name;

                    if (value.is_table() && value.as_table().count("path"))
                    {
                        dep.path = toml::find<std::string>(value, "path");
                        dep.resolvedPath = package.packageRoot.resolvePath(dep.path);
                    }

                    package.dependencies[name] = std::move(dep);
                }
            }
        }

        // Parse [dev_dependencies]
        if (data.contains("dev_dependencies"))
        {
            auto& deps = data.at("dev_dependencies");
            if (deps.is_table())
            {
                for (const auto& [name, value] : deps.as_table())
                {
                    RotrieverDependency dep;
                    dep.name = name;

                    if (value.is_table() && value.as_table().count("path"))
                    {
                        dep.path = toml::find<std::string>(value, "path");
                        dep.resolvedPath = package.packageRoot.resolvePath(dep.path);
                    }

                    package.devDependencies[name] = std::move(dep);
                }
            }
        }

        return package;
    }
    catch (const std::exception& e)
    {
        std::cerr << "RotrieverResolver: Failed to parse " << manifestPath.fsPath() << ": " << e.what() << std::endl;
        return std::nullopt;
    }
}

// Check if an expression is require(script.X) and return X if so
static std::optional<std::string> getScriptRequireName(Luau::AstExpr* expr)
{
    auto* call = expr->as<Luau::AstExprCall>();
    if (!call || call->args.size != 1)
        return std::nullopt;

    // Check for require(...)
    auto* requireGlobal = call->func->as<Luau::AstExprGlobal>();
    if (!requireGlobal || std::string(requireGlobal->name.value) != "require")
        return std::nullopt;

    // Check for script.X
    auto* indexExpr = call->args.data[0]->as<Luau::AstExprIndexName>();
    if (!indexExpr)
        return std::nullopt;

    auto* scriptGlobal = indexExpr->expr->as<Luau::AstExprGlobal>();
    if (!scriptGlobal || std::string(scriptGlobal->name.value) != "script")
        return std::nullopt;

    return std::string(indexExpr->index.value);
}

// Recursively parse exports from an init.lua file
// prefix: the key prefix for nested exports (e.g., "Events" -> "Events.gamePlayIntent")
// maxDepth: prevent infinite recursion
static void parseExportsRecursive(
    const Uri& initLuaPath, const std::string& prefix, std::vector<std::string>& valueExports, std::vector<std::string>& typeExports, int maxDepth)
{
    if (maxDepth <= 0)
        return;

    // Read the file
    auto contents = Luau::FileUtils::readFile(initLuaPath.fsPath());
    if (!contents)
        return;

    try
    {
        // Parse the Lua file
        Luau::Allocator allocator;
        Luau::AstNameTable names(allocator);
        Luau::ParseResult result = Luau::Parser::parse(contents->c_str(), contents->size(), names, allocator);

        if (!result.errors.empty())
            return;

        // Walk the AST to find exported types and return statements
        for (Luau::AstStat* stat : result.root->body)
        {
            // Check for export type declarations
            if (auto* typeAlias = stat->as<Luau::AstStatTypeAlias>())
            {
                if (typeAlias->exported)
                {
                    std::string typeName(typeAlias->name.value);
                    std::string fullTypeName = prefix.empty() ? typeName : (prefix + "." + typeName);
                    typeExports.push_back(fullTypeName);
                }
                continue;
            }
        }

        // Second pass: find return statements for value exports
        for (Luau::AstStat* stat : result.root->body)
        {
            auto* returnStat = stat->as<Luau::AstStatReturn>();
            if (!returnStat)
                continue;

            // Check if we have a single return value that's a table
            if (returnStat->list.size != 1)
                continue;

            auto* tableExpr = returnStat->list.data[0]->as<Luau::AstExprTable>();
            if (!tableExpr)
                continue;

            // Extract the keys from the table
            for (const auto& item : tableExpr->items)
            {
                // We only care about Record items (foo = bar)
                if (item.kind != Luau::AstExprTable::Item::Record)
                    continue;

                // The key should be a constant string for Record items
                auto* keyStr = item.key->as<Luau::AstExprConstantString>();
                if (!keyStr)
                    continue;

                std::string key(keyStr->value.data, keyStr->value.size);
                std::string fullKey = prefix.empty() ? key : (prefix + "." + key);

                // Always add the direct export
                valueExports.push_back(fullKey);

                // Check if the value is require(script.X) - indicating a sub-module
                if (auto subModuleName = getScriptRequireName(item.value))
                {
                    // Recursively parse the sub-module
                    // The sub-module should be at parentDir/subModuleName/init.lua
                    auto parentDir = initLuaPath.parent();
                    if (parentDir)
                    {
                        auto subModulePath = parentDir->resolvePath(*subModuleName + "/init.lua");
                        parseExportsRecursive(subModulePath, fullKey, valueExports, typeExports, maxDepth - 1);
                    }
                }
            }

            // We found the return statement, no need to continue
            break;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "RotrieverResolver: Exception parsing " << initLuaPath.fsPath() << ": " << e.what() << std::endl;
    }
}

RotrieverExports RotrieverResolver::parseExports(const Uri& initLuaPath)
{
    RotrieverExports exports;
    // Parse with max depth of 3 to prevent excessive recursion
    parseExportsRecursive(initLuaPath, "", exports.values, exports.types, 3);
    return exports;
}

void RotrieverResolver::debugPrint(const RotrieverPackage& package)
{
    std::cerr << "=== Rotriever Package ===" << std::endl;
    std::cerr << "Name: " << package.name << std::endl;
    std::cerr << "Version: " << package.version << std::endl;
    std::cerr << "Content Root: " << package.contentRoot << std::endl;
    std::cerr << "Package Root: " << package.packageRoot.fsPath() << std::endl;

    std::cerr << "\nDependencies (" << package.dependencies.size() << "):" << std::endl;
    for (const auto& [name, dep] : package.dependencies)
    {
        std::cerr << "  " << name << ": " << dep.path << " -> " << dep.resolvedPath.fsPath() << std::endl;
    }

    std::cerr << "\nDev Dependencies (" << package.devDependencies.size() << "):" << std::endl;
    for (const auto& [name, dep] : package.devDependencies)
    {
        std::cerr << "  " << name << ": " << dep.path << " -> " << dep.resolvedPath.fsPath() << std::endl;
    }

    std::cerr << "\nExports (" << package.exports.size() << "):" << std::endl;
    for (const auto& exp : package.exports)
    {
        std::cerr << "  " << exp << std::endl;
    }

    std::cerr << "=========================" << std::endl;
}

} // namespace Luau::LanguageServer
