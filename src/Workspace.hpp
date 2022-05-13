#pragma once
#include "Luau/Frontend.h"
#include "Luau/Autocomplete.h"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ToString.h"
#include "Protocol.hpp"
#include "Sourcemap.hpp"

static std::optional<Luau::AutocompleteEntryMap> nullCallback(std::string tag, std::optional<const Luau::ClassTypeVar*> ptr)
{
    return std::nullopt;
}

static std::optional<std::string> getParentPath(const std::string& path)
{
    if (path == "" || path == "." || path == "/")
        return std::nullopt;

    std::string::size_type slash = path.find_last_of("\\/", path.size() - 1);

    if (slash == 0)
        return "/";

    if (slash != std::string::npos)
        return path.substr(0, slash);

    return "";
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

    // The root source node from a parsed Rojo source map
    SourceNodePtr rootSourceNode;
    mutable std::unordered_map<std::string, SourceNodePtr> realPathsToSourceNodes;
    mutable std::unordered_map<Luau::ModuleName, SourceNodePtr> virtualPathsToSourceNodes;

    // Currently opened files where content is managed by client
    mutable std::unordered_map<Luau::ModuleName, std::string> managedFiles;
    mutable std::unordered_map<std::string, Luau::Config> configCache;
    mutable std::vector<std::pair<std::filesystem::path, std::string>> configErrors;

    WorkspaceFileResolver()
    {
        defaultConfig.mode = Luau::Mode::Nonstrict;
    }

    /// The file is managed by the client, so FS will be out of date
    bool isManagedFile(const Luau::ModuleName& name) const
    {
        return managedFiles.find(name) != managedFiles.end();
    }

    /// The name points to a virtual path (i.e., game/ or ProjectRoot/)
    bool isVirtualPath(const Luau::ModuleName& name) const
    {
        return name == "game" || name == "ProjectRoot" || Luau::startsWith(name, "game/") || Luau::startsWith(name, "ProjectRoot");
    }

    std::optional<SourceNodePtr> getSourceNodeFromVirtualPath(const Luau::ModuleName& name) const
    {
        if (virtualPathsToSourceNodes.find(name) == virtualPathsToSourceNodes.end())
            return std::nullopt;
        return virtualPathsToSourceNodes.at(name);
    }

    std::optional<SourceNodePtr> getSourceNodeFromRealPath(const std::string& name) const
    {
        std::error_code ec;
        auto canonicalName = std::filesystem::canonical(name, ec);
        if (ec.value() != 0)
            canonicalName = name;
        auto strName = canonicalName.generic_string();
        if (realPathsToSourceNodes.find(strName) == realPathsToSourceNodes.end())
            return std::nullopt;
        return realPathsToSourceNodes.at(strName);
    }

    Luau::ModuleName getVirtualPathFromSourceNode(const SourceNodePtr& sourceNode) const
    {
        return sourceNode->virtualPath;
    }

    std::optional<Luau::ModuleName> resolveToVirtualPath(const std::string& name) const
    {
        if (isVirtualPath(name))
        {
            return name;
        }
        else
        {
            auto sourceNode = getSourceNodeFromRealPath(name);
            if (!sourceNode)
                return std::nullopt;
            return getVirtualPathFromSourceNode(sourceNode.value());
        }
    }

    std::optional<std::filesystem::path> resolveVirtualPathToRealPath(const Luau::ModuleName& name) const
    {
        if (auto sourceNode = getSourceNodeFromVirtualPath(name))
        {
            return sourceNode.value()->getScriptFilePath();
        }
        return std::nullopt;
    }

    std::optional<Luau::SourceCode> readSource(const Luau::ModuleName& name) override
    {
        Luau::SourceCode::Type sourceType = Luau::SourceCode::Module;
        std::optional<std::string> source;

        std::filesystem::path realFileName = name;
        if (isVirtualPath(name))
        {
            auto sourceNode = getSourceNodeFromVirtualPath(name);
            if (!sourceNode)
                return std::nullopt;
            auto filePath = sourceNode.value()->getScriptFilePath();
            if (!filePath)
                return std::nullopt;
            realFileName = filePath.value();
            sourceType = sourceNode.value()->sourceCodeType();
        }
        else
        {
            sourceType = sourceCodeTypeFromPath(realFileName);
        }

        if (isManagedFile(name))
        {
            source = managedFiles.at(name);
        }
        else
        {
            source = readFile(realFileName);
            // TODO: handle if json
        }

        if (!source)
            return std::nullopt;

        return Luau::SourceCode{*source, sourceType};
    }

    std::optional<Luau::ModuleInfo> resolveModule(const Luau::ModuleInfo* context, Luau::AstExpr* node) override
    {
        // Handle require("path") for compatibility
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
        else if (Luau::AstExprGlobal* g = node->as<Luau::AstExprGlobal>())
        {
            if (g->name == "game")
                return Luau::ModuleInfo{"game"};

            if (g->name == "script")
            {
                if (auto virtualPath = resolveToVirtualPath(context->name))
                {
                    return Luau::ModuleInfo{virtualPath.value()};
                }
            }
        }
        else if (Luau::AstExprIndexName* i = node->as<Luau::AstExprIndexName>())
        {
            if (context)
            {
                if (strcmp(i->index.value, "Parent") == 0)
                {
                    // Pop the name instead
                    auto parentPath = getParentPath(context->name);
                    if (parentPath.has_value())
                        return Luau::ModuleInfo{parentPath.value(), context->optional};
                }

                return Luau::ModuleInfo{context->name + '/' + i->index.value, context->optional};
            }
        }
        else if (Luau::AstExprIndexExpr* i_expr = node->as<Luau::AstExprIndexExpr>())
        {
            if (Luau::AstExprConstantString* index = i_expr->index->as<Luau::AstExprConstantString>())
            {
                if (context)
                    return Luau::ModuleInfo{context->name + '/' + std::string(index->value.data, index->value.size), context->optional};
            }
        }
        else if (Luau::AstExprCall* call = node->as<Luau::AstExprCall>(); call && call->self && call->args.size >= 1 && context)
        {
            if (Luau::AstExprConstantString* index = call->args.data[0]->as<Luau::AstExprConstantString>())
            {
                Luau::AstName func = call->func->as<Luau::AstExprIndexName>()->index;

                if (func == "GetService" && context->name == "game")
                    return Luau::ModuleInfo{"game/" + std::string(index->value.data, index->value.size)};
                if (func == "WaitForChild" || func == "FindFirstChild")
                    if (context)
                        return Luau::ModuleInfo{context->name + '/' + std::string(index->value.data, index->value.size), context->optional};
            }
        }

        return std::nullopt;
    }

    std::string getHumanReadableModuleName(const Luau::ModuleName& name) const override
    {
        if (isVirtualPath(name))
        {
            if (auto realPath = resolveVirtualPathToRealPath(name))
            {
                return realPath->relative_path().generic_string() + " [" + name + "]";
            }
            else
            {
                return name;
            }
        }
        else
        {
            return name;
        }
    }

    const Luau::Config& getConfig(const Luau::ModuleName& name) const override
    {
        std::optional<std::filesystem::path> realPath = name;
        if (isVirtualPath(name))
        {
            realPath = resolveVirtualPathToRealPath(name);
        }

        if (!realPath || !realPath->has_relative_path() || !realPath->has_parent_path())
            return defaultConfig;

        return readConfigRec(realPath->parent_path());
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

    void writePathsToMap(const SourceNodePtr& node, const std::string& base)
    {
        node->virtualPath = base;
        virtualPathsToSourceNodes[base] = node;

        if (auto realPath = node->getScriptFilePath())
        {
            std::error_code ec;
            auto canonicalName = std::filesystem::canonical(*realPath, ec);
            if (ec.value() != 0)
                canonicalName = *realPath;
            realPathsToSourceNodes[canonicalName.generic_string()] = node;
        }

        for (auto& child : node->children)
        {
            writePathsToMap(child, base + "/" + child->name);
        }
    }

    void updateSourceMap(const std::string& sourceMapContents)
    {
        realPathsToSourceNodes.clear();
        virtualPathsToSourceNodes.clear();

        try
        {
            auto j = json::parse(sourceMapContents);
            rootSourceNode = std::make_shared<SourceNode>(j.get<SourceNode>());

            // Write paths
            std::string base = rootSourceNode->className == "DataModel" ? "game" : "ProjectRoot";
            writePathsToMap(rootSourceNode, base);
        }
        catch (const std::exception& e)
        {
            // TODO: log message?
            e;
        }
    }
};

namespace types
{
std::optional<Luau::TypeId> getTypeIdForClass(const Luau::ScopePtr& globalScope, std::optional<std::string> className)
{
    std::optional<Luau::TypeFun> baseType;
    if (className.has_value())
    {
        baseType = globalScope->lookupType(className.value());
    }
    if (!baseType.has_value())
    {
        baseType = globalScope->lookupType("Instance");
    }

    if (baseType.has_value())
    {
        return baseType->type;
    }
    else
    {
        // If we reach this stage, we couldn't find the class name nor the "Instance" type
        // This most likely means a valid definitions file was not provided
        return std::nullopt;
    }
}

Luau::TypeId makeLazyInstanceType(Luau::TypeArena& arena, const Luau::ScopePtr& globalScope, const SourceNodePtr& node,
    std::optional<Luau::TypeId> parent, const WorkspaceFileResolver& fileResolver)
{
    Luau::LazyTypeVar ltv;
    ltv.thunk = [&arena, globalScope, node, parent, fileResolver]()
    {
        // TODO: we should cache created instance types and reuse them where possible

        // Look up the base class instance
        auto baseTypeId = getTypeIdForClass(globalScope, node->className);
        if (!baseTypeId)
        {
            return Luau::getSingletonTypes().anyType;
        }

        // Create the ClassTypeVar representing the instance
        Luau::ClassTypeVar ctv{node->name, {}, baseTypeId, std::nullopt, {}, {}, "@roblox"};
        auto typeId = arena.addType(std::move(ctv));

        // Attach Parent and Children info
        // Get the mutable version of the type var
        if (Luau::ClassTypeVar* ctv = Luau::getMutable<Luau::ClassTypeVar>(typeId))
        {

            // Add the parent
            if (parent.has_value())
            {
                ctv->props["Parent"] = Luau::makeProperty(parent.value());
            }
            else
            {
                // Search for the parent type
                if (auto parentPath = getParentPath(node->virtualPath))
                {
                    if (auto parentNode = fileResolver.getSourceNodeFromVirtualPath(parentPath.value()))
                    {
                        ctv->props["Parent"] =
                            Luau::makeProperty(makeLazyInstanceType(arena, globalScope, parentNode.value(), std::nullopt, fileResolver));
                    }
                }
            }

            // Add the children
            for (const auto& child : node->children)
            {
                ctv->props[child->name] = Luau::makeProperty(makeLazyInstanceType(arena, globalScope, child, typeId, fileResolver));
            }
        }
        return typeId;
    };

    return arena.addType(std::move(ltv));
}

// Magic function for `Instance:IsA("ClassName")` predicate
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionInstanceIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult)
{
    if (expr.args.size != 1)
        return std::nullopt;

    auto index = expr.func->as<Luau::AstExprIndexName>();
    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!index || !str)
        return std::nullopt;

    std::optional<Luau::LValue> lvalue = tryGetLValue(*index->expr);
    std::optional<Luau::TypeFun> tfun = scope->lookupType(std::string(str->value.data, str->value.size));
    if (!lvalue || !tfun)
        return std::nullopt;

    Luau::TypePackId booleanPack = typeChecker.globalTypes.addTypePack({typeChecker.booleanType});
    return Luau::ExprResult<Luau::TypePackId>{booleanPack, {Luau::IsAPredicate{std::move(*lvalue), expr.location, tfun->type}}};
}

// Magic function for `instance:Clone()`, so that we return the exact subclass that `instance` is, rather than just a generic Instance
static std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionInstanceClone(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult)
{
    auto index = expr.func->as<Luau::AstExprIndexName>();
    if (!index)
        return std::nullopt;

    Luau::TypeArena& arena = typeChecker.currentModule->internalTypes;
    Luau::TypeId instanceType = typeChecker.checkLValueBinding(scope, *index->expr);
    return Luau::ExprResult<Luau::TypePackId>{arena.addTypePack({instanceType})};
}

// Magic function for `Instance:FindFirstChildWhichIsA("ClassName")` and friends
std::optional<Luau::ExprResult<Luau::TypePackId>> magicFunctionFindFirstXWhichIsA(
    Luau::TypeChecker& typeChecker, const Luau::ScopePtr& scope, const Luau::AstExprCall& expr, Luau::ExprResult<Luau::TypePackId> exprResult)
{
    if (expr.args.size < 1)
        return std::nullopt;

    auto str = expr.args.data[0]->as<Luau::AstExprConstantString>();
    if (!str)
        return std::nullopt;

    std::optional<Luau::TypeFun> tfun = scope->lookupType(std::string(str->value.data, str->value.size));
    if (!tfun)
        return std::nullopt;

    Luau::TypeId nillableClass = Luau::makeOption(typeChecker, typeChecker.globalTypes, tfun->type);
    return Luau::ExprResult<Luau::TypePackId>{typeChecker.globalTypes.addTypePack({nillableClass})};
}

} // namespace types

class WorkspaceFolder
{
public:
    std::string name;
    lsp::DocumentUri rootUri;
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
                else if (auto ttv = Luau::get<Luau::TableTypeVar>(id))
                {
                    // Special case the RBXScriptSignal type as a connection
                    if (ttv->name && ttv->name.value() == "RBXScriptSignal")
                    {
                        item.kind = lsp::CompletionItemKind::Event;
                    }
                }
                else if (auto ctv = Luau::get<Luau::ClassTypeVar>(id))
                {
                    item.kind = lsp::CompletionItemKind::Class;
                }
                item.detail = Luau::toString(id);
            }

            items.emplace_back(item);
        }

        return items;
    }

    void updateSourceMap()
    {
        // Read in the sourcemap
        // TODO: We should invoke the rojo process dynamically if possible here, so that we can also refresh the sourcemap when we notice files are
        // changed
        // TODO: we assume a sourcemap.json file in the workspace root
        if (auto sourceMapContents = readFile(rootUri.fsPath() / "sourcemap.json"))
        {
            fileResolver.updateSourceMap(sourceMapContents.value());
        }
    }

private:
    void setup()
    {
        updateSourceMap();

        // Register general builtin types
        Luau::registerBuiltinTypes(frontend.typeChecker);

        // Register extended types
        // TODO: we assume a globalTypes.d.lua in workspace root
        if (auto definitions = readFile(rootUri.fsPath() / "globalTypes.d.lua"))
        {
            auto loadResult = Luau::loadDefinitionFile(frontend.typeChecker, frontend.typeChecker.globalScope, *definitions, "@roblox");
            if (!loadResult.success)
            {
                // TODO: publish diagnostics for file
            }

            // Extend globally registered types with Instance information
            if (fileResolver.rootSourceNode->className == "DataModel")
            {
                for (const auto& service : fileResolver.rootSourceNode->children)
                {
                    auto serviceName = service->className; // We know it must be a service of the same class name
                    if (auto serviceType = frontend.typeChecker.globalScope->lookupType(serviceName))
                    {
                        if (Luau::ClassTypeVar* ctv = Luau::getMutable<Luau::ClassTypeVar>(serviceType->type))
                        {
                            // Extend the props to include the children
                            for (const auto& child : service->children)
                            {
                                ctv->props[child->name] = Luau::makeProperty(types::makeLazyInstanceType(
                                    frontend.typeChecker.globalTypes, frontend.typeChecker.globalScope, child, serviceType->type, fileResolver));
                            }
                        }
                    }
                }
            }

            // Prepare module scope so that we can dynamically reassign the type of "script" to retrieve instance info
            frontend.typeChecker.prepareModuleScope = [this](const Luau::ModuleName& name, const Luau::ScopePtr& scope)
            {
                if (auto node = fileResolver.isVirtualPath(name) ? fileResolver.getSourceNodeFromVirtualPath(name)
                                                                 : fileResolver.getSourceNodeFromRealPath(name))
                {
                    // HACK: we need a way to get the typeArena for the module, but I don't know how
                    // we can see that moduleScope->returnType is assigned before prepareModuleScope is called in TypeInfer, so we could try it this
                    // way...
                    LUAU_ASSERT(scope->returnType);
                    auto typeArena = scope->returnType->owningArena;
                    LUAU_ASSERT(typeArena);

                    scope->bindings[Luau::AstName("script")] =
                        Luau::Binding{types::makeLazyInstanceType(*typeArena, scope, node.value(), std::nullopt, fileResolver), Luau::Location{}, {},
                            {}, std::nullopt};
                }
            };
        }

        if (auto instanceType = frontend.typeChecker.globalScope->lookupType("Instance"))
        {
            if (auto* ctv = Luau::getMutable<Luau::ClassTypeVar>(instanceType->type))
            {
                Luau::attachMagicFunction(ctv->props["IsA"].type, types::magicFunctionInstanceIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstChildWhichIsA"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstChildOfClass"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstAncestorWhichIsA"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["FindFirstAncestorOfClass"].type, types::magicFunctionFindFirstXWhichIsA);
                Luau::attachMagicFunction(ctv->props["Clone"].type, types::magicFunctionInstanceClone);
            }
        }

        // Freeze arena
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