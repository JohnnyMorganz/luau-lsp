#pragma once

#include <optional>
#include <vector>

#include "nlohmann/json.hpp"

namespace lsp
{

enum struct SemanticTokenTypes
{
    Namespace,
    Type,
    Class,
    Enum,
    Interface,
    Struct,
    TypeParameter,
    Parameter,
    Variable,
    Property,
    EnumMember,
    Event,
    Function,
    Method,
    Macro,
    Keyword,
    Modifier,
    Comment,
    String,
    Number,
    RegExp,
    Operator,
    Decorator,
};
const SemanticTokenTypes SemanticTokenTypesList[] = {
    SemanticTokenTypes::Namespace,
    SemanticTokenTypes::Type,
    SemanticTokenTypes::Class,
    SemanticTokenTypes::Enum,
    SemanticTokenTypes::Interface,
    SemanticTokenTypes::Struct,
    SemanticTokenTypes::TypeParameter,
    SemanticTokenTypes::Parameter,
    SemanticTokenTypes::Variable,
    SemanticTokenTypes::Property,
    SemanticTokenTypes::EnumMember,
    SemanticTokenTypes::Event,
    SemanticTokenTypes::Function,
    SemanticTokenTypes::Method,
    SemanticTokenTypes::Macro,
    SemanticTokenTypes::Keyword,
    SemanticTokenTypes::Modifier,
    SemanticTokenTypes::Comment,
    SemanticTokenTypes::String,
    SemanticTokenTypes::Number,
    SemanticTokenTypes::RegExp,
    SemanticTokenTypes::Operator,
    SemanticTokenTypes::Decorator,
};
NLOHMANN_JSON_SERIALIZE_ENUM(SemanticTokenTypes, {
                                                     {SemanticTokenTypes::Namespace, "namespace"},
                                                     {SemanticTokenTypes::Type, "type"},
                                                     {SemanticTokenTypes::Class, "class"},
                                                     {SemanticTokenTypes::Enum, "enum"},
                                                     {SemanticTokenTypes::Interface, "interface"},
                                                     {SemanticTokenTypes::Struct, "struct"},
                                                     {SemanticTokenTypes::TypeParameter, "typeParameter"},
                                                     {SemanticTokenTypes::Parameter, "parameter"},
                                                     {SemanticTokenTypes::Variable, "variable"},
                                                     {SemanticTokenTypes::Property, "property"},
                                                     {SemanticTokenTypes::EnumMember, "enumMember"},
                                                     {SemanticTokenTypes::Event, "event"},
                                                     {SemanticTokenTypes::Function, "function"},
                                                     {SemanticTokenTypes::Method, "method"},
                                                     {SemanticTokenTypes::Macro, "macro"},
                                                     {SemanticTokenTypes::Keyword, "keyword"},
                                                     {SemanticTokenTypes::Modifier, "modifier"},
                                                     {SemanticTokenTypes::Comment, "comment"},
                                                     {SemanticTokenTypes::String, "string"},
                                                     {SemanticTokenTypes::Number, "number"},
                                                     {SemanticTokenTypes::RegExp, "regexp"},
                                                     {SemanticTokenTypes::Operator, "operator"},
                                                     {SemanticTokenTypes::Decorator, "decorator"},
                                                 });

enum struct SemanticTokenModifiers : uint16_t
{
    None = 0,
    Declaration = 1 << 0,
    Definition = 1 << 1,
    Readonly = 1 << 2,
    Static = 1 << 3,
    Deprecated = 1 << 4,
    Abstract = 1 << 5,
    Async = 1 << 6,
    Modification = 1 << 7,
    Documentation = 1 << 8,
    DefaultLibrary = 1 << 9,
};

const SemanticTokenModifiers SemanticTokenModifiersList[] = {
    SemanticTokenModifiers::Declaration,
    SemanticTokenModifiers::Definition,
    SemanticTokenModifiers::Readonly,
    SemanticTokenModifiers::Static,
    SemanticTokenModifiers::Deprecated,
    SemanticTokenModifiers::Abstract,
    SemanticTokenModifiers::Async,
    SemanticTokenModifiers::Modification,
    SemanticTokenModifiers::Documentation,
    SemanticTokenModifiers::DefaultLibrary,
};

NLOHMANN_JSON_SERIALIZE_ENUM(SemanticTokenModifiers, {
                                                         {SemanticTokenModifiers::Declaration, "declaration"},
                                                         {SemanticTokenModifiers::Definition, "definition"},
                                                         {SemanticTokenModifiers::Readonly, "readonly"},
                                                         {SemanticTokenModifiers::Static, "static"},
                                                         {SemanticTokenModifiers::Deprecated, "deprecated"},
                                                         {SemanticTokenModifiers::Abstract, "abstract"},
                                                         {SemanticTokenModifiers::Async, "async"},
                                                         {SemanticTokenModifiers::Modification, "modification"},
                                                         {SemanticTokenModifiers::Documentation, "documentation"},
                                                         {SemanticTokenModifiers::DefaultLibrary, "defaultLibrary"},
                                                     });

constexpr SemanticTokenModifiers operator|(SemanticTokenModifiers a, SemanticTokenModifiers b) noexcept
{
    return static_cast<SemanticTokenModifiers>(static_cast<int>(a) | static_cast<int>(b));
}

struct SemanticTokensLegend
{
    std::vector<SemanticTokenTypes> tokenTypes;
    std::vector<SemanticTokenModifiers> tokenModifiers;
};
NLOHMANN_DEFINE_OPTIONAL(SemanticTokensLegend, tokenTypes, tokenModifiers);

enum struct TokenFormat
{
    Relative,
};

NLOHMANN_JSON_SERIALIZE_ENUM(TokenFormat, {
                                              {TokenFormat::Relative, "relative"},
                                          });

struct SemanticTokensParams
{
    TextDocumentIdentifier textDocument;
};
NLOHMANN_DEFINE_OPTIONAL(SemanticTokensParams, textDocument);

struct SemanticTokens
{
    std::optional<std::string> resultId;
    std::vector<size_t> data;
};
NLOHMANN_DEFINE_OPTIONAL(SemanticTokens, resultId, data);
} // namespace lsp