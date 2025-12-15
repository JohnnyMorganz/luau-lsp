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

std::vector<std::string> RotrieverResolver::extractExportsFromAst(Luau::AstStatBlock* root)
{
    std::vector<std::string> exports;

    if (!root)
        return exports;

    // Walk the AST to find return statements at the top level
    for (Luau::AstStat* stat : root->body)
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
            if (auto* keyStr = item.key->as<Luau::AstExprConstantString>())
            {
                std::string key(keyStr->value.data, keyStr->value.size);
                exports.push_back(std::move(key));
            }
        }

        // We found the return statement, no need to continue
        break;
    }

    return exports;
}

std::vector<std::string> RotrieverResolver::parseExports(const Uri& initLuaPath)
{
    // Read the file
    auto contents = Luau::FileUtils::readFile(initLuaPath.fsPath());
    if (!contents)
    {
        std::cerr << "RotrieverResolver: Failed to read init.lua at " << initLuaPath.fsPath() << std::endl;
        return {};
    }

    try
    {
        // Parse the Lua file
        Luau::Allocator allocator;
        Luau::AstNameTable names(allocator);
        Luau::ParseResult result = Luau::Parser::parse(contents->c_str(), contents->size(), names, allocator);

        if (!result.errors.empty())
        {
            std::cerr << "RotrieverResolver: Parse errors in " << initLuaPath.fsPath() << std::endl;
            return {};
        }

        return RotrieverResolver::extractExportsFromAst(result.root);
    }
    catch (const std::exception& e)
    {
        std::cerr << "RotrieverResolver: Exception parsing " << initLuaPath.fsPath() << ": " << e.what() << std::endl;
    }

    return {};
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
