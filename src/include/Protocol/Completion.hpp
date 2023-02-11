#pragma once

#include <string>
#include <vector>
#include <optional>
#include "Protocol/Structures.hpp"

namespace lsp
{
enum struct CompletionItemKind
{
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

enum struct InsertTextFormat
{
    PlainText = 1,
    Snippet = 2,
};

enum struct CompletionItemTag
{
    Deprecated = 1,
};

enum struct InsertTextMode
{
    AsIs = 1,
    AdjustIndentation = 2,
};

enum struct CompletionTriggerKind
{
    Invoked = 1,
    TriggerCharacter = 2,
    TriggerForIncompleteCompletions = 3
};

struct CompletionContext
{
    CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;
    std::optional<std::string> triggerCharacter = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionContext, triggerKind, triggerCharacter);

struct CompletionParams : TextDocumentPositionParams
{
    std::optional<CompletionContext> context = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionParams, textDocument, position, context);

struct CompletionItemLabelDetails
{
    std::optional<std::string> detail = std::nullopt;
    std::optional<std::string> description = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItemLabelDetails, detail, description);

struct CompletionItem
{
    std::string label;
    std::optional<CompletionItemLabelDetails> labelDetails = std::nullopt;
    std::optional<CompletionItemKind> kind = std::nullopt;
    std::optional<std::vector<CompletionItemTag>> tags = std::nullopt;
    std::optional<std::string> detail = std::nullopt;
    std::optional<MarkupContent> documentation = std::nullopt;
    bool deprecated = false;
    bool preselect = false;
    std::optional<std::string> sortText = std::nullopt;
    std::optional<std::string> filterText = std::nullopt;
    std::optional<std::string> insertText = std::nullopt;
    InsertTextFormat insertTextFormat = InsertTextFormat::PlainText;
    std::optional<InsertTextMode> insertTextMode = std::nullopt;
    std::optional<TextEdit> textEdit = std::nullopt;
    std::optional<std::string> textEditString = std::nullopt;
    std::vector<TextEdit> additionalTextEdits{};
    std::optional<std::vector<std::string>> commitCharacters = std::nullopt;
    std::optional<Command> command = std::nullopt;
    // TODO: data?
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItem, label, labelDetails, kind, tags, detail, documentation, deprecated, preselect, sortText, filterText,
    insertText, insertTextFormat, insertTextMode, textEdit, textEditString, additionalTextEdits, commitCharacters, command);
} // namespace lsp