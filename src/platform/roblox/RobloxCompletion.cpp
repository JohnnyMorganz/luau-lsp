#include "Platform/RobloxPlatform.hpp"

#include "Luau/TimeTrace.h"
#include "LuauFileUtils.hpp"

#include "LSP/Completion.hpp"

#include "Platform/AutoImports.hpp"
#include "Platform/InstanceRequireAutoImporter.hpp"
#include "Platform/StringRequireAutoImporter.hpp"

LUAU_FASTFLAG(LuauSolverV2)

enum class SelectorContext
{
    ClassName,
    PseudoClass,
    PropertyName,
    None
};

static SelectorContext detectSelectorContext(const std::string& contents)
{
    // Find the end of meaningful content (strip trailing identifier characters to get the "trigger" position)
    size_t end = contents.size();
    while (end > 0 && (std::isalnum(static_cast<unsigned char>(contents[end - 1])) || contents[end - 1] == '_'))
        end--;

    if (end == 0)
        return SelectorContext::ClassName;

    char trigger = contents[end - 1];

    if (trigger == '>' || trigger == ',' || trigger == '(')
        return SelectorContext::ClassName;

    if (trigger == ':')
        return SelectorContext::PseudoClass;

    if (trigger == '[')
        return SelectorContext::PropertyName;

    if (trigger == '.' || trigger == '#')
        return SelectorContext::None;

    if (trigger == ' ' || trigger == '\t')
        return SelectorContext::ClassName;

    return SelectorContext::None;
}

static std::string findPrecedingClassName(const std::string& contents)
{
    // Scan backward from the last '[' to find a preceding uppercase-starting identifier (class name)
    size_t bracketPos = contents.rfind('[');
    if (bracketPos == std::string::npos || bracketPos == 0)
        return "";

    // Skip whitespace before '['
    size_t pos = bracketPos - 1;
    while (pos > 0 && (contents[pos] == ' ' || contents[pos] == '\t'))
        pos--;

    // Collect identifier characters backward
    size_t identEnd = pos + 1;
    while (pos > 0 && (std::isalnum(static_cast<unsigned char>(contents[pos - 1])) || contents[pos - 1] == '_'))
        pos--;
    // Check the character at pos itself
    if (!(std::isalnum(static_cast<unsigned char>(contents[pos])) || contents[pos] == '_'))
        pos++;

    if (pos >= identEnd)
        return "";

    std::string ident = contents.substr(pos, identEnd - pos);

    // Class names start with uppercase
    if (!ident.empty() && std::isupper(static_cast<unsigned char>(ident[0])))
    {
        // Make sure it's not preceded by '.' or '#' (which would make it a tag/name selector)
        if (pos > 0 && (contents[pos - 1] == '.' || contents[pos - 1] == '#'))
            return "";
        return ident;
    }

    return "";
}

static Luau::AutocompleteEntryMap getPropertiesOfType(const Luau::ExternType* ctv, Luau::TypeId stringType)
{
    Luau::AutocompleteEntryMap result;
    while (ctv)
    {
        for (auto& [propName, prop] : ctv->props)
        {
            LUAU_ASSERT(prop.readTy);
            auto ty = Luau::follow(*prop.readTy);
            if (Luau::get<Luau::FunctionType>(ty) || Luau::isOverloadedFunction(ty))
                continue;
            else if (auto ttv = Luau::get<Luau::TableType>(ty); ttv && ttv->name && ttv->name.value() == "RBXScriptSignal")
                continue;
            else if (Luau::hasTag(prop, kSourcemapGeneratedTag))
                continue;

            result.insert_or_assign(
                propName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, stringType, false, false, Luau::TypeCorrectKind::Correct});
        }
        if (ctv->parent)
            ctv = Luau::get<Luau::ExternType>(*ctv->parent);
        else
            break;
    }
    return result;
}

static constexpr const char* COMMON_SERVICES[] = {
    "Players",
    "ReplicatedStorage",
    "ServerStorage",
    "MessagingService",
    "TeleportService",
    "HttpService",
    "CollectionService",
    "DataStoreService",
    "ContextActionService",
    "UserInputService",
    "Teams",
    "Chat",
    "TextService",
    "TextChatService",
    "GamepadService",
    "VoiceChatService",
};

static constexpr const char* COMMON_INSTANCE_PROPERTIES[] = {
    "Parent",
    "Name",
    // Methods
    "FindFirstChild",
    "IsA",
    "Destroy",
    "GetAttribute",
    "GetChildren",
    "GetDescendants",
    "WaitForChild",
    "Clone",
    "SetAttribute",
};

static constexpr const char* COMMON_SERVICE_PROVIDER_PROPERTIES[] = {
    "GetService",
};

static lsp::CompletionItem createSuggestService(const std::string& service, size_t lineNumber, bool appendNewline = false)
{
    auto textEdit = Luau::LanguageServer::AutoImports::createServiceTextEdit(service, lineNumber, appendNewline);

    lsp::CompletionItem item;
    item.label = service;
    item.kind = lsp::CompletionItemKind::Class;
    item.detail = "Auto-import";
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", textEdit.newText)};
    item.insertText = service;
    item.sortText = SortText::AutoImports;

    item.additionalTextEdits.emplace_back(textEdit);

    return item;
}

std::optional<Luau::AutocompleteEntryMap> RobloxPlatform::completionCallback(
    const std::string& tag, std::optional<const Luau::ExternType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
{
    if (auto parentResult = LSPPlatform::completionCallback(tag, ctx, contents, moduleName))
        return parentResult;

    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

    if (tag == "ClassNames")
    {
        if (auto instanceType = workspaceFolder->frontend.globals.globalScope->lookupType("Instance"))
        {
            if (auto* ctv = Luau::get<Luau::ExternType>(instanceType->type))
            {
                Luau::AutocompleteEntryMap result;
                for (auto& [_, ty] : workspaceFolder->frontend.globals.globalScope->exportedTypeBindings)
                {
                    if (auto* c = Luau::get<Luau::ExternType>(ty.type))
                    {
                        // Check if the ctv is a subclass of instance
                        if (Luau::isSubclass(c, ctv))

                            result.insert_or_assign(
                                c->name, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                             workspaceFolder->frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});
                    }
                }

                return result;
            }
        }
    }
    else if (tag == "Properties")
    {
        if (ctx && ctx.value())
            return getPropertiesOfType(ctx.value(), workspaceFolder->frontend.builtinTypes->stringType);
    }
    else if (tag == "Children")
    {
        if (auto ctv = ctx.value())
        {
            Luau::AutocompleteEntryMap result;
            for (auto& [propName, prop] : ctv->props)
            {
                if (Luau::hasTag(prop, kSourcemapGeneratedTag) &&
                    !(prop.readTy && (Luau::is<Luau::FunctionType>(*prop.readTy) || Luau::isOverloadedFunction(*prop.readTy))))
                    result.insert_or_assign(
                        propName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType,
                                      false, false, Luau::TypeCorrectKind::Correct});
            }
            return result;
        }
    }
    else if (tag == "Enums")
    {
        auto it = workspaceFolder->frontend.globals.globalScope->importedTypeBindings.find("Enum");
        if (it == workspaceFolder->frontend.globals.globalScope->importedTypeBindings.end())
            return std::nullopt;

        Luau::AutocompleteEntryMap result;
        for (auto& [enumName, _] : it->second)
            result.insert_or_assign(enumName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                                  workspaceFolder->frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});

        return result;
    }
    else if (tag == "CreatableInstances")
    {
        Luau::AutocompleteEntryMap result;
        if (metadata)
        {
            for (const auto& className : metadata->CREATABLE_INSTANCES)
                result.insert_or_assign(
                    className, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                                   false, Luau::TypeCorrectKind::Correct});
        }
        return result;
    }
    else if (tag == "Services")
    {
        Luau::AutocompleteEntryMap result;

        // We are autocompleting a `game:GetService("$1")` call, so we set a flag to
        // highlight this so that we can prioritise common services first in the list
        if (metadata)
        {
            for (const auto& className : metadata->SERVICES)
                result.insert_or_assign(
                    className, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false,
                                   false, Luau::TypeCorrectKind::Correct});
        }

        return result;
    }
    else if (tag == "QuerySelector")
    {
        if (!contents.has_value())
            return std::nullopt;

        auto context = detectSelectorContext(contents.value());

        if (context == SelectorContext::ClassName)
        {
            if (auto instanceType = workspaceFolder->frontend.globals.globalScope->lookupType("Instance"))
            {
                if (auto* instanceCtv = Luau::get<Luau::ExternType>(instanceType->type))
                {
                    Luau::AutocompleteEntryMap result;
                    for (auto& [_, ty] : workspaceFolder->frontend.globals.globalScope->exportedTypeBindings)
                    {
                        if (auto* c = Luau::get<Luau::ExternType>(ty.type))
                        {
                            if (Luau::isSubclass(c, instanceCtv))
                                result.insert_or_assign(c->name,
                                    Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String,
                                        workspaceFolder->frontend.builtinTypes->stringType, false, false, Luau::TypeCorrectKind::Correct});
                        }
                    }
                    return result;
                }
            }
        }
        else if (context == SelectorContext::PseudoClass)
        {
            Luau::AutocompleteEntryMap result;
            for (const auto& pseudoClass : {"not", "has"})
            {
                Luau::AutocompleteEntry entry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType, false, false,
                    Luau::TypeCorrectKind::Correct};
                entry.tags.push_back("SelectorPseudoClass");
                result.insert_or_assign(pseudoClass, std::move(entry));
            }
            return result;
        }
        else if (context == SelectorContext::PropertyName)
        {
            std::string className = findPrecedingClassName(contents.value());
            std::string lookupName = className.empty() ? "Instance" : className;
            if (auto classType = workspaceFolder->frontend.globals.globalScope->lookupType(lookupName))
            {
                if (auto* ctv = Luau::get<Luau::ExternType>(classType->type))
                    return getPropertiesOfType(ctv, workspaceFolder->frontend.builtinTypes->stringType);
            }
        }

        return std::nullopt;
    }

    return std::nullopt;
}

const char* RobloxPlatform::handleSortText(
    const Luau::Frontend& frontend, const std::string& name, const Luau::AutocompleteEntry& entry, const std::unordered_set<std::string>& tags)
{
    // If it's a `game:GetSerivce("$1")` call, then prioritise common services
    if (tags.count("Services"))
        if (auto it = std::find(std::begin(COMMON_SERVICES), std::end(COMMON_SERVICES), name); it != std::end(COMMON_SERVICES))
            return SortText::PrioritisedSuggestion;

    // If calling a property on ServiceProvider, then prioritise these properties
    auto& completionGlobals = FFlag::LuauSolverV2 ? frontend.globals : frontend.globalsForAutocomplete;
    if (auto dataModelType = completionGlobals.globalScope->lookupType("ServiceProvider");
        dataModelType && Luau::get<Luau::ExternType>(dataModelType->type) && entry.containingExternType &&
        Luau::isSubclass(entry.containingExternType.value(), Luau::get<Luau::ExternType>(dataModelType->type)) && !entry.wrongIndexType)
    {
        if (auto it = std::find(std::begin(COMMON_SERVICE_PROVIDER_PROPERTIES), std::end(COMMON_SERVICE_PROVIDER_PROPERTIES), name);
            it != std::end(COMMON_SERVICE_PROVIDER_PROPERTIES))
            return SortText::PrioritisedSuggestion;
    }

    // If calling a property on an Instance, then prioritise these properties
    else if (auto instanceType = completionGlobals.globalScope->lookupType("Instance");
        instanceType && Luau::get<Luau::ExternType>(instanceType->type) && entry.containingExternType &&
        Luau::isSubclass(entry.containingExternType.value(), Luau::get<Luau::ExternType>(instanceType->type)) && !entry.wrongIndexType)
    {
        if (auto it = std::find(std::begin(COMMON_INSTANCE_PROPERTIES), std::end(COMMON_INSTANCE_PROPERTIES), name);
            it != std::end(COMMON_INSTANCE_PROPERTIES))
            return SortText::PrioritisedSuggestion;
    }

    return nullptr;
}

std::optional<lsp::CompletionItemKind> RobloxPlatform::handleEntryKind(const Luau::AutocompleteEntry& entry)
{
    if (entry.type.has_value())
    {
        auto id = Luau::follow(entry.type.value());

        if (auto ttv = Luau::get<Luau::TableType>(id))
        {
            // Special case the RBXScriptSignal type as a connection
            if (ttv->name && ttv->name.value() == "RBXScriptSignal")
                return lsp::CompletionItemKind::Event;
        }
    }

    return std::nullopt;
}

void RobloxPlatform::handleSuggestImports(const TextDocument& textDocument, const Luau::SourceModule& module, const ClientConfiguration& config,
    size_t hotCommentsLineNumber, bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
{
    LUAU_TIMETRACE_SCOPE("RobloxPlatform::handleSuggestImports", "LSP");

    // Find all import calls
    Luau::LanguageServer::AutoImports::RobloxFindImportsVisitor importsVisitor;
    importsVisitor.visit(module.root);

    if (config.completion.imports.suggestServices && !completingTypeReferencePrefix)
    {
        std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

        auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};
        for (auto& service : services)
        {
            // ASSUMPTION: if the service was defined, it was defined with the exact same name
            if (contains(importsVisitor.serviceLineMap, service))
                continue;

            if ((!config.completion.imports.includedServices.empty() && !contains(config.completion.imports.includedServices, service)) ||
                contains(config.completion.imports.excludedServices, service))
                continue;

            size_t lineNumber = importsVisitor.findBestLineForService(service, hotCommentsLineNumber);

            bool appendNewline = false;
            if (config.completion.imports.separateGroupsWithLine && importsVisitor.firstRequireLine &&
                importsVisitor.firstRequireLine.value() - lineNumber == 0)
                appendNewline = true;

            items.emplace_back(createSuggestService(service, lineNumber, appendNewline));
        }
    }

    if (config.completion.imports.suggestRequires)
    {
        if (config.completion.imports.stringRequires.enabled)
        {
            Luau::LanguageServer::AutoImports::StringRequireAutoImporterContext ctx{
                module.name,
                Luau::NotNull(&textDocument),
                Luau::NotNull(&workspaceFolder->frontend),
                Luau::NotNull(workspaceFolder),
                Luau::NotNull(&config.completion.imports),
                hotCommentsLineNumber,
                Luau::NotNull(&importsVisitor),
            };

            return Luau::LanguageServer::AutoImports::suggestStringRequires(ctx, items);
        }
        else
        {
            Luau::LanguageServer::AutoImports::InstanceRequireAutoImporterContext ctx{
                module.name,
                Luau::NotNull(&textDocument),
                Luau::NotNull(&workspaceFolder->frontend),
                Luau::NotNull(workspaceFolder),
                Luau::NotNull(&config.completion.imports),
                hotCommentsLineNumber,
                Luau::NotNull(&importsVisitor),
                Luau::NotNull(this),
            };

            return Luau::LanguageServer::AutoImports::suggestInstanceRequires(ctx, items);
        }
    }
}
