#include "Platform/RobloxPlatform.hpp"

#include "LSP/Completion.hpp"
#include "LSP/Workspace.hpp"
#include "LSP/FileUtils.h"

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

static lsp::TextEdit createRequireTextEdit(const std::string& name, const std::string& path, size_t lineNumber, bool prependNewline = false)
{
    auto range = lsp::Range{{lineNumber, 0}, {lineNumber, 0}};
    auto importText = "local " + name + " = require(" + path + ")\n";
    if (prependNewline)
        importText = "\n" + importText;
    return {range, importText};
}

static lsp::CompletionItem createSuggestRequire(
    const std::string& name, const std::vector<lsp::TextEdit>& textEdits, const char* sortText, const std::string& path)
{
    std::string documentation;
    for (const auto& edit : textEdits)
        documentation += edit.newText;

    lsp::CompletionItem item;
    item.label = name;
    item.kind = lsp::CompletionItemKind::Module;
    item.detail = "Auto-import";
    item.documentation = {lsp::MarkupKind::Markdown, codeBlock("luau", documentation) + "\n\n" + path};
    item.insertText = name;
    item.sortText = sortText;

    item.additionalTextEdits = textEdits;

    return item;
}

static size_t getLengthEqual(const std::string& a, const std::string& b)
{
    size_t i = 0;
    for (; i < a.size() && i < b.size(); ++i)
    {
        if (a[i] != b[i])
            break;
    }
    return i;
}

static std::string optimiseAbsoluteRequire(const std::string& path)
{
    if (!Luau::startsWith(path, "game/"))
        return path;

    auto parts = Luau::split(path, '/');
    if (parts.size() > 2)
    {
        auto service = std::string(parts[1]);
        return service + "/" + Luau::join(std::vector(parts.begin() + 2, parts.end()), "/");
    }

    return path;
}

std::optional<Luau::AutocompleteEntryMap> RobloxPlatform::completionCallback(
    const std::string& tag, std::optional<const Luau::ClassType*> ctx, std::optional<std::string> contents, const Luau::ModuleName& moduleName)
{
    if (auto parentResult = LSPPlatform::completionCallback(tag, ctx, contents, moduleName))
        return parentResult;

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

void RobloxPlatform::handleSuggestImports(const TextDocument& textDocument, const Luau::SourceModule& module, const ClientConfiguration& config,
    size_t hotCommentsLineNumber, bool completingTypeReferencePrefix, std::vector<lsp::CompletionItem>& items)
{
    // Find all import calls
    RobloxFindImportsVisitor importsVisitor;
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
        size_t minimumLineNumber = hotCommentsLineNumber;
        size_t visitorMinimumLine = importsVisitor.getMinimumRequireLine();

        if (visitorMinimumLine > minimumLineNumber)
            minimumLineNumber = visitorMinimumLine;

        if (importsVisitor.firstRequireLine)
            minimumLineNumber = *importsVisitor.firstRequireLine >= minimumLineNumber ? (*importsVisitor.firstRequireLine) : minimumLineNumber;

        for (auto& [path, node] : virtualPathsToSourceNodes)
        {
            auto name = node->name;
            replaceAll(name, " ", "_");

            if (path == module.name || node->className != "ModuleScript" || importsVisitor.containsRequire(name))
                continue;
            if (auto scriptFilePath = getRealPathFromSourceNode(node); scriptFilePath && workspaceFolder->isIgnoredFile(*scriptFilePath, config))
                continue;

            std::string requirePath;
            std::vector<lsp::TextEdit> textEdits;

            // Compute the style of require
            bool isRelative = false;
            auto parent1 = getParentPath(module.name), parent2 = getParentPath(path);
            if (config.completion.imports.requireStyle == ImportRequireStyle::AlwaysRelative ||
                Luau::startsWith(path, "ProjectRoot/") || // All model projects should always require relatively
                (config.completion.imports.requireStyle != ImportRequireStyle::AlwaysAbsolute &&
                    (Luau::startsWith(module.name, path) || Luau::startsWith(path, module.name) || parent1 == parent2)))
            {
                requirePath = "./" + std::filesystem::relative(path, module.name).string();
                isRelative = true;
            }
            else
                requirePath = optimiseAbsoluteRequire(path);

            auto require = convertToScriptPath(requirePath);

            size_t lineNumber = minimumLineNumber;
            size_t bestLength = 0;
            for (auto& group : importsVisitor.requiresMap)
            {
                for (auto& [_, stat] : group)
                {
                    auto line = stat->location.end.line;

                    // HACK: We read the text of the require argument to sort the lines
                    // Note: requires may be in the form `require(path) :: any`, so we need to handle that too
                    Luau::AstExprCall* call = stat->values.data[0]->as<Luau::AstExprCall>();
                    if (auto assertion = stat->values.data[0]->as<Luau::AstExprTypeAssertion>())
                        call = assertion->expr->as<Luau::AstExprCall>();
                    if (!call)
                        continue;

                    auto location = call->args.data[0]->location;
                    auto range = lsp::Range{{location.begin.line, location.begin.column}, {location.end.line, location.end.column}};
                    auto argText = textDocument.getText(range);
                    auto length = getLengthEqual(argText, require);

                    if (length > bestLength && argText < require && line >= lineNumber)
                        lineNumber = line + 1;
                }
            }

            if (!isRelative)
            {
                // Service will be the first part of the path
                // If we haven't imported the service already, then we auto-import it
                auto service = requirePath.substr(0, requirePath.find('/'));
                if (!contains(importsVisitor.serviceLineMap, service))
                {
                    auto lineNumber = importsVisitor.findBestLineForService(service, hotCommentsLineNumber);
                    bool appendNewline = false;
                    // If there is no firstRequireLine, then the require that we insert will become the first require,
                    // so we use `.value_or(lineNumber)` to ensure it equals 0 and a newline is added
                    if (config.completion.imports.separateGroupsWithLine && importsVisitor.firstRequireLine.value_or(lineNumber) - lineNumber == 0)
                        appendNewline = true;
                    textEdits.emplace_back(createServiceTextEdit(service, lineNumber, appendNewline));
                }
            }

            // Whether we need to add a newline before the require to separate it from the services
            bool prependNewline = config.completion.imports.separateGroupsWithLine && importsVisitor.shouldPrependNewline(lineNumber);

            textEdits.emplace_back(createRequireTextEdit(node->name, require, lineNumber, prependNewline));

            items.emplace_back(createSuggestRequire(name, textEdits, isRelative ? SortText::AutoImports : SortText::AutoImportsAbsolute, path));
        }
    }
}
