#include <utility>

#include "Platform/LSPPlatform.hpp"
#include "Luau/BuiltinDefinitions.h"
#include "Luau/ToString.h"
#include "Luau/Transpiler.h"
#include "Luau/TypeInfer.h"
#include "LSP/LuauExt.hpp"
#include "LSP/Utils.hpp"

namespace types
{
std::optional<std::string> getTypeName(Luau::TypeId typeId)
{
    std::optional<std::string> name = std::nullopt;
    auto ty = Luau::follow(typeId);

    if (auto typeName = Luau::getName(ty))
    {
        name = *typeName;
    }
    else if (auto mtv = Luau::get<Luau::MetatableType>(ty))
    {
        if (auto mtvName = Luau::getName(mtv->metatable))
            name = *mtvName;
    }
    else if (auto parentClass = Luau::get<Luau::ClassType>(ty))
    {
        name = parentClass->name;
    }
    // if (auto parentUnion = Luau::get<UnionType>(ty))
    // {
    //     return returnFirstNonnullOptionOfType<ClassType>(parentUnion);
    // }

    // strip synthetic typeof() for builtin tables
    if (name && name->compare(0, 7, "typeof(") == 0 && name->back() == ')')
        return name->substr(7, name->length() - 8);
    else
        return name;
}

std::optional<nlohmann::json> parseDefinitionsFileMetadata(const std::string& definitions)
{
    auto firstLine = getFirstLine(definitions);
    if (Luau::startsWith(firstLine, "--#METADATA#"))
    {
        firstLine = firstLine.substr(12);
        return json::parse(firstLine);
    }
    return std::nullopt;
}

Luau::LoadDefinitionFileResult registerDefinitions(
    Luau::Frontend& frontend, Luau::GlobalTypes& globals, const std::string& definitions, bool typeCheckForAutocomplete)
{
    // TODO: packageName shouldn't just be "@roblox"
    return frontend.loadDefinitionFile(globals, globals.globalScope, definitions, "@roblox", /* captureComments = */ false, typeCheckForAutocomplete);
}

using NameOrExpr = std::variant<std::string, Luau::AstExpr*>;

// Converts an FTV and function call to a nice string
// In the format "function NAME(args): ret"
std::string toStringNamedFunction(const Luau::ModulePtr& module, const Luau::FunctionType* ftv, const NameOrExpr nameOrFuncExpr,
    std::optional<Luau::ScopePtr> scope, const ToStringNamedFunctionOpts& stringOpts)
{
    Luau::ToStringOptions opts;
    opts.functionTypeArguments = true;
    opts.hideNamedFunctionTypeParameters = false;
    opts.hideTableKind = stringOpts.hideTableKind;
    opts.useLineBreaks = stringOpts.multiline;
    if (scope)
        opts.scope = *scope;
    auto functionString = Luau::toStringNamedFunction("", *ftv, opts);

    // HACK: remove all instances of "_: " from the function string
    // They don't look great, maybe we should upstream this as an option?
    replaceAll(functionString, "_: ", "");

    // If a name has already been provided, just use that
    if (auto name = std::get_if<std::string>(&nameOrFuncExpr))
    {
        return "function " + *name + functionString;
    }
    auto funcExprPtr = std::get_if<Luau::AstExpr*>(&nameOrFuncExpr);

    // TODO: error here?
    if (!funcExprPtr)
        return "function" + functionString;
    auto funcExpr = *funcExprPtr;

    // See if it's just in the form `func(args)`
    if (auto local = funcExpr->as<Luau::AstExprLocal>())
    {
        return "function " + std::string(local->local->name.value) + functionString;
    }
    else if (auto global = funcExpr->as<Luau::AstExprGlobal>())
    {
        return "function " + std::string(global->name.value) + functionString;
    }
    else if (funcExpr->as<Luau::AstExprGroup>() || funcExpr->as<Luau::AstExprFunction>())
    {
        // In the form (expr)(args), which implies that it's probably a IIFE
        return "function" + functionString;
    }

    // See if the name belongs to a ClassType
    Luau::TypeId* parentIt = nullptr;
    std::string methodName;
    std::string baseName;

    if (auto indexName = funcExpr->as<Luau::AstExprIndexName>())
    {
        parentIt = module->astTypes.find(indexName->expr);
        methodName = std::string(1, indexName->op) + indexName->index.value;
        // If we are calling this as a method ':', we should implicitly hide self, and recompute the functionString
        opts.hideFunctionSelfArgument = indexName->op == ':';
        functionString = Luau::toStringNamedFunction("", *ftv, opts);
        replaceAll(functionString, "_: ", "");
        // We can try and give a temporary base name from what we can infer by the index, and then attempt to improve it with proper information
        baseName = Luau::toString(indexName->expr);
        trim(baseName); // Trim it, because toString is probably not meant to be used in this context (it has whitespace)
    }
    else if (auto indexExpr = funcExpr->as<Luau::AstExprIndexExpr>())
    {
        parentIt = module->astTypes.find(indexExpr->expr);
        methodName = "[" + Luau::toString(indexExpr->index) + "]";
        // We can try and give a temporary base name from what we can infer by the index, and then attempt to improve it with proper information
        baseName = Luau::toString(indexExpr->expr);
        // Trim it, because toString is probably not meant to be used in this context (it has whitespace)
        trim(baseName);
    }

    if (!parentIt)
        return "function" + methodName + functionString;

    if (auto name = getTypeName(*parentIt))
        baseName = *name;

    return "function " + baseName + methodName + functionString;
}

std::string toStringReturnType(Luau::TypePackId retTypes, Luau::ToStringOptions options)
{
    return toStringReturnTypeDetailed(retTypes, std::move(options)).name;
}

Luau::ToStringResult toStringReturnTypeDetailed(Luau::TypePackId retTypes, Luau::ToStringOptions options)
{
    size_t retSize = Luau::size(retTypes);
    bool hasTail = !Luau::finite(retTypes);
    bool wrap = Luau::get<Luau::TypePack>(Luau::follow(retTypes)) && (hasTail ? retSize != 0 : retSize != 1);

    auto result = Luau::toStringDetailed(retTypes, options);
    if (wrap)
        result.name = "(" + result.name + ")";
    return result;
}

// Duplicated from Luau/TypeInfer.h, since its static
std::optional<Luau::AstExpr*> matchRequire(const Luau::AstExprCall& call)
{
    const char* require = "require";

    if (call.args.size != 1)
        return std::nullopt;

    const Luau::AstExprGlobal* funcAsGlobal = call.func->as<Luau::AstExprGlobal>();
    if (!funcAsGlobal || funcAsGlobal->name != require)
        return std::nullopt;

    if (call.args.size != 1)
        return std::nullopt;

    return call.args.data[0];
}
} // namespace types

struct FindNodeType : public Luau::AstVisitor
{
    Luau::Position pos;
    Luau::Position documentEnd;
    Luau::AstNode* best = nullptr;
    bool closed = false;

    explicit FindNodeType(Luau::Position pos, Luau::Position documentEnd, bool closed)
        : pos(pos)
        , documentEnd(documentEnd)
        , closed(closed)
    {
    }

    bool isCloserMatch(Luau::Location& newLocation) const
    {
        return (closed ? newLocation.containsClosed(pos) : newLocation.contains(pos)) && (!best || best->location.encloses(newLocation));
    }

    bool visit(Luau::AstNode* node) override
    {
        if (isCloserMatch(node->location))
        {
            best = node;
            return true;
        }

        // Edge case: If we ask for the node at the position that is the very end of the document
        // return the innermost AST element that ends at that position.

        if (node->location.end == documentEnd && pos >= documentEnd)
        {
            best = node;
            return true;
        }

        return false;
    }

    bool visit(class Luau::AstType* node) override
    {
        return visit(static_cast<Luau::AstNode*>(node));
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        visit(static_cast<Luau::AstNode*>(block));

        for (Luau::AstStat* stat : block->body)
        {
            if (stat->location.end < pos)
                continue;
            if (stat->location.begin > pos)
                break;

            stat->visit(this);
        }

        return false;
    }
};

Luau::AstNode* findNodeOrTypeAtPosition(const Luau::SourceModule& source, Luau::Position pos)
{
    const Luau::Position end = source.root->location.end;
    if (pos < source.root->location.begin)
        return source.root;

    if (pos > end)
        pos = end;

    FindNodeType findNode{pos, end, /* closed: */ false};
    findNode.visit(source.root);
    return findNode.best;
}

Luau::AstNode* findNodeOrTypeAtPositionClosed(const Luau::SourceModule& source, Luau::Position pos)
{
    const Luau::Position end = source.root->location.end;
    if (pos < source.root->location.begin)
        return source.root;

    if (pos > end)
        pos = end;

    FindNodeType findNode{pos, end, /* closed: */ true};
    findNode.visit(source.root);
    return findNode.best;
}

std::optional<Luau::Location> lookupTypeLocation(const Luau::Scope& deepScope, const Luau::Name& name)
{
    const Luau::Scope* scope = &deepScope;
    while (true)
    {
        auto it = scope->typeAliasLocations.find(name);
        if (it != scope->typeAliasLocations.end())
            return it->second;

        if (scope->parent)
            scope = scope->parent.get();
        else
            return std::nullopt;
    }
}

// Returns [base, property] - base is important during intersections
std::optional<std::pair<Luau::TypeId, Luau::Property>> lookupProp(const Luau::TypeId& parentType, const Luau::Name& name)
{
    if (auto ctv = Luau::get<Luau::ClassType>(parentType))
    {
        if (auto prop = Luau::lookupClassProp(ctv, name))
            return std::make_pair(parentType, *prop);
    }
    else if (auto tbl = Luau::get<Luau::TableType>(parentType))
    {
        if (tbl->props.find(name) != tbl->props.end())
        {
            return std::make_pair(parentType, tbl->props.at(name));
        }
    }
    else if (auto mt = Luau::get<Luau::MetatableType>(parentType))
    {
        if (auto mtable = Luau::get<Luau::TableType>(Luau::follow(mt->metatable)))
        {
            auto indexIt = mtable->props.find("__index");
            if (indexIt != mtable->props.end())
            {
                Luau::TypeId followed = Luau::follow(indexIt->second.type());
                if ((Luau::get<Luau::TableType>(followed) || Luau::get<Luau::MetatableType>(followed)) && followed != parentType) // ensure acyclic
                {
                    return lookupProp(followed, name);
                }
                else if (Luau::get<Luau::FunctionType>(followed))
                {
                    // TODO: can we handle an index function...?
                    return std::nullopt;
                }
            }
        }

        auto baseTableTy = Luau::follow(mt->table);
        if (auto mtBaseTable = Luau::get<Luau::TableType>(baseTableTy))
        {
            if (mtBaseTable->props.find(name) != mtBaseTable->props.end())
            {
                return std::make_pair(baseTableTy, mtBaseTable->props.at(name));
            }
        }
    }
    else if (auto i = Luau::get<Luau::IntersectionType>(parentType))
    {
        for (Luau::TypeId ty : i->parts)
        {
            if (auto prop = lookupProp(Luau::follow(ty), name))
                return prop;
        }
    }
    // else if (auto u = get<Luau::UnionType>(parentType))
    // {
    //     // Find the corresponding ty
    // }
    return std::nullopt;
}

std::optional<Luau::ModuleName> lookupImportedModule(const Luau::Scope& deepScope, const Luau::Name& name)
{
    const Luau::Scope* scope = &deepScope;
    while (true)
    {
        auto it = scope->importedModules.find(name);
        if (it != scope->importedModules.end())
            return it->second;

        if (scope->parent)
            scope = scope->parent.get();
        else
            return std::nullopt;
    }
}

bool types::isMetamethod(const Luau::Name& name)
{
    return name == "__index" || name == "__newindex" || name == "__call" || name == "__concat" || name == "__unm" || name == "__add" ||
           name == "__sub" || name == "__mul" || name == "__div" || name == "__mod" || name == "__pow" || name == "__tostring" ||
           name == "__metatable" || name == "__eq" || name == "__lt" || name == "__le" || name == "__mode" || name == "__iter" || name == "__len";
}

lsp::Position toUTF16(const TextDocument* textDocument, const Luau::Position& position)
{
    if (textDocument)
        return textDocument->convertPosition(position);
    else
        return lsp::Position{static_cast<size_t>(position.line), static_cast<size_t>(position.column)};
}

lsp::Diagnostic createTypeErrorDiagnostic(const Luau::TypeError& error, Luau::FileResolver* fileResolver, const TextDocument* textDocument)
{
    std::string message;
    if (const auto* syntaxError = Luau::get_if<Luau::SyntaxError>(&error.data))
        message = "SyntaxError: " + syntaxError->message;
    else
        message = "TypeError: " + Luau::toString(error, Luau::TypeErrorToStringOptions{fileResolver});

    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = error.code();
    diagnostic.message = message;
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {toUTF16(textDocument, error.location.begin), toUTF16(textDocument, error.location.end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/typecheck")};
    return diagnostic;
}

lsp::Diagnostic createLintDiagnostic(const Luau::LintWarning& lint, const TextDocument* textDocument)
{
    std::string lintName = Luau::LintWarning::getName(lint.code);

    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = lint.code;
    diagnostic.message = lintName + ": " + lint.text;
    diagnostic.severity = lsp::DiagnosticSeverity::Warning; // Configuration can convert this to an error
    diagnostic.range = {toUTF16(textDocument, lint.location.begin), toUTF16(textDocument, lint.location.end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/lint#" + toLower(lintName) + "-" + std::to_string(static_cast<int>(lint.code)))};

    if (lint.code == Luau::LintWarning::Code::Code_LocalUnused || lint.code == Luau::LintWarning::Code::Code_ImportUnused ||
        lint.code == Luau::LintWarning::Code::Code_FunctionUnused)
    {
        diagnostic.tags.emplace_back(lsp::DiagnosticTag::Unnecessary);
    }
    else if (lint.code == Luau::LintWarning::Code::Code_DeprecatedApi || lint.code == Luau::LintWarning::Code::Code_DeprecatedGlobal)
    {
        diagnostic.tags.emplace_back(lsp::DiagnosticTag::Deprecated);
    }

    return diagnostic;
}

lsp::Diagnostic createParseErrorDiagnostic(const Luau::ParseError& error, const TextDocument* textDocument)
{
    lsp::Diagnostic diagnostic;
    diagnostic.source = "Luau";
    diagnostic.code = "SyntaxError";
    diagnostic.message = "SyntaxError: " + error.getMessage();
    diagnostic.severity = lsp::DiagnosticSeverity::Error;
    diagnostic.range = {toUTF16(textDocument, error.getLocation().begin), toUTF16(textDocument, error.getLocation().end)};
    diagnostic.codeDescription = {Uri::parse("https://luau-lang.org/syntax")};
    return diagnostic;
}

// Based on upstream, except we use containsClosed
struct FindExprOrLocalClosed : public Luau::AstVisitor
{
    Luau::Position pos;
    Luau::ExprOrLocal result;

    explicit FindExprOrLocalClosed(Luau::Position pos)
        : pos(pos)
    {
    }

    // We want to find the result with the smallest location range.
    bool isCloserMatch(Luau::Location newLocation)
    {
        auto current = result.getLocation();
        return newLocation.containsClosed(pos) && (!current || current->encloses(newLocation));
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            if (stat->location.end < pos)
                continue;
            if (stat->location.begin > pos)
                break;

            stat->visit(this);
        }

        return false;
    }

    bool visit(Luau::AstExpr* expr) override
    {
        if (isCloserMatch(expr->location))
        {
            result.setExpr(expr);
            return true;
        }
        return false;
    }

    bool visitLocal(Luau::AstLocal* local)
    {
        if (isCloserMatch(local->location))
        {
            result.setLocal(local);
            return true;
        }
        return false;
    }

    bool visit(Luau::AstStatLocalFunction* function) override
    {
        visitLocal(function->name);
        return true;
    }

    bool visit(Luau::AstStatLocal* al) override
    {
        for (size_t i = 0; i < al->vars.size; ++i)
        {
            visitLocal(al->vars.data[i]);
        }
        return true;
    }

    bool visit(Luau::AstExprFunction* fn) override
    {
        for (size_t i = 0; i < fn->args.size; ++i)
        {
            visitLocal(fn->args.data[i]);
        }
        return visit((Luau::AstExpr*)fn);
    }

    bool visit(Luau::AstStatFor* forStat) override
    {
        visitLocal(forStat->var);
        return true;
    }

    bool visit(Luau::AstStatForIn* forIn) override
    {
        for (Luau::AstLocal* var : forIn->vars)
        {
            visitLocal(var);
        }
        return true;
    }
};

Luau::ExprOrLocal findExprOrLocalAtPositionClosed(const Luau::SourceModule& source, Luau::Position pos)
{
    FindExprOrLocalClosed findVisitor{pos};
    findVisitor.visit(source.root);
    return findVisitor.result;
}

struct FindSymbolReferences : public Luau::AstVisitor
{
    Luau::Symbol symbol;
    std::optional<std::vector<lsp::DocumentHighlightKind>> kinds;
    std::vector<Luau::Location> locations{};
    bool isModuleImport = false;
    bool shouldExitCurrentScope = false;
    size_t symbolDepth = 0;

    explicit FindSymbolReferences(Luau::Symbol symbol, bool withKinds)
        : symbol(symbol)
        , kinds(withKinds ? std::optional(std::vector<lsp::DocumentHighlightKind>()) : std::nullopt)
    {
    }

    void addResult(Luau::Location location, lsp::DocumentHighlightKind kind)
    {
        locations.push_back(location);
        if (kinds)
            kinds.value().push_back(kind);
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        if (symbol.local && block->location.encloses(symbol.local->location))
            symbolDepth += 1;
        auto depth = symbolDepth;
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
            if (shouldExitCurrentScope)
            {
                shouldExitCurrentScope = false;
                break;
            }
            if (depth != symbolDepth)
                // This scope is an ancestor of the one in which the local is defined, so we won't find any references here
                break;
        }

        return false;
    }

    bool visitLocal(Luau::AstLocal* local) const
    {
        return Luau::Symbol(local) == symbol;
    }

    bool visitGlobal(const Luau::AstName& name) const
    {
        return Luau::Symbol(name) == symbol;
    }

    bool visitWriteExpr(Luau::AstExpr* expr) const
    {
        if (auto local = expr->as<Luau::AstExprLocal>(); local && visitLocal(local->local))
            return true;
        else if (auto global = expr->as<Luau::AstExprGlobal>(); global && visitGlobal(global->name))
            return true;
        return false;
    }

    bool visit(Luau::AstStatLocalFunction* function) override
    {
        if (visitLocal(function->name))
        {
            addResult(function->name->location, lsp::DocumentHighlightKind::Write);
        }
        return true;
    }

    bool visit(Luau::AstStatFunction* function) override
    {
        auto name = function->name;
        if (visitWriteExpr(name))
            addResult(name->location, lsp::DocumentHighlightKind::Write);
        else
            name->visit(this);
        function->func->visit(this);
        return false;
    }

    bool visit(Luau::AstStatLocal* al) override
    {
        for (size_t i = 0; i < al->vars.size; ++i)
        {
            auto var = al->vars.data[i];
            auto isSymbol = visitLocal(var);
            if (symbol.local && i < al->values.size)
            {
                if (symbol.local->name == var->name && isRequire(al->values.data[i]))
                {
                    if (isSymbol)
                        isModuleImport = true;
                    else if (symbol.local->location.begin < var->location.begin)
                    {
                        // Stop processing this scope because the symbol got shadowed by a module import
                        shouldExitCurrentScope = true;
                        return false;
                    }
                }
            }
            if (isSymbol)
            {
                addResult(var->location, lsp::DocumentHighlightKind::Write);
            }
        }
        return true;
    }

    bool visit(Luau::AstExprLocal* local) override
    {
        if (visitLocal(local->local))
        {
            addResult(local->location, lsp::DocumentHighlightKind::Read);
        }
        return true;
    }

    bool visit(Luau::AstExprGlobal* global) override
    {
        if (visitGlobal(global->name))
        {
            addResult(global->location, lsp::DocumentHighlightKind::Read);
        }
        return true;
    }

    bool visit(Luau::AstStatAssign* assign) override
    {

        for (auto var : assign->vars)
        {
            if (visitWriteExpr(var))
                addResult(var->location, lsp::DocumentHighlightKind::Write);
            else
                var->visit(this);
        }

        for (auto expr : assign->values)
            expr->visit(this);

        return false;
    }

    bool visit(Luau::AstStatCompoundAssign* compoundAssign) override
    {
        if (visitWriteExpr(compoundAssign->var))
            addResult(compoundAssign->var->location, lsp::DocumentHighlightKind::Write);
        else
            compoundAssign->var->visit(this);
        compoundAssign->value->visit(this);
        return false;
    }

    bool visit(Luau::AstExprFunction* fn) override
    {
        for (size_t i = 0; i < fn->args.size; ++i)
        {
            if (visitLocal(fn->args.data[i]))
            {
                addResult(fn->args.data[i]->location, lsp::DocumentHighlightKind::Write);
                symbolDepth += 1;
            }
        }
        return true;
    }

    bool visit(Luau::AstStatFor* forStat) override
    {
        if (visitLocal(forStat->var))
        {
            addResult(forStat->var->location, lsp::DocumentHighlightKind::Write);
            symbolDepth += 1;
        }
        return true;
    }

    bool visit(Luau::AstStatForIn* forIn) override
    {
        for (auto var : forIn->vars)
        {
            if (visitLocal(var))
            {
                addResult(var->location, lsp::DocumentHighlightKind::Write);
                symbolDepth += 1;
            }
        }
        return true;
    }

    bool visit(Luau::AstType* type) override
    {
        return true;
    }

    bool visit(Luau::AstTypeReference* typeReference) override
    {
        if (typeReference->prefix && isModuleImport && typeReference->prefix.value() == symbol.local->name)
        {
            addResult(typeReference->prefixLocation.value(), lsp::DocumentHighlightKind::Read);
        }
        return true;
    }
};

std::vector<Luau::Location> findSymbolReferences(const Luau::SourceModule& source, Luau::Symbol symbol)
{
    FindSymbolReferences finder(symbol, false);
    source.root->visit(&finder);
    return std::move(finder.locations);
}

std::pair<std::vector<Luau::Location>, std::vector<lsp::DocumentHighlightKind>> findSymbolReferencesWithKinds(
    const Luau::SourceModule& source, Luau::Symbol symbol)
{
    FindSymbolReferences finder(symbol, true);
    source.root->visit(&finder);
    return {std::move(finder.locations), std::move(finder.kinds.value())};
}

struct FindClosestAncestorModuleImportSymbol : public Luau::AstVisitor
{
    const Luau::AstName name;
    const Luau::Position pos;
    std::optional<std::pair<Luau::AstLocal*, Luau::AstExpr*>> result = std::nullopt;

    explicit FindClosestAncestorModuleImportSymbol(const Luau::AstName name, const Luau::Position pos)
        : name(name)
        , pos(pos)
    {
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        if (!block->location.contains(pos))
            // We only allow module imports that are declared with a local statement, and those can only be within statement blocks
            return false;

        for (auto stat : block->body)
        {
            if (stat->location.begin > pos)
                break;
            stat->visit(this);
        }

        return false;
    }

    bool visit(Luau::AstStatLocal* al) override
    {
        for (size_t i = 0; i < al->vars.size && i < al->values.size; ++i)
        {
            auto var = al->vars.data[i];
            auto value = al->values.data[i];
            auto call = value->as<Luau::AstExprCall>();
            if (!call)
                continue;
            if (var->name == name && isRequire(value))
                result = {{var, call->args.data[0]}};
        }
        return false;
    }
};

// Returns the local for the binding and the expr which is passed as the one and only argument to require
std::optional<std::pair<Luau::AstLocal*, Luau::AstExpr*>> findClosestAncestorModuleImport(
    const Luau::SourceModule& source, const Luau::AstName name, const Luau::Position pos)
{
    FindClosestAncestorModuleImportSymbol finder(name, pos);
    source.root->visit(&finder);
    return finder.result;
}

struct FindPropertyReferences : public Luau::AstVisitor
{
    Luau::Name property;
    Luau::TypeId ty;
    Luau::DenseHashMap<const Luau::AstExpr*, Luau::TypeId> astTypes;
    std::optional<std::vector<lsp::DocumentHighlightKind>> kinds;
    std::vector<Luau::Location> locations{};

    explicit FindPropertyReferences(
        Luau::Name property, Luau::TypeId ty, Luau::DenseHashMap<const Luau::AstExpr*, Luau::TypeId> astTypes, bool withKinds)
        : property(property)
        , ty(ty)
        , astTypes(astTypes)
        , kinds(withKinds ? std::optional(std::vector<lsp::DocumentHighlightKind>()) : std::nullopt)
    {
    }

    void addResult(Luau::Location location, lsp::DocumentHighlightKind kind)
    {
        locations.push_back(location);
        if (kinds)
            kinds.value().push_back(kind);
    }

    bool visitIndexName(Luau::AstExprIndexName* indexName)
    {
        if (indexName->index.value != property)
            return false;
        auto possibleParentTy = astTypes.find(indexName->expr);
        if (!possibleParentTy || !isSameTable(ty, Luau::follow(*possibleParentTy)))
            return false;
        return true;
    }

    bool visit(Luau::AstStatFunction* function)
    {
        if (auto indexName = function->name->as<Luau::AstExprIndexName>())
        {
            if (visitIndexName(indexName))
                addResult(indexName->indexLocation, lsp::DocumentHighlightKind::Write);
            indexName->expr->visit(this);
        }
        else
            function->name->visit(this);
        function->func->visit(this);
        return false;
    }

    bool visit(Luau::AstStatAssign* assign)
    {
        for (auto var : assign->vars)
        {
            if (auto indexName = var->as<Luau::AstExprIndexName>())
            {
                if (visitIndexName(indexName))
                    addResult(indexName->indexLocation, lsp::DocumentHighlightKind::Write);
                indexName->expr->visit(this);
            }
            else
                var->visit(this);
        }

        for (auto value : assign->values)
            value->visit(this);

        return false;
    }

    bool visit(Luau::AstStatCompoundAssign* compoundAssign)
    {
        if (auto indexName = compoundAssign->var->as<Luau::AstExprIndexName>())
        {
            if (visitIndexName(indexName))
                addResult(indexName->indexLocation, lsp::DocumentHighlightKind::Write);
            indexName->expr->visit(this);
        }
        else
            compoundAssign->var->visit(this);
        compoundAssign->value->visit(this);
        return false;
    }

    bool visit(Luau::AstExprIndexName* indexName)
    {
        if (visitIndexName(indexName))
            addResult(indexName->indexLocation, lsp::DocumentHighlightKind::Read);
        return true;
    }

    bool visit(Luau::AstExprTable* table)
    {
        auto referencedTy = astTypes.find(table->asExpr());
        if (!referencedTy || !isSameTable(ty, Luau::follow(*referencedTy)))
            return true;
        for (const auto& item : table->items)
        {
            item.value->visit(this);
            if (!item.key)
                continue;
            item.key->visit(this);
            auto propName = item.key->as<Luau::AstExprConstantString>();
            if (!propName || propName->value.data != property)
                continue;
            addResult(item.key->location, lsp::DocumentHighlightKind::Write);
        }
        return false;
    }
};

std::vector<Luau::Location> findPropertyReferences(
    const Luau::SourceModule& source, const Luau::Name& property, Luau::TypeId ty, Luau::DenseHashMap<const Luau::AstExpr*, Luau::TypeId> astTypes)
{
    FindPropertyReferences finder(property, ty, astTypes, false);
    source.root->visit(&finder);
    return std::move(finder.locations);
}

std::pair<std::vector<Luau::Location>, std::vector<lsp::DocumentHighlightKind>> findPropertyReferencesWithKinds(
    const Luau::SourceModule& source, const Luau::Name& property, Luau::TypeId ty, Luau::DenseHashMap<const Luau::AstExpr*, Luau::TypeId> astTypes)
{
    FindPropertyReferences finder(property, ty, astTypes, true);
    source.root->visit(&finder);
    return {std::move(finder.locations), std::move(finder.kinds.value())};
}

struct FindTypeReferences : public Luau::AstVisitor
{
    Luau::Name typeName;
    std::optional<const Luau::Name> prefix;
    std::vector<Luau::Location> result{};

    explicit FindTypeReferences(Luau::Name typeName, std::optional<const Luau::Name> prefix)
        : typeName(std::move(typeName))
        , prefix(std::move(prefix))
    {
    }

    bool visit(class Luau::AstType* node) override
    {
        return true;
    }

    bool visit(class Luau::AstTypeReference* node) override
    {
        if (node->name.value == typeName && ((!prefix && !node->prefix) || (prefix && node->prefix && node->prefix->value == prefix.value())))
            result.push_back(node->nameLocation);

        return true;
    }
};

std::vector<Luau::Location> findTypeReferences(const Luau::SourceModule& source, const Luau::Name& typeName, std::optional<const Luau::Name> prefix)
{
    FindTypeReferences finder(typeName, std::move(prefix));
    source.root->visit(&finder);
    return std::move(finder.result);
}

struct FindTypeParameterUsages : public Luau::AstVisitor
{
    Luau::AstName name;
    bool initialNode = true;
    std::vector<Luau::Location> locations{};
    std::optional<std::vector<lsp::DocumentHighlightKind>> kinds;

    FindTypeParameterUsages(Luau::AstName name, bool withKinds)
        : name(name)
        , kinds(withKinds ? std::optional(std::vector<lsp::DocumentHighlightKind>()) : std::nullopt)
    {
    }

    void addResult(Luau::Location location, lsp::DocumentHighlightKind kind)
    {
        locations.push_back(location);
        if (kinds)
            kinds.value().push_back(kind);
    }

    bool visit(class Luau::AstType* node) override
    {
        return true;
    }

    bool visit(class Luau::AstTypeReference* node) override
    {
        if (node->name == name)
            addResult(node->nameLocation, lsp::DocumentHighlightKind::Read);
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstTypeFunction* node) override
    {
        // Check to see the type parameter has not been redefined
        for (auto t : node->generics)
        {
            if (t.name == name)
            {
                if (initialNode)
                    addResult(t.location, lsp::DocumentHighlightKind::Write);
                else
                    return false;
            }
        }
        for (auto t : node->genericPacks)
        {
            if (t.name == name)
            {
                if (initialNode)
                    addResult(t.location, lsp::DocumentHighlightKind::Write);
                else
                    return false;
            }
        }
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstTypePack* node) override
    {
        return true;
    }

    bool visit(class Luau::AstTypePackGeneric* node) override
    {
        if (node->genericName == name)
            // node location also consists of the three dots "...", so we need to remove them
            addResult(
                Luau::Location{node->location.begin, {node->location.end.line, node->location.end.column - 3}}, lsp::DocumentHighlightKind::Read);
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstStatTypeAlias* node) override
    {
        for (auto t : node->generics)
            if (t.name == name)
                addResult(t.location, lsp::DocumentHighlightKind::Write);
        for (auto t : node->genericPacks)
            if (t.name == name)
                addResult(t.location, lsp::DocumentHighlightKind::Write);
        initialNode = false;
        return true;
    }

    bool visit(class Luau::AstExprFunction* node) override
    {
        for (auto t : node->generics)
            if (t.name == name)
                addResult(t.location, lsp::DocumentHighlightKind::Write);
        for (auto t : node->genericPacks)
            if (t.name == name)
                addResult(t.location, lsp::DocumentHighlightKind::Write);
        initialNode = false;
        return true;
    }
};

std::vector<Luau::Location> findTypeParameterUsages(Luau::AstNode& node, Luau::AstName name)
{
    FindTypeParameterUsages finder(name, false);
    node.visit(&finder);
    return std::move(finder.locations);
}

std::pair<std::vector<Luau::Location>, std::vector<lsp::DocumentHighlightKind>> findTypeParameterUsagesWithKinds(
    Luau::AstNode& node, Luau::AstName name)
{
    FindTypeParameterUsages finder(name, true);
    node.visit(&finder);
    return {std::move(finder.locations), std::move(finder.kinds.value())};
}

// Returns the AST name for the type referenced at the given position, if any, or else std::nullopt
std::optional<Luau::AstName> findTypeReferenceName(
    Luau::Position position, Luau::AstArray<Luau::AstGenericType> generics, Luau::AstArray<Luau::AstGenericTypePack> genericPacks)
{
    for (const auto t : generics)
        if (t.location.containsClosed(position))
            return t.name;
    for (const auto t : genericPacks)
        if (t.location.containsClosed(position))
            return t.name;
    return std::nullopt;
}

std::optional<Luau::Location> getLocation(Luau::TypeId type)
{
    type = follow(type);

    if (auto ftv = Luau::get<Luau::FunctionType>(type))
    {
        if (ftv->definition)
            return ftv->definition->originalNameLocation;
    }

    return std::nullopt;
}

bool isGetService(const Luau::AstExpr* expr)
{
    if (auto call = expr->as<Luau::AstExprCall>())
        if (auto index = call->func->as<Luau::AstExprIndexName>())
            if (index->index == "GetService")
                if (auto name = index->expr->as<Luau::AstExprGlobal>())
                    if (name->name == "game")
                        return true;

    return false;
}

bool isRequire(const Luau::AstExpr* expr)
{
    if (auto call = expr->as<Luau::AstExprCall>(); call && call->args.size == 1)
    {
        if (auto funcAsGlobal = call->func->as<Luau::AstExprGlobal>(); funcAsGlobal && funcAsGlobal->name == "require")
            return true;
    }
    else if (auto assertion = expr->as<Luau::AstExprTypeAssertion>())
    {
        return isRequire(assertion->expr);
    }

    return false;
}

bool isMethod(const Luau::FunctionType* ftv)
{
    if (ftv->hasSelf)
        return true;

    // TODO: hasSelf is not always specified, so we manually check for the "self" name (https://github.com/Roblox/luau/issues/551)
    if (ftv->argNames.size() > 0 && ftv->argNames[0].has_value() && ftv->argNames[0]->name == "self")
    {
        return true;
    }

    return false;
}

static bool isMethod(Luau::TypeId ty)
{
    auto ftv = Luau::get<Luau::FunctionType>(Luau::follow(ty));
    if (!ftv)
        return false;

    return isMethod(ftv);
}

bool isOverloadedMethod(Luau::TypeId ty)
{
    if (!Luau::get<Luau::IntersectionType>(Luau::follow(ty)))
        return false;

    auto isOverloadedMethod = [](Luau::TypeId part) -> bool
    {
        return isMethod(part);
    };

    std::vector<Luau::TypeId> parts = Luau::flattenIntersection(ty);
    return std::all_of(parts.begin(), parts.end(), isOverloadedMethod);
}

bool isSameTable(const Luau::TypeId a, const Luau::TypeId b)
{
    if (a == b)
        return true;

    // TODO: in some cases, the table in the first module doesnt point to the same ty as the table in the second
    // for example, in the first module (the one being required), the table may be unsealed
    // we check for location equality in these cases
    if (auto ttv1 = Luau::get<Luau::TableType>(a))
        if (auto ttv2 = Luau::get<Luau::TableType>(b))
            return !ttv1->definitionModuleName.empty() && ttv1->definitionModuleName == ttv2->definitionModuleName &&
                   ttv1->definitionLocation == ttv2->definitionLocation;

    return false;
}

// Determines whether the name matches a type reference in one of the provided generics
bool isTypeReference(Luau::AstName name, Luau::AstArray<Luau::AstGenericType> generics, Luau::AstArray<Luau::AstGenericTypePack> genericPacks)
{
    for (const auto& t : generics)
    {
        if (t.name == name)
            return true;
    }
    for (const auto& t : genericPacks)
    {
        if (t.name == name)
            return true;
    }
    return false;
}