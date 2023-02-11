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
    std::optional<Range> range = std::nullopt;
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
    std::optional<std::string> tooltip = std::nullopt;
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
    std::optional<std::string> detail = std::nullopt;
    SymbolKind kind = SymbolKind::Array;
    std::vector<SymbolTag> tags{};
    bool deprecated = false;
    Range range;
    Range selectionRange;
    std::vector<DocumentSymbol> children{};
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
    std::optional<InlayHintKind> kind = std::nullopt;
    std::vector<TextEdit> textEdits{};
    std::optional<std::string> tooltip = std::nullopt;
    bool paddingLeft = false;
    bool paddingRight = false;
};

NLOHMANN_DEFINE_OPTIONAL(InlayHint, position, label, kind, textEdits, tooltip, paddingLeft, paddingRight);

using InlayHintResult = std::vector<InlayHint>;

struct DocumentColorParams
{
    /**
     * The text document.
     */
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(DocumentColorParams, textDocument);

struct Color
{
    /**
     * The red component of this color in the range [0-1].
     */
    double red = 0.0;

    /**
     * The green component of this color in the range [0-1].
     */
    double green = 0.0;

    /**
     * The blue component of this color in the range [0-1].
     */
    double blue = 0.0;

    /**
     * The alpha component of this color in the range [0-1].
     */
    double alpha = 0.0;
};
NLOHMANN_DEFINE_OPTIONAL(Color, red, green, blue, alpha);

struct ColorInformation
{
    /**
     * The range in the document where this color appears.
     */
    Range range;

    /**
     * The actual color value for this color range.
     */
    Color color;
};
NLOHMANN_DEFINE_OPTIONAL(ColorInformation, range, color);

using DocumentColorResult = std::vector<ColorInformation>;

struct ColorPresentationParams
{
    /**
     * The text document.
     */
    TextDocumentIdentifier textDocument;

    /**
     * The color information to request presentations for.
     */
    Color color;

    /**
     * The range where the color would be inserted. Serves as a context.
     */
    Range range;
};
NLOHMANN_DEFINE_OPTIONAL(ColorPresentationParams, textDocument, color, range);


struct ColorPresentation
{
    /**
     * The label of this color presentation. It will be shown on the color
     * picker header. By default this is also the text that is inserted when
     * selecting this color presentation.
     */
    std::string label;
    /**
     * An [edit](#TextEdit) which is applied to a document when selecting
     * this presentation for the color. When `falsy` the
     * [label](#ColorPresentation.label) is used.
     */
    std::optional<TextEdit> textEdit = std::nullopt;
    /**
     * An optional array of additional [text edits](#TextEdit) that are applied
     * when selecting this color presentation. Edits must not overlap with the
     * main [edit](#ColorPresentation.textEdit) nor with themselves.
     */
    std::optional<std::vector<TextEdit>> additionalTextEdits = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(ColorPresentation, label, textEdit, additionalTextEdits);

using ColorPresentationResult = std::vector<ColorPresentation>;

} // namespace lsp
