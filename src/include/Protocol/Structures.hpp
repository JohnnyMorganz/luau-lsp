#pragma once

#include <string>
#include <vector>

#include "LSP/Uri.hpp"
#include "Protocol/Base.hpp"

namespace lsp
{
using URI = Uri;
using DocumentUri = Uri;

/**
 * A type indicating how positions are encoded,
 * specifically what column offsets mean.
 *
 * @since 3.17.0
 */
enum struct PositionEncodingKind
{
    /**
     * Character offsets count UTF-8 code units.
     */
    UTF8,

    /**
     * Character offsets count UTF-16 code units.
     *
     * This is the default and must always be supported
     * by servers
     */
    UTF16,

    /**
     * Character offsets count UTF-32 code units.
     *
     * Implementation note: these are the same as Unicode code points,
     * so this `PositionEncodingKind` may also be used for an
     * encoding-agnostic representation of character offsets.
     */
    UTF32
};
NLOHMANN_JSON_SERIALIZE_ENUM(
    PositionEncodingKind, {{PositionEncodingKind::UTF8, "utf-8"}, {PositionEncodingKind::UTF16, "utf-16"}, {PositionEncodingKind::UTF32, "utf-32"}})

struct WorkspaceFolder
{
    DocumentUri uri;
    std::string name;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceFolder, uri, name)


struct TextDocumentIdentifier
{
    DocumentUri uri;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextDocumentIdentifier, uri)

struct Position
{
    size_t line = 0;
    size_t character = 0;
    friend bool operator==(const Position& lhs, const Position& rhs);
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Position, line, character)
inline bool operator==(const Position& lhs, const Position& rhs)
{
    return lhs.line == rhs.line && lhs.character == rhs.character;
}
inline bool operator<(const Position& lhs, const Position& rhs)
{
    return lhs.line < rhs.line || (lhs.line == rhs.line && lhs.character < rhs.character);
}
inline bool operator>(const Position& lhs, const Position& rhs)
{
    return lhs.line > rhs.line || (lhs.line == rhs.line && lhs.character > rhs.character);
}

struct Range
{
    Position start;
    Position end;

    bool operator==(const Range& other) const
    {
        return start == other.start && end == other.end;
    }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Range, start, end)

struct TextDocumentItem
{
    DocumentUri uri;
    std::string languageId;
    size_t version = 0;
    std::string text;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextDocumentItem, uri, languageId, version, text)

struct VersionedTextDocumentIdentifier : TextDocumentIdentifier
{
    size_t version = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VersionedTextDocumentIdentifier, uri, version)

struct TextDocumentPositionParams
{
    TextDocumentIdentifier textDocument;
    Position position;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TextDocumentPositionParams, textDocument, position)

struct TextEdit
{
    Range range;
    std::string newText;
};
NLOHMANN_DEFINE_OPTIONAL(TextEdit, range, newText)

struct Location
{
    DocumentUri uri;
    Range range;

    bool operator==(const Location& other)
    {
        return uri == other.uri && range == other.range;
    }
};
NLOHMANN_DEFINE_OPTIONAL(Location, uri, range)

struct Command
{
    std::string title;
    std::string command;
    std::vector<json> arguments{};
};
NLOHMANN_DEFINE_OPTIONAL(Command, title, command, arguments)


enum struct MarkupKind
{
    PlainText,
    Markdown,
};
NLOHMANN_JSON_SERIALIZE_ENUM(MarkupKind, {{MarkupKind::PlainText, "plaintext"}, {MarkupKind::Markdown, "markdown"}})

struct MarkupContent
{
    MarkupKind kind = MarkupKind::PlainText;
    std::string value;
};
NLOHMANN_DEFINE_OPTIONAL(MarkupContent, kind, value)

/**
 * A string value used as a snippet is a template which allows to insert text
 * and to control the editor cursor when insertion happens.
 *
 * A snippet can define tab stops and placeholders with `$1`, `$2`
 * and `${3:foo}`. `$0` defines the final tab stop, it defaults to
 * the end of the snippet. Variables are defined with `$name` and
 * `${name:default value}`.
 *
 * @since 3.18.0
 */
struct StringValue
{
    std::string kind = "snippet";
    std::string value;
};
NLOHMANN_DEFINE_OPTIONAL(StringValue, kind, value)

/**
 * An interactive text edit.
 *
 * @since 3.18.0
 */
struct SnippetTextEdit
{
    Range range;
    StringValue snippet;
};
NLOHMANN_DEFINE_OPTIONAL(SnippetTextEdit, range, snippet)
} // namespace lsp

namespace nlohmann
{
// An edit in a `TextDocumentEdit` is either a plain `TextEdit` or, when the client
// supports it, a `SnippetTextEdit` (@since 3.18.0) carrying a cursor/tabstop-aware snippet.
template<>
struct adl_serializer<std::variant<lsp::TextEdit, lsp::SnippetTextEdit>>
{
    static void to_json(json& j, const std::variant<lsp::TextEdit, lsp::SnippetTextEdit>& data)
    {
        if (auto textEdit = std::get_if<lsp::TextEdit>(&data))
            j = *textEdit;
        else if (auto snippetTextEdit = std::get_if<lsp::SnippetTextEdit>(&data))
            j = *snippetTextEdit;
    }

    static void from_json(const json& j, std::variant<lsp::TextEdit, lsp::SnippetTextEdit>& data)
    {
        if (j.contains("snippet"))
            data = j.get<lsp::SnippetTextEdit>();
        else
            data = j.get<lsp::TextEdit>();
    }
};
} // namespace nlohmann

namespace lsp
{
/**
 * Describes textual changes on a single text document, optionally including
 * `SnippetTextEdit`s (@since 3.18.0) alongside plain `TextEdit`s.
 */
struct TextDocumentEdit
{
    VersionedTextDocumentIdentifier textDocument;
    std::vector<std::variant<TextEdit, SnippetTextEdit>> edits;
};
NLOHMANN_DEFINE_OPTIONAL(TextDocumentEdit, textDocument, edits)

struct WorkspaceEdit
{
    // TODO: this is optional and there are other options provided
    std::unordered_map<Uri, std::vector<TextEdit>, UriHash> changes{};

    /**
     * Versioned document changes for a single document, used instead of
     * `changes` when we need to encode `SnippetTextEdit`s
     * (@since 3.18.0), which are only permitted here.
     */
    std::optional<std::vector<TextDocumentEdit>> documentChanges = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(WorkspaceEdit, changes, documentChanges)

// Alias a std::optional to PartialResponse
// If it contains std::nullopt, we shouldn't send a result.
template<typename T>
using PartialResponse = std::optional<T>;

struct PartialResultParams
{
    std::optional<ProgressToken> partialResultToken = std::nullopt;
};
} // namespace lsp
