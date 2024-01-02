#include "Platform/RobloxPlatform.hpp"

#include "LSP/Completion.hpp"
#include "LSP/Workspace.hpp"

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

static lsp::TextEdit createServiceTextEdit(const std::string& name, size_t lineNumber, bool appendNewline = false)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = "local " + name + " = game:GetService(\"" + name + "\")\n";
    if (appendNewline)
        importText += "\n";
    return {range, importText};
}

static lsp::CompletionItem createSuggestService(const std::string& service, size_t lineNumber, bool appendNewline = false)
{
    auto textEdit = createServiceTextEdit(service, lineNumber, appendNewline);

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
    const std::string& tag, std::optional<const Luau::ClassType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
{
    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

    if (tag == "ClassNames")
    {
        if (auto instanceType = workspaceFolder->frontend.globals.globalScope->lookupType("Instance"))
        {
            if (auto* ctv = Luau::get<Luau::ClassType>(instanceType->type))
            {
                Luau::AutocompleteEntryMap result;
                for (auto& [_, ty] : workspaceFolder->frontend.globals.globalScope->exportedTypeBindings)
                {
                    if (auto* c = Luau::get<Luau::ClassType>(ty.type))
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
        {
            Luau::AutocompleteEntryMap result;
            auto ctv = ctx.value();
            while (ctv)
            {
                for (auto& [propName, prop] : ctv->props)
                {
                    // Don't include functions or events
                    auto ty = Luau::follow(prop.type());
                    if (Luau::get<Luau::FunctionType>(ty) || Luau::isOverloadedFunction(ty))
                        continue;
                    else if (auto ttv = Luau::get<Luau::TableType>(ty); ttv && ttv->name && ttv->name.value() == "RBXScriptSignal")
                        continue;

                    result.insert_or_assign(
                        propName, Luau::AutocompleteEntry{Luau::AutocompleteEntryKind::String, workspaceFolder->frontend.builtinTypes->stringType,
                                      false, false, Luau::TypeCorrectKind::Correct});
                }
                if (ctv->parent)
                    ctv = Luau::get<Luau::ClassType>(*ctv->parent);
                else
                    break;
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
    if (auto dataModelType = frontend.globalsForAutocomplete.globalScope->lookupType("ServiceProvider");
        dataModelType && Luau::get<Luau::ClassType>(dataModelType->type) && entry.containingClass &&
        Luau::isSubclass(entry.containingClass.value(), Luau::get<Luau::ClassType>(dataModelType->type)) && !entry.wrongIndexType)
    {
        if (auto it = std::find(std::begin(COMMON_SERVICE_PROVIDER_PROPERTIES), std::end(COMMON_SERVICE_PROVIDER_PROPERTIES), name);
            it != std::end(COMMON_SERVICE_PROVIDER_PROPERTIES))
            return SortText::PrioritisedSuggestion;
    }

    // If calling a property on an Instance, then prioritise these properties
    else if (auto instanceType = frontend.globalsForAutocomplete.globalScope->lookupType("Instance");
             instanceType && Luau::get<Luau::ClassType>(instanceType->type) && entry.containingClass &&
             Luau::isSubclass(entry.containingClass.value(), Luau::get<Luau::ClassType>(instanceType->type)) && !entry.wrongIndexType)
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

void RobloxPlatform::handleSuggestImports(const ClientConfiguration& config, FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber,
    bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
{
    if (!config.platform.roblox.suggestServices || !config.completion.imports.suggestServices || completingTypeReferencePrefix)
        return;

    // Suggest services
    auto robloxVisitor = dynamic_cast<RobloxFindImportsVisitor*>(importsVisitor);
    if (robloxVisitor == nullptr)
        return;

    std::optional<RobloxDefinitionsFileMetadata> metadata = workspaceFolder->definitionsFileMetadata;

    auto services = metadata.has_value() ? metadata->SERVICES : std::vector<std::string>{};
    for (auto& service : services)
    {
        // ASSUMPTION: if the service was defined, it was defined with the exact same name
        if (contains(robloxVisitor->serviceLineMap, service))
            continue;

        size_t lineNumber = robloxVisitor->findBestLineForService(service, hotCommentsLineNumber);

        bool appendNewline = false;
        if (config.completion.imports.separateGroupsWithLine && importsVisitor->firstRequireLine &&
            importsVisitor->firstRequireLine.value() - lineNumber == 0)
            appendNewline = true;

        items.emplace_back(createSuggestService(service, lineNumber, appendNewline));
    }
}

void RobloxPlatform::handleRequireAutoImport(const std::string& requirePath, size_t lineNumber, bool isRelative, const ClientConfiguration& config,
    FindImportsVisitor* importsVisitor, size_t hotCommentsLineNumber, std::vector<lsp::TextEdit>& textEdits)
{
    if (!isRelative)
        return;

    auto robloxVisitor = dynamic_cast<RobloxFindImportsVisitor*>(importsVisitor);
    if (robloxVisitor == nullptr)
        return;

    // Service will be the first part of the path
    // If we haven't imported the service already, then we auto-import it
    auto service = requirePath.substr(0, requirePath.find('/'));
    if (!contains(robloxVisitor->serviceLineMap, service))
    {
        auto lineNumber = robloxVisitor->findBestLineForService(service, hotCommentsLineNumber);
        bool appendNewline = false;
        // If there is no firstRequireLine, then the require that we insert will become the first require,
        // so we use `.value_or(lineNumber)` to ensure it equals 0 and a newline is added
        if (config.completion.imports.separateGroupsWithLine && robloxVisitor->firstRequireLine.value_or(lineNumber) - lineNumber == 0)
            appendNewline = true;
        textEdits.emplace_back(createServiceTextEdit(service, lineNumber, appendNewline));
    }
}
