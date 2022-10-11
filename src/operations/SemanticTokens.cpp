#include "LSP/LanguageServer.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/Protocol.hpp"
#include "LSP/LuauExt.hpp"
#include "LSP/SemanticTokens.hpp"

static void fillBuiltinGlobals(std::unordered_map<Luau::AstName, Luau::TypeId>& builtins, const Luau::AstNameTable& names, const Luau::ScopePtr& env)
{
    Luau::ScopePtr current = env;
    while (true)
    {
        for (auto& [global, binding] : current->bindings)
        {
            Luau::AstName name = names.get(global.c_str());

            if (name.value)
                builtins.insert_or_assign(name, binding.typeId);
        }

        if (current->parent)
            current = current->parent;
        else
            break;
    }
}

enum struct AstLocalInfo
{
    // local is self
    Self,
    // local is a function parameter
    Parameter,
};

static lsp::SemanticTokenTypes inferTokenType(Luau::TypeId* ty, lsp::SemanticTokenTypes base)
{
    if (!ty)
        return base;

    auto followedTy = Luau::follow(*ty);

    if (auto ftv = Luau::get<Luau::FunctionTypeVar>(followedTy))
    {
        if (ftv->hasSelf)
            return lsp::SemanticTokenTypes::Method;
        else
            return lsp::SemanticTokenTypes::Function;
    }
    else if (Luau::get<Luau::IntersectionTypeVar>(followedTy))
    {
        if (Luau::isOverloadedFunction(followedTy))
        {
            return lsp::SemanticTokenTypes::Function;
        }
    }
    else if (auto ttv = Luau::get<Luau::TableTypeVar>(followedTy))
    {
        if (ttv->name && ttv->name == "RBXScriptSignal")
        {
            return lsp::SemanticTokenTypes::Event;
        }
    }

    return base;
}

struct SemanticTokensVisitor : public Luau::AstVisitor
{
    Luau::ModulePtr module;
    std::vector<SemanticToken> tokens;
    std::unordered_map<Luau::AstLocal*, AstLocalInfo> localMap;
    std::unordered_map<Luau::AstName, Luau::TypeId> builtinGlobals;
    std::unordered_set<Luau::AstType*> syntheticTypes;

    explicit SemanticTokensVisitor(Luau::ModulePtr module, std::unordered_map<Luau::AstName, Luau::TypeId> builtinGlobals)
        : module(module)
        , builtinGlobals(builtinGlobals)
    {
    }

    // HACK: Luau introduces some synthetic tokens in the AST for types
    // { T } gets converted to { [number]: T } (where number is an introduced AstTypeReference)
    // string? gets converted to string | nil (where nil is an introduced AstTypeReference)
    // We do not want to highlight these synthetic tokens, as they don't exist in real code.
    // We apply visitors in these locations to try and check for synthetic tokens, and mark them as such
    bool visit(Luau::AstTypeTable* table) override
    {
        // If the indexer location is the same as a result type, the indexer was synthetically added
        if (table->indexer && table->indexer->indexType->location == table->indexer->resultType->location)
            syntheticTypes.emplace(table->indexer->indexType);
        return true;
    }

    bool visit(Luau::AstTypeUnion* unionType) override
    {
        // If the union contains a "nil" part, but it has the the size of only 1 column, then it is synthetic
        // Note that the union could contain > 2 parts:
        //  T? -> T | nil
        //  U | V? -> U | V | nil
        for (const auto& ty : unionType->types)
            if (auto ref = ty->as<Luau::AstTypeReference>())
                if (!ref->prefix && !ref->hasParameterList && ref->name == "nil" && ref->location.end.column == ref->location.begin.column + 1)
                    syntheticTypes.emplace(ref);
        return true;
    }


    bool visit(Luau::AstType* type) override
    {
        return true;
    }

    bool visit(Luau::AstTypeReference* ref) override
    {
        // Ignore synthetic type references
        if (syntheticTypes.find(ref) != syntheticTypes.end())
            return false;

        // HACK: The location information provided only gives location for the whole reference
        // but we do not want to highlight punctuation inside of it (Module.Type<Param> should not highlight . < >)
        // So we use the start position and consider the end positions separately.
        // Here, we make the assumption that there is no comments or newlines present in between the punctuation

        auto startPosition = ref->location.begin;
        // Highlight prefix if exists
        if (ref->prefix)
        {
            Luau::Position endPosition{startPosition.line, startPosition.column + static_cast<unsigned int>(strlen(ref->prefix->value))};
            tokens.emplace_back(SemanticToken{startPosition, endPosition, lsp::SemanticTokenTypes::Namespace, lsp::SemanticTokenModifiers::None});
            startPosition = {endPosition.line, endPosition.column + 1};
        }

        // Highlight name
        Luau::Position endPosition{startPosition.line, startPosition.column + static_cast<unsigned int>(strlen(ref->name.value))};
        tokens.emplace_back(SemanticToken{startPosition, endPosition, lsp::SemanticTokenTypes::Type, lsp::SemanticTokenModifiers::None});

        // Do not highlight parameters as they will be visited later
        return true;
    }

    bool visit(Luau::AstTypePack* type) override
    {
        return true;
    }

    bool visit(Luau::AstTypePackGeneric* type) override
    {
        // HACK: do not highlight punctuation
        Luau::Position endPosition{
            type->location.begin.line, type->location.begin.column + static_cast<unsigned int>(strlen(type->genericName.value))};
        tokens.emplace_back(SemanticToken{type->location.begin, endPosition, lsp::SemanticTokenTypes::Namespace, lsp::SemanticTokenModifiers::None});
        return true;
    }

    bool visit(Luau::AstStatLocal* local) override
    {
        auto scope = Luau::findScopeAtPosition(*module, local->location.begin);
        if (!scope)
            return true;

        for (auto var : local->vars)
        {
            auto ty = scope->lookup(var);
            if (ty)
            {
                auto followedTy = Luau::follow(*ty);
                auto type = inferTokenType(&followedTy, lsp::SemanticTokenTypes::Variable);
                tokens.emplace_back(SemanticToken{var->location.begin, var->location.end, type, lsp::SemanticTokenModifiers::None});
            }
        }

        return true;
    }

    // bool visit(Luau::AstStatFunction* func) override
    // {
    //     tokens.emplace_back(SemanticToken{
    //         func->name->location.begin, func->name->location.end, lsp::SemanticTokenTypes::Function, lsp::SemanticTokenModifiers::None});
    //     return true;
    // }

    // bool visit(Luau::AstStatLocalFunction* func) override
    // {
    //     tokens.emplace_back(SemanticToken{
    //         func->name->location.begin, func->name->location.end, lsp::SemanticTokenTypes::Function, lsp::SemanticTokenModifiers::None});
    //     return true;
    // }

    bool visit(Luau::AstExprFunction* func) override
    {
        if (func->self)
            localMap.insert_or_assign(func->self, AstLocalInfo::Self);

        for (auto arg : func->args)
        {
            tokens.emplace_back(
                SemanticToken{arg->location.begin, arg->location.end, lsp::SemanticTokenTypes::Parameter, lsp::SemanticTokenModifiers::None});
            localMap.insert_or_assign(arg, AstLocalInfo::Parameter);
        }
        return true;
    }

    bool visit(Luau::AstExprLocal* local) override
    {
        auto defaultType = lsp::SemanticTokenTypes::Variable;
        if (localMap.find(local->local) != localMap.end())
        {
            auto localInfo = localMap.at(local->local);
            if (localInfo == AstLocalInfo::Self)
            {
                tokens.emplace_back(SemanticToken{
                    local->location.begin, local->location.end, lsp::SemanticTokenTypes::Property, lsp::SemanticTokenModifiers::DefaultLibrary});

                return true;
            }
            else if (localInfo == AstLocalInfo::Parameter)
            {
                defaultType = lsp::SemanticTokenTypes::Parameter;
            }
        }

        auto type = inferTokenType(module->astTypes.find(local), defaultType);
        if (type == lsp::SemanticTokenTypes::Variable)
            return true;

        tokens.emplace_back(SemanticToken{local->location.begin, local->location.end, type, lsp::SemanticTokenModifiers::None});
        return true;
    }

    bool visit(Luau::AstExprGlobal* global) override
    {
        auto it = builtinGlobals.find(global->name);
        if (it != builtinGlobals.end() && strlen(global->name.value) > 0)
        {
            // SPECIAL CASE: if name is "Enum", classify it as an enum
            if (global->name == "Enum")
            {
                tokens.emplace_back(SemanticToken{
                    global->location.begin, global->location.end, lsp::SemanticTokenTypes::Enum, lsp::SemanticTokenModifiers::DefaultLibrary});
            }
            // If it starts with an uppercase letter, flag it as a class
            // Otherwise, flag it as a builtin
            else if (isupper(global->name.value[0]))
            {
                tokens.emplace_back(SemanticToken{
                    global->location.begin, global->location.end, lsp::SemanticTokenTypes::Class, lsp::SemanticTokenModifiers::DefaultLibrary});
            }
            else
            {
                auto type = inferTokenType(&it->second, lsp::SemanticTokenTypes::Variable);
                tokens.emplace_back(SemanticToken{global->location.begin, global->location.end, type,
                    lsp::SemanticTokenModifiers::DefaultLibrary | lsp::SemanticTokenModifiers::Readonly});
            }
        }
        else
        {
            auto ty = module->astTypes.find(global);
            if (!ty)
                return true;

            auto type = inferTokenType(ty, lsp::SemanticTokenTypes::Variable);
            if (type == lsp::SemanticTokenTypes::Variable)
                return true;

            tokens.emplace_back(SemanticToken{global->location.begin, global->location.end, type, lsp::SemanticTokenModifiers::None});
        }

        return true;
    }

    bool visit(Luau::AstExprIndexName* index) override
    {
        auto parentTy = module->astTypes.find(index->expr);
        if (!parentTy)
            return true;

        auto parentIsBuiltin = false;
        auto parentIsEnum = false;
        if (auto global = index->expr->as<Luau::AstExprGlobal>())
        {
            parentIsBuiltin = builtinGlobals.find(global->name) != builtinGlobals.end();
            if (parentIsBuiltin && global->name == "Enum")
                parentIsEnum = true;
        }

        auto ty = Luau::follow(*parentTy);
        if (auto prop = lookupProp(ty, std::string(index->index.value)))
        {
            auto defaultType = lsp::SemanticTokenTypes::Property;
            if (parentIsEnum)
                defaultType = lsp::SemanticTokenTypes::Enum;
            else if (Luau::hasTag(prop->tags, "EnumItem"))
                defaultType = lsp::SemanticTokenTypes::EnumMember;

            auto type = inferTokenType(&prop->type, defaultType);
            auto modifiers = lsp::SemanticTokenModifiers::None;
            if (parentIsBuiltin)
            {
                modifiers = modifiers | lsp::SemanticTokenModifiers::DefaultLibrary | lsp::SemanticTokenModifiers::Readonly;
            }
            else if (types::isMetamethod(std::string(index->index.value)))
            {
                modifiers = modifiers | lsp::SemanticTokenModifiers::DefaultLibrary;
            }
            tokens.emplace_back(SemanticToken{index->indexLocation.begin, index->indexLocation.end, type, modifiers});
        }

        return true;
    }

    bool visit(Luau::AstExprTable* tbl) override
    {
        for (auto item : tbl->items)
        {
            if (item.kind == Luau::AstExprTable::Item::Kind::Record)
            {
                auto type = inferTokenType(module->astTypes.find(item.value), lsp::SemanticTokenTypes::Property);
                tokens.emplace_back(SemanticToken{item.key->location.begin, item.key->location.end, type, lsp::SemanticTokenModifiers::None});
            }
        }

        return true;
    }

    bool visit(Luau::AstStatBlock* block) override
    {
        for (Luau::AstStat* stat : block->body)
        {
            stat->visit(this);
        }

        return false;
    }
};

std::vector<SemanticToken> getSemanticTokens(const Luau::Frontend& frontend, const Luau::ModulePtr module, const Luau::SourceModule* sourceModule)
{
    std::unordered_map<Luau::AstName, Luau::TypeId> builtinGlobals;
    fillBuiltinGlobals(builtinGlobals, *sourceModule->names, frontend.typeChecker.globalScope);

    SemanticTokensVisitor visitor{module, builtinGlobals};
    visitor.visit(sourceModule->root);
    return visitor.tokens;
}

size_t convertTokenType(const lsp::SemanticTokenTypes tokenType)
{
    return static_cast<size_t>(tokenType);
}

std::vector<size_t> packTokens(const TextDocument* textDocument, std::vector<SemanticToken>& tokens)
{
    // Sort the tokens into the correct ordering
    std::sort(tokens.begin(), tokens.end(),
        [](const SemanticToken& a, const SemanticToken& b)
        {
            return a.start < b.start;
        });

    // Pack the tokens
    std::vector<size_t> result;
    result.reserve(tokens.size() * 5); // Each token will take up 5 slots in the result

    size_t lastLine = 0;
    size_t lastChar = 0;

    for (auto& token : tokens)
    {
        auto start = textDocument->convertPosition(token.start);
        auto end = textDocument->convertPosition(token.end);

        auto line = start.line;
        auto startChar = start.character;

        auto deltaLine = line - lastLine;
        auto deltaStartChar = deltaLine == 0 ? startChar - lastChar : startChar;
        auto length = end.character - start.character;

        result.insert(
            result.end(), {deltaLine, deltaStartChar, length, convertTokenType(token.tokenType), static_cast<size_t>(token.tokenModifiers)});

        lastLine = line;
        lastChar = startChar;
    }

    return result;
}

std::optional<lsp::SemanticTokens> WorkspaceFolder::semanticTokens(const lsp::SemanticTokensParams& params)
{
    auto moduleName = fileResolver.getModuleName(params.textDocument.uri);
    auto textDocument = fileResolver.getTextDocument(moduleName);
    if (!textDocument)
    {
        // TODO: REMOVE TRACE LOGGING
        std::vector<std::string> managed;
        managed.reserve(fileResolver.managedFiles.size());
        for (auto [file, _] : fileResolver.managedFiles)
            managed.push_back(file);
        client->sendLogMessage(lsp::MessageType::Error, "managed document info: " + json(managed).dump());
        throw JsonRpcException(lsp::ErrorCode::RequestFailed, "No managed text document for " + moduleName);
    }

    // Run the type checker to ensure we are up to date
    if (frontend.isDirty(moduleName))
        frontend.check(moduleName);

    auto sourceModule = frontend.getSourceModule(moduleName);
    auto module = frontend.moduleResolver.getModule(moduleName);
    if (!sourceModule || !module)
        return std::nullopt;

    auto tokens = getSemanticTokens(frontend, module, sourceModule);
    lsp::SemanticTokens result;
    result.data = packTokens(textDocument, tokens);
    return result;
}

std::optional<lsp::SemanticTokens> LanguageServer::semanticTokens(const lsp::SemanticTokensParams& params)
{
    auto workspace = findWorkspace(params.textDocument.uri);
    return workspace->semanticTokens(params);
}