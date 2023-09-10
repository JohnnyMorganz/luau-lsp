#pragma once
#include <optional>
#include <string>
#include <vector>
#include "Protocol/Structures.hpp"

namespace lsp
{

/**
 * Represents a parameter of a callable-signature. A parameter can
 * have a label and a doc-comment.
 */
struct ParameterInformation
{
    /**
     * The label of this parameter information.
     *
     * Either a string or an inclusive start and exclusive end offsets within
     * its containing signature label. (see SignatureInformation.label). The
     * offsets are based on a UTF-16 string representation as `Position` and
     * `Range` does.
     *
     * *Note*: a label of type string should be a substring of its containing
     * signature label. Its intended use case is to highlight the parameter
     * label part in the `SignatureInformation.label`.
     */
    std::variant<std::string, std::vector<size_t>> label = "";

    /**
     * The human-readable doc-comment of this parameter. Will be shown
     * in the UI but can be omitted.
     */
    std::optional<MarkupContent> documentation = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(ParameterInformation, label, documentation)

struct SignatureInformation
{
    std::string label;
    std::optional<MarkupContent> documentation = std::nullopt;
    std::optional<std::vector<ParameterInformation>> parameters = std::nullopt;
    std::optional<size_t> activeParameter = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(SignatureInformation, label, documentation, parameters, activeParameter)

struct SignatureHelp
{
    std::vector<SignatureInformation> signatures{};
    size_t activeSignature = 0;
    size_t activeParameter = 0;
};
NLOHMANN_DEFINE_OPTIONAL(SignatureHelp, signatures, activeSignature, activeParameter)

enum struct SignatureHelpTriggerKind
{
    Invoked = 1,
    TriggerCharacter = 2,
    ContentChange = 3,
};

struct SignatureHelpContext
{
    SignatureHelpTriggerKind triggerKind = SignatureHelpTriggerKind::Invoked;
    std::optional<std::string> triggerCharacter = std::nullopt;
    bool isRetrigger = false;
    std::optional<SignatureHelp> activeSignatureHelp = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(SignatureHelpContext, triggerKind, triggerCharacter, isRetrigger, activeSignatureHelp)

struct SignatureHelpParams : TextDocumentPositionParams
{
    std::optional<SignatureHelpContext> context = std::nullopt;
};
} // namespace lsp
