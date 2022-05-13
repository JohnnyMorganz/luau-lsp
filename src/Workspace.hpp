#pragma once
#include "Luau/Frontend.h"
#include "Luau/Autocomplete.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ToString.h"
#include "Protocol.hpp"

static std::optional<Luau::AutocompleteEntryMap> nullCallback(std::string tag, std::optional<const Luau::ClassTypeVar*> ptr)
{
    return std::nullopt;
}

// Get the corresponding Luau module name for a file
Luau::ModuleName getModuleName(const std::string& name)
{
    return name;
}
Luau::ModuleName getModuleName(const std::filesystem::path& name)
{
    return name.generic_string();
}
Luau::ModuleName getModuleName(const Uri& name)
{
    return name.fsPath().generic_string();
}

std::optional<std::string> readFile(const std::filesystem::path& filePath)
{
    std::ifstream fileContents;
    fileContents.open(filePath);

    std::string output;
    std::stringstream buffer;

    if (fileContents)
    {
        buffer << fileContents.rdbuf();
        output = buffer.str();
        return output;
    }
    else
    {
        return std::nullopt;
    }
}

struct WorkspaceFileResolver
    : Luau::FileResolver
    , Luau::ConfigResolver
{
    Luau::Config defaultConfig;

    // Currently opened files where content is managed by client
    mutable std::unordered_map<Luau::ModuleName, std::string> managedFiles;
    mutable std::unordered_map<std::string, Luau::Config> configCache;
    mutable std::vector<std::pair<std::filesystem::path, std::string>> configErrors;

    WorkspaceFileResolver()
    {
        defaultConfig.mode = Luau::Mode::Nonstrict;
    }

    /// The file is managed by the client, so FS will be out of date
    bool isManagedFile(const Luau::ModuleName& name)
    {
        return managedFiles.find(name) != managedFiles.end();
    }

    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
    {
        Luau::SourceCode::Type sourceType = Luau::SourceCode::Module;
        std::optional<std::string> source;

        if (isManagedFile(name))
        {
            source = managedFiles.at(name);
        }
        else
        {
            source = readFile(name);
        }

        if (!source)
            return std::nullopt;

        return Luau::SourceCode{*source, sourceType};
    }

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override
    {
        if (Luau::AstExprConstantString* expr = node->as<Luau::AstExprConstantString>())
        {
            Luau::ModuleName name = std::string(expr->value.data, expr->value.size) + ".luau";
            if (!readFile(name))
            {
                // fall back to .lua if a module with .luau doesn't exist
                name = std::string(expr->value.data, expr->value.size) + ".lua";
            }

            return {{name}};
        }

        return std::nullopt;
    }

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override
    {
        return name;
    }

    const Luau::Config& getConfig(const Luau::ModuleName& name) const override
    {
        std::filesystem::path realPath = name;
        if (!realPath.has_parent_path())
            return defaultConfig;

        return readConfigRec(realPath.parent_path());
    }

    const Luau::Config& readConfigRec(const std::filesystem::path& path) const
    {
        auto it = configCache.find(path.generic_string());
        if (it != configCache.end())
            return it->second;

        Luau::Config result = (path.has_relative_path() && path.has_parent_path()) ? readConfigRec(path.parent_path()) : defaultConfig;
        auto configPath = path / Luau::kConfigName;

        if (std::optional<std::string> contents = readFile(configPath))
        {
            std::optional<std::string> error = Luau::parseConfig(*contents, result);
            if (error)
                configErrors.push_back({configPath, *error});
        }

        return configCache[path.generic_string()] = result;
    }
};

class WorkspaceFolder
{
public:
    std::string name;
    lsp::DocumentUri rootUri;

private:
    WorkspaceFileResolver fileResolver;
    Luau::Frontend frontend;

public:
    WorkspaceFolder()
        : fileResolver(WorkspaceFileResolver())
        , frontend(Luau::Frontend(&fileResolver, &fileResolver, {true}))
    {
        setup();
    }
    WorkspaceFolder(const std::string& name, const lsp::DocumentUri& uri)
        : name(name)
        , rootUri(uri)
        , fileResolver(WorkspaceFileResolver())
        , frontend(Luau::Frontend(&fileResolver, &fileResolver, {true}))
    {
        setup();
    }

    /// Checks whether a provided file is part of the workspace
    bool isInWorkspace(const lsp::DocumentUri& file)
    {
        // Check if the root uri is a prefix of the file
        auto prefixStr = rootUri.toString();
        auto checkStr = file.toString();
        if (checkStr.compare(0, prefixStr.size(), prefixStr) == 0)
        {
            return true;
        }
        return false;
    }

    void openTextDocument(const lsp::DocumentUri& uri, const std::string& text)
    {
        auto moduleName = getModuleName(uri);
        fileResolver.managedFiles.insert_or_assign(moduleName, text);
    }

    void updateTextDocument(const lsp::DocumentUri& uri, const std::vector<lsp::TextDocumentContentChangeEvent>& changes)
    {
        auto moduleName = getModuleName(uri);
        for (auto& change : changes)
        {
            // TODO: if range is present - we should update incrementally, currently we ask for full sync
            fileResolver.managedFiles.insert_or_assign(moduleName, change.text);
        }
        // Mark the module dirty for the typechecker
        frontend.markDirty(moduleName);
    }

    void closeTextDocument(const lsp::DocumentUri& uri)
    {
        auto moduleName = getModuleName(uri);
        fileResolver.managedFiles.erase(moduleName);
    }

    lsp::PublishDiagnosticsParams publishDiagnostics(const lsp::DocumentUri& uri, std::optional<int> version)
    {
        auto moduleName = getModuleName(uri);
        auto diagnostics = findDiagnostics(moduleName);
        return {uri, version, diagnostics};
    }

    std::vector<lsp::CompletionItem> completion(const lsp::CompletionParams& params)
    {
        auto result = Luau::autocomplete(
            frontend, getModuleName(params.textDocument.uri), Luau::Position(params.position.line, params.position.character), nullCallback);
        std::vector<lsp::CompletionItem> items;

        for (auto& [name, entry] : result.entryMap)
        {
            lsp::CompletionItem item;
            item.label = name;
            item.deprecated = entry.deprecated;
            item.documentation = entry.documentationSymbol; // TODO: eval doc symbol

            switch (entry.kind)
            {
            case Luau::AutocompleteEntryKind::Property:
                item.kind = lsp::CompletionItemKind::Field;
                break;
            case Luau::AutocompleteEntryKind::Binding:
                item.kind = lsp::CompletionItemKind::Variable;
                break;
            case Luau::AutocompleteEntryKind::Keyword:
                item.kind = lsp::CompletionItemKind::Keyword;
                break;
            case Luau::AutocompleteEntryKind::String:
                item.kind = lsp::CompletionItemKind::Constant; // TODO: is a string autocomplete always a singleton constant?
                break;
            case Luau::AutocompleteEntryKind::Type:
                item.kind = lsp::CompletionItemKind::Interface;
                break;
            case Luau::AutocompleteEntryKind::Module:
                item.kind = lsp::CompletionItemKind::Module;
                break;
            }

            // Handle parentheses suggestions
            if (entry.parens == Luau::ParenthesesRecommendation::CursorAfter)
            {
                item.insertText = name + "()$0";
                item.insertTextFormat = lsp::InsertTextFormat::Snippet;
            }
            else if (entry.parens == Luau::ParenthesesRecommendation::CursorInside)
            {
                item.insertText = name + "($1)$0";
                item.insertTextFormat = lsp::InsertTextFormat::Snippet;
            }

            if (entry.type.has_value())
            {
                auto id = Luau::follow(entry.type.value());
                // Try to infer more type info about the entry to provide better suggestion info
                if (auto ftv = Luau::get<Luau::FunctionTypeVar>(id))
                {
                    item.kind = lsp::CompletionItemKind::Function;
                }
                item.detail = Luau::toString(id);
            }

            items.emplace_back(item);
        }

        return items;
    }

private:
    void setup()
    {
        Luau::registerBuiltinTypes(frontend.typeChecker);
        Luau::freeze(frontend.typeChecker.globalTypes);
    }

    std::vector<lsp::Diagnostic> findDiagnostics(const Luau::ModuleName& fileName)
    {
        Luau::CheckResult cr;
        if (frontend.isDirty(fileName))
            cr = frontend.check(fileName);

        if (!frontend.getSourceModule(fileName))
        {
            lsp::Diagnostic errorDiagnostic;
            errorDiagnostic.source = "Luau";
            errorDiagnostic.code = "000";
            errorDiagnostic.message = "Failed to resolve source module for this file";
            errorDiagnostic.severity = lsp::DiagnosticSeverity::Error;
            errorDiagnostic.range = {{0, 0}, {0, 0}};
            return {errorDiagnostic};
        }

        std::vector<lsp::Diagnostic> diagnostics;
        for (auto& error : cr.errors)
        {
            lsp::Diagnostic diag;
            diag.source = "Luau";
            diag.code = error.code();
            diag.message = "TypeError: " + Luau::toString(error);
            diag.severity = lsp::DiagnosticSeverity::Error;
            diag.range = {{error.location.begin.line, error.location.begin.column}, {error.location.end.line, error.location.end.column}};
            diagnostics.emplace_back(diag);
        }

        Luau::LintResult lr = frontend.lint(fileName);
        for (auto& error : lr.errors)
        {
            lsp::Diagnostic diag;
            diag.source = "Luau";
            diag.code = error.code;
            diag.message = std::string(Luau::LintWarning::getName(error.code)) + ": " + error.text;
            diag.severity = lsp::DiagnosticSeverity::Error;
            lsp::Position start{error.location.begin.line, error.location.begin.column};
            lsp::Position end{error.location.end.line, error.location.end.column};
            diag.range = {start, end};
            diagnostics.emplace_back(diag);
        }
        for (auto& error : lr.warnings)
        {
            lsp::Diagnostic diag;
            diag.source = "Luau";
            diag.code = error.code;
            diag.message = std::string(Luau::LintWarning::getName(error.code)) + ": " + error.text;
            diag.severity = lsp::DiagnosticSeverity::Warning;
            lsp::Position start{error.location.begin.line, error.location.begin.column};
            lsp::Position end{error.location.end.line, error.location.end.column};
            diag.range = {start, end};
            diagnostics.emplace_back(diag);
        }

        return diagnostics;
    }
};