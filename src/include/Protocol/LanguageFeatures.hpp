#pragma once

#include <optional>
#include <string>

#include "Protocol/Structures.hpp"

namespace lsp
{
struct HoverParams : TextDocumentPositionParams
{
};

struct Hover
{
    MarkupContent contents;
    std::optional<Range> range;
};
NLOHMANN_DEFINE_OPTIONAL(Hover, contents, range);


struct DefinitionParams : TextDocumentPositionParams
{
};

using DefinitionResult = std::vector<lsp::Location>;

struct TypeDefinitionParams : TextDocumentPositionParams
{
};

struct ReferenceContext
{
    bool includeDeclaration = false;
};
NLOHMANN_DEFINE_OPTIONAL(ReferenceContext, includeDeclaration);

struct ReferenceParams : TextDocumentPositionParams
{
    ReferenceContext context;
};
NLOHMANN_DEFINE_OPTIONAL(ReferenceParams, textDocument, position, context);

using ReferenceResult = std::optional<std::vector<Location>>;

struct RenameParams : TextDocumentPositionParams
{
    std::string newName;
};
NLOHMANN_DEFINE_OPTIONAL(RenameParams, textDocument, position, newName);

using RenameResult = std::optional<WorkspaceEdit>;

struct DocumentLinkParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentLinkParams, textDocument);

struct DocumentLink
{
    Range range;
    DocumentUri target; // TODO: potentially optional if we resolve later
    std::optional<std::string> tooltip;
    // std::optional<json> data; // for resolver
};
NLOHMANN_DEFINE_OPTIONAL(DocumentLink, range, target, tooltip);

struct DocumentSymbolParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentSymbolParams, textDocument);

enum struct SymbolKind
{
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26,
};

enum struct SymbolTag
{
    Deprecated = 1,
};

struct DocumentSymbol
{
    std::string name;
    std::optional<std::string> detail;
    SymbolKind kind = SymbolKind::Array;
    std::vector<SymbolTag> tags;
    bool deprecated = false;
    Range range;
    Range selectionRange;
    std::vector<DocumentSymbol> children;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentSymbol, name, detail, kind, tags, deprecated, range, selectionRange, children);

struct InlayHintParams
{
    TextDocumentIdentifier textDocument;
    Range range;
};
NLOHMANN_DEFINE_OPTIONAL(InlayHintParams, textDocument, range);

enum struct InlayHintKind
{
    Type = 1,
    Parameter = 2,
};

struct InlayHint
{
    Position position;
    std::string label;
    std::optional<InlayHintKind> kind;
    std::vector<TextEdit> textEdits;
    std::optional<std::string> tooltip;
    bool paddingLeft = false;
    bool paddingRight = false;
};

NLOHMANN_DEFINE_OPTIONAL(InlayHint, position, label, kind, textEdits, tooltip, paddingLeft, paddingRight);

using InlayHintResult = std::vector<InlayHint>;

} // namespace lsp
