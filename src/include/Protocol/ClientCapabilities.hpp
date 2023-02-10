#pragma once

#include <optional>
#include <string>
#include <vector>

#include "nlohmann/json.hpp"
#include "Protocol/Completion.hpp"

namespace lsp
{
/**
 * Client capabilities specific to diagnostic pull requests.
 *
 * @since 3.17.0
 */
struct DiagnosticClientCapabilities
{
    /**
     * Whether implementation supports dynamic registration. If this is set to
     * `true` the client supports the new
     * `(TextDocumentRegistrationOptions & StaticRegistrationOptions)`
     * return value for the corresponding server capability as well.
     */
    bool dynamicRegistration = false;

    /**
     * Whether the clients supports related documents for document diagnostic
     * pulls.
     */
    bool relatedDocumentSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticClientCapabilities, dynamicRegistration, relatedDocumentSupport);


struct CompletionItemTagSupportClientCapabilities
{
    /**
     * The tags supported by the client.
     */
    std::vector<CompletionItemTag> valueSet{};
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItemTagSupportClientCapabilities, valueSet);

struct CompletionItemResolveSupportClientCapabilities
{
    /**
     * The properties that a client can resolve lazily.
     */
    std::vector<std::string> properties{};
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItemResolveSupportClientCapabilities, properties);

struct CompletionItemInsertTextModeSupportClientCapabilities
{
    std::vector<InsertTextMode> valueSet{};
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItemInsertTextModeSupportClientCapabilities, valueSet);

struct CompletionItemClientCapabilities
{
    /**
     * Client supports snippets as insert text.
     *
     * A snippet can define tab stops and placeholders with `$1`, `$2`
     * and `${3:foo}`. `$0` defines the final tab stop, it defaults to
     * the end of the snippet. Placeholders with equal identifiers are
     * linked, that is typing in one will update others too.
     */
    bool snippetSupport = false;

    /**
     * Client supports commit characters on a completion item.
     */
    bool commitCharactersSupport = false;

    /**
     * Client supports the follow content formats for the documentation
     * property. The order describes the preferred format of the client.
     */
    std::vector<MarkupKind> documentationFormat{};

    /**
     * Client supports the deprecated property on a completion item.
     */
    bool deprecatedSupport = false;

    /**
     * Client supports the preselect property on a completion item.
     */
    bool preselectSupport = false;

    /**
     * Client supports the tag property on a completion item. Clients
     * supporting tags have to handle unknown tags gracefully. Clients
     * especially need to preserve unknown tags when sending a completion
     * item back to the server in a resolve call.
     *
     * @since 3.15.0
     */
    std::optional<CompletionItemTagSupportClientCapabilities> tagSupport = std::nullopt;

    /**
     * Client supports insert replace edit to control different behavior if
     * a completion item is inserted in the text or should replace text.
     *
     * @since 3.16.0
     */
    bool insertReplaceSupport = false;

    /**
     * Indicates which properties a client can resolve lazily on a
     * completion item. Before version 3.16.0 only the predefined properties
     * `documentation` and `detail` could be resolved lazily.
     *
     * @since 3.16.0
     */
    std::optional<CompletionItemResolveSupportClientCapabilities> resolveSupport = std::nullopt;

    /**
     * The client supports the `insertTextMode` property on
     * a completion item to override the whitespace handling mode
     * as defined by the client (see `insertTextMode`).
     *
     * @since 3.16.0
     */
    std::optional<CompletionItemInsertTextModeSupportClientCapabilities> insertTextModeSupport = std::nullopt;

    /**
     * The client has support for completion item label
     * details (see also `CompletionItemLabelDetails`).
     *
     * @since 3.17.0
     */
    bool labelDetailsSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItemClientCapabilities, snippetSupport, commitCharactersSupport, documentationFormat, deprecatedSupport,
    preselectSupport, tagSupport, insertReplaceSupport, resolveSupport, insertTextModeSupport, labelDetailsSupport);

struct CompletionItemKindClientCapabilities
{
    /**
     * The completion item kind values the client supports. When this
     * property exists the client also guarantees that it will
     * handle values outside its set gracefully and falls back
     * to a default value when unknown.
     *
     * If this property is not present the client only supports
     * the completion items kinds from `Text` to `Reference` as defined in
     * the initial version of the protocol.
     */
    std::vector<CompletionItemKind> valueSet{};
};
NLOHMANN_DEFINE_OPTIONAL(CompletionItemKindClientCapabilities, valueSet);

struct CompletionListClientCapabilities
{
    /**
     * The client supports the following itemDefaults on
     * a completion list.
     *
     * The value lists the supported property names of the
     * `CompletionList.itemDefaults` object. If omitted
     * no properties are supported.
     *
     * @since 3.17.0
     */
    std::vector<std::string> itemDefaults{};
};
NLOHMANN_DEFINE_OPTIONAL(CompletionListClientCapabilities, itemDefaults);

struct CompletionClientCapabilities
{
    /**
     * Whether completion supports dynamic registration.
     */
    bool dynamicRegistration = false;

    /**
     * The client supports the following `CompletionItem` specific
     * capabilities.
     */
    std::optional<CompletionItemClientCapabilities> completionItem = std::nullopt;
    std::optional<CompletionItemKindClientCapabilities> completionItemKind = std::nullopt;

    /**
     * The client supports to send additional context information for a
     * `textDocument/completion` request.
     */
    bool contextSupport = false;

    /**
     * The client's default when the completion item doesn't provide a
     * `insertTextMode` property.
     *
     * @since 3.17.0
     */
    InsertTextMode insertTextMode = InsertTextMode::AsIs;

    /**
     * The client supports the following `CompletionList` specific
     * capabilities.
     *
     * @since 3.17.0
     */
    std::optional<CompletionListClientCapabilities> completionList = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(
    CompletionClientCapabilities, dynamicRegistration, completionItem, completionItemKind, contextSupport, insertTextMode, completionList);

struct TextDocumentClientCapabilities
{
    /**
     * Capabilities specific to the `textDocument/completion` request.
     */
    std::optional<CompletionClientCapabilities> completion = std::nullopt;

    std::optional<DiagnosticClientCapabilities> diagnostic = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(TextDocumentClientCapabilities, completion, diagnostic);

struct DidChangeConfigurationClientCapabilities
{
    /**
     * Did change configuration notification supports dynamic registration.
     */
    bool dynamicRegistration = false;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeConfigurationClientCapabilities, dynamicRegistration);

struct DidChangeWatchedFilesClientCapabilities
{
    /**
     * Did change watched files notification supports dynamic registration.
     * Please note that the current protocol doesn't support static
     * configuration for file changes from the server side.
     */
    bool dynamicRegistration = false;

    /**
     * Whether the client has support for relative patterns
     * or not.
     *
     * @since 3.17.0
     */
    bool relativePatternSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(DidChangeWatchedFilesClientCapabilities, dynamicRegistration, relativePatternSupport);


/**
 * Client workspace capabilities specific to inlay hints.
 *
 * @since 3.17.0
 */
struct InlayHintWorkspaceClientCapabilities
{
    /**
     * Whether the client implementation supports a refresh request sent from
     * the server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * inlay hints currently shown. It should be used with absolute care and
     * is useful for situation where a server for example detects a project wide
     * change that requires such a calculation.
     */
    bool refreshSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(InlayHintWorkspaceClientCapabilities, refreshSupport);

struct DiagnosticWorkspaceClientCapabilities
{
    /**
     * Whether the client implementation supports a refresh request sent from
     * the server to the client.
     *
     * Note that this event is global and will force the client to refresh all
     * pulled diagnostics currently shown. It should be used with absolute care
     * and is useful for situation where a server for example detects a project
     * wide change that requires such a calculation.
     */
    bool refreshSupport = false;
};
NLOHMANN_DEFINE_OPTIONAL(DiagnosticWorkspaceClientCapabilities, refreshSupport);

struct ClientWorkspaceCapabilities
{
    /**
     * Capabilities specific to the `workspace/didChangeConfiguration`
     * notification.
     */
    std::optional<DidChangeConfigurationClientCapabilities> didChangeConfiguration = std::nullopt;

    /**
     * Capabilities specific to the `workspace/didChangeWatchedFiles`
     * notification.
     */
    std::optional<DidChangeWatchedFilesClientCapabilities> didChangeWatchedFiles = std::nullopt;

    /**
     * The client supports `workspace/configuration` requests.
     *
     * @since 3.6.0
     */
    bool configuration = false;

    /**
     * Client workspace capabilities specific to inlay hints.
     *
     * @since 3.17.0
     */
    std::optional<InlayHintWorkspaceClientCapabilities> inlayHint = std::nullopt;

    /**
     * Client workspace capabilities specific to diagnostics.
     *
     * @since 3.17.0.
     */
    std::optional<DiagnosticWorkspaceClientCapabilities> diagnostics = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(ClientWorkspaceCapabilities, didChangeConfiguration, didChangeWatchedFiles, configuration, inlayHint, diagnostics);

struct ClientGeneralCapabilities
{
    /**
     * The position encodings supported by the client. Client and server
     * have to agree on the same position encoding to ensure that offsets
     * (e.g. character position in a line) are interpreted the same on both
     * side.
     *
     * To keep the protocol backwards compatible the following applies: if
     * the value 'utf-16' is missing from the array of position encodings
     * servers can assume that the client supports UTF-16. UTF-16 is
     * therefore a mandatory encoding.
     *
     * If omitted it defaults to ['utf-16'].
     *
     * Implementation considerations: since the conversion from one encoding
     * into another requires the content of the file / line the conversion
     * is best done where the file is read which is usually on the server
     * side.
     *
     * @since 3.17.0
     */
    std::optional<std::vector<PositionEncodingKind>> positionEncodings = std::nullopt;
};
NLOHMANN_DEFINE_OPTIONAL(ClientGeneralCapabilities, positionEncodings);

struct ClientCapabilities
{
    /**
     * Text document specific client capabilities.
     */
    std::optional<TextDocumentClientCapabilities> textDocument = std::nullopt;

    /**
     * Workspace specific client capabilities.
     */
    std::optional<ClientWorkspaceCapabilities> workspace = std::nullopt;

    /**
     * General client capabilities.
     *
     * @since 3.16.0
     */
    std::optional<ClientGeneralCapabilities> general = std::nullopt;

    // TODO
    // notebook
    // window
};
NLOHMANN_DEFINE_OPTIONAL(ClientCapabilities, textDocument, workspace, general);
} // namespace lsp
